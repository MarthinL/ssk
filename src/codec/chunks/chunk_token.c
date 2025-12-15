/*
 * src/codec/chunks/chunk_token.c
 *
 * Token encoding/decoding dispatcher and RAW coalescing logic
 *
 * A MIX segment consists of a sequence of tokens. Each token represents
 * one or more chunks of the abvector. Token types:
 *   - ENUM (00): Sparse chunk, k <= K_ENUM_MAX, stored as combinadic rank
 *   - RAW (01): Dense chunk, k > K_ENUM_MAX, stored as raw bits
 *   - RAW_RUN (10): Coalesced consecutive RAW chunks (required by canon)
 *   - Reserved (11): Invalid, must reject
 *
 * CANON REQUIREMENT: Consecutive RAW chunks MUST be coalesced into RAW_RUN.
 * Encoder must never emit two RAW tokens in a row. Decoder must reject if
 * it sees RAW followed by RAW.
 *
 * Token boundary alignment: Tokens are NOT byte-aligned; they flow as a
 * continuous bit stream within the segment.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ssk.h"
#include "ssk_format.h"
#include "bitblocks.h"

/* ============================================================================
 * EXTERNAL FUNCTION DECLARATIONS
 * (These are implemented in chunk_enum.c and chunk_raw.c)
 * ============================================================================
 */

/* From chunk_enum.c */
extern size_t enum_token_bits(uint8_t n, uint8_t k);
extern size_t enum_token_encode(uint64_t bits, uint8_t n, uint8_t k,
                                uint8_t *buf, size_t bit_pos);
extern int enum_token_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                             uint8_t n, uint64_t *bits_out, uint8_t *k_out,
                             size_t *bits_read);
extern bool should_use_enum(uint8_t k);

/* From chunk_raw.c */
extern size_t raw_token_bits(uint8_t n);
extern size_t raw_token_encode(uint64_t bits, uint8_t n, uint8_t *buf, size_t bit_pos);
extern int raw_token_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                            uint8_t n, uint64_t *bits_out, size_t *bits_read);
extern size_t raw_run_token_bits(uint16_t run_len, uint8_t chunk_bits, uint8_t final_nbits);
extern size_t raw_run_header_encode(uint16_t run_len, uint8_t *buf, size_t bit_pos);
extern int raw_run_header_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                                 uint16_t *run_len_out, size_t *bits_read);

/* ============================================================================
 * SINGLE TOKEN ENCODING
 * ============================================================================
 */

/**
 * Encode a single chunk as appropriate token type.
 * Does NOT handle RAW_RUN coalescing - caller must accumulate RAW chunks.
 *
 * @param bits            Chunk bit pattern (right-aligned)
 * @param n               Chunk size in bits (1-64)
 * @param k               Number of set bits (popcount)
 * @param buf             Output buffer
 * @param bit_pos         Starting bit position
 * @param token_type_out  Output: token type used (TOK_ENUM or TOK_RAW)
 * @return Bits written, or 0 on error
 *
 * As per Spec Format 0: Decides between ENUM (if k <= SSK_K_CHUNK_ENUM_MAX)
 *                       and RAW (otherwise), delegates to appropriate encoder.
 */
size_t
token_encode_single(uint64_t bits, uint8_t n, uint8_t k,
                    uint8_t *buf, size_t bit_pos,
                    TokenKind *token_type_out)
{
    if (should_use_enum(k))
    {
        *token_type_out = TOK_ENUM;
        return enum_token_encode(bits, n, k, buf, bit_pos);
    }
    else
    {
        *token_type_out = TOK_RAW;
        return raw_token_encode(bits, n, buf, bit_pos);
    }
}

/* ============================================================================
 * TOKEN STREAM DECODER
 * ============================================================================
 */

/**
 * Decode a single token from the stream.
 *
 * @param buf           Input buffer
 * @param bit_pos       Starting bit position
 * @param buf_bits      Total bits available
 * @param chunk_bits    Default chunk size (for RAW)
 * @param token_out     Output: decoded token info
 * @param bits_read     Output: total bits consumed
 * @param prev_was_raw  Input: was the previous token RAW? (for canon check)
 * @return 0 on success, -1 on error, -2 on canon violation
 *
 * For RAW_RUN, this only decodes the header. Caller must handle
 * reading the raw data bits.
 *
 * As per Spec Format 0: Validates token type is not RESERVED,
 *                       checks RAW coalescing (no consecutive RAW).
 */
int
token_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
             uint8_t chunk_bits, SSKToken *token_out, size_t *bits_read,
             bool prev_was_raw)
{
    size_t start_pos = bit_pos;
    
    /* 1. Read token type: 2 bits */
    if (bit_pos + 2 > buf_bits)
        return -1;  /* Truncated */
    
    uint8_t token_type = (uint8_t)bb_read_bits(buf, bit_pos, 2);
    bit_pos += 2;
    
    /* 2. Validate token type is not reserved */
    if (token_type == TOK_RESERVED)
        return -1;  /* Reserved token type invalid */
    
    /* 3. Canon check: RAW cannot follow RAW (as per Spec Format 0) */
    if (token_type == TOK_RAW && prev_was_raw)
        return -2;  /* Canon violation: uncoalesced consecutive RAW */
    
    token_out->kind = token_type;
    token_out->dirty = 0;
    
    /* 4. Decode token based on its type */
    switch (token_type)
    {
        case TOK_ENUM:
        {
            uint64_t chunk_bits_out;
            uint8_t k;
            size_t enum_read;
            
            int rc = enum_token_decode(buf, bit_pos, buf_bits, chunk_bits,
                                       &chunk_bits_out, &k, &enum_read);
            if (rc != 0)
                return -1;
            
            bit_pos += enum_read;
            token_out->popcount = k;
            /* token_out->data_off set by caller */
            break;
        }
        
        case TOK_RAW:
        {
            uint64_t chunk_bits_out;
            size_t raw_read;
            
            int rc = raw_token_decode(buf, bit_pos, buf_bits, chunk_bits,
                                      &chunk_bits_out, &raw_read);
            if (rc != 0)
                return -1;
            
            bit_pos += raw_read;
            token_out->popcount = ssk_popcount64(chunk_bits_out);
            break;
        }
        
        case TOK_RAW_RUN:
        {
            uint16_t run_len;
            size_t header_read;
            
            int rc = raw_run_header_decode(buf, bit_pos, buf_bits,
                                           &run_len, &header_read);
            if (rc != 0)
                return -1;
            
            bit_pos += header_read;
            /* Caller must read run_len * chunk_bits of raw data */
            /* popcount must be computed from data by caller */
            token_out->popcount = 0;  /* To be filled by caller */
            break;
        }
        
        default:
            return -1;  /* Should never reach */
    }
    
    *bits_read = bit_pos - start_pos;
    return 0;
}

/* ============================================================================
 * TOKEN TYPE PREDICATES
 * ============================================================================
 */

/**
 * Check if a token type is valid.
 */
bool
token_type_valid(uint8_t token_type)
{
    return token_type <= TOK_RAW_RUN;
}

/**
 * Get human-readable name for token type.
 */
const char *
token_type_name(TokenKind kind)
{
    switch (kind)
    {
        case TOK_ENUM:    return "ENUM";
        case TOK_RAW:     return "RAW";
        case TOK_RAW_RUN: return "RAW_RUN";
        default:          return "RESERVED";
    }
}