/*
 * src/codec/vlq_p.c
 *
 * VLQ-P (Variable Length Quantity - Parameterized) encoder/decoder
 *
 * VLQ-P encodes integers using a parameterized multi-stage scheme where each
 * stage contributes a fixed number of bits. A continuation bit indicates
 * whether more stages follow.
 *
 * Example: stage_bits = {3, 5, 8}
 *   Stage 0: 3 data bits, values 0-7 fit without continuation
 *   Stage 1: 3 + 5 = 8 cumulative data bits, continuation bit between
 *   Stage 2: 3 + 5 + 8 = 16 cumulative data bits, no continuation (final)
 *
 * Encoding is canonical (minimal): a value MUST use the fewest stages possible.
 * Decoders MUST reject non-minimal encodings.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef USE_PG
#include "postgres.h"
#include "fmgr.h"
#else
#include <stdlib.h>
#endif

#include "ssk_format.h"
#include "bitstream.h"

/* ============================================================================
 * VLQ-P SUBTYPE DEFINITIONS
 *
 * These are initial educated guesses. Tune after profiling real workloads.
 * Each subtype is optimized for a different expected value distribution.
 * ============================================================================
 */

/* DEFAULT: For format version field. Must encode small values efficiently. */
const VLQPParams VLQP_DEFAULT = {
    .domain_size = 255,
    .n_stages = 2,
    .stage_bits = {4, 4}   /* 0-15 in 4 bits, 16-255 in 8 bits */
};

/* SMALL_INT: Expected mostly 0-10, occasional 100s (segment_count, raw_run_len) */
const VLQPParams VLQP_SMALL_INT = {
    .domain_size = 65535,
    .n_stages = 3,
    .stage_bits = {4, 6, 6}   /* 0-15, 16-1023, 1024-65535 */
};

/* MEDIUM_INT: Expected mostly 64-2048, occasional 10Ks (length_bits, popcount) */
const VLQPParams VLQP_MEDIUM_INT = {
    .domain_size = 1048575,
    .n_stages = 3,
    .stage_bits = {6, 7, 7}   /* 0-63, 64-8191, 8192-1M */
};

/* LARGE_INT: Wide spread 0-2^32, clustered (partition_delta, initial_delta, enum_combined) */
const VLQPParams VLQP_LARGE_INT = {
    .domain_size = UINT32_MAX,
    .n_stages = 4,
    .stage_bits = {5, 7, 10, 10}   /* 0-31, 32-4K, 4K-4M, 4M-4G */
};

/* HUGE_INT: Rare but must handle 2^64 (future-proofing) */
const VLQPParams VLQP_HUGE_INT = {
    .domain_size = UINT64_MAX,
    .n_stages = 5,
    .stage_bits = {6, 8, 10, 16, 24}   /* Progressive stages up to 64 bits */
};

/* ============================================================================
 * VLQ-P ENCODING
 * ============================================================================
 */

/**
 * Compute the minimum number of stages needed to encode a value.
 *
 * @param value   Value to encode
 * @param params  VLQ-P parameters
 * @return Number of stages needed (1 to n_stages), or 0 if value exceeds domain
 */
static uint8_t
vlqp_stages_needed(uint64_t value, const VLQPParams *params)
{
    uint64_t max_value = 0;
    
    for (uint8_t stage = 0; stage < params->n_stages; stage++)
    {
        /* Cumulative bits for this stage */
        max_value = (max_value << params->stage_bits[stage]) | 
                    ((1ULL << params->stage_bits[stage]) - 1);
        
        if (value <= max_value)
            return stage + 1;
    }
    
    return 0;  /* Value too large for this VLQ-P subtype */
}

/**
 * Compute total bits needed to encode a value with VLQ-P.
 *
 * @param value   Value to encode
 * @param params  VLQ-P parameters
 * @return Total bits (data + continuation), or 0 if value exceeds domain
 */
size_t
vlqp_encoded_bits(uint64_t value, const VLQPParams *params)
{
    uint8_t stages = vlqp_stages_needed(value, params);
    if (stages == 0)
        return 0;
    
    size_t total_bits = 0;
    for (uint8_t i = 0; i < stages; i++)
    {
        total_bits += params->stage_bits[i];
        /* Continuation bit after each stage except the final possible stage */
        if (i < params->n_stages - 1)
            total_bits += 1;
    }
    
    return total_bits;
}

/**
 * Encode a value using VLQ-P.
 *
 * @param value     Value to encode
 * @param params    VLQ-P parameters
 * @param buf       Output buffer (must have space for encoded bits)
 * @param bit_pos   Starting bit position in buffer
 * @return Number of bits written, or 0 on error
 */
size_t
vlqp_encode(uint64_t value, const VLQPParams *params, uint8_t *buf, size_t bit_pos)
{
    uint8_t stages = vlqp_stages_needed(value, params);
    if (stages == 0)
        return 0;  /* Value exceeds domain */
    
    size_t start_pos = bit_pos;
    
    /* 
     * Encoding strategy:
     * - Extract bits for each stage from value, MSB-aligned per stage
     * - Write data bits, then continuation bit (if not final stage)
     *
     * Example: value=100, stage_bits={3,5,8}
     *   Stage 0 can hold 0-7, not enough -> continuation=1, write 3 bits of partial
     *   Stage 1 can hold remaining -> continuation=0 (implicit), write 5 bits
     *
     * Actually simpler: compute cumulative data bits, extract from value directly.
     */
    
    /* Compute total data bits for required stages */
    uint8_t total_data_bits = 0;
    for (uint8_t i = 0; i < stages; i++)
        total_data_bits += params->stage_bits[i];
    
    /* Extract bits from value for each stage, MSB-first overall */
    uint8_t remaining_data_bits = total_data_bits;
    
    for (uint8_t stage = 0; stage < stages; stage++)
    {
        uint8_t stage_bits = params->stage_bits[stage];
        remaining_data_bits -= stage_bits;
        
        /* Extract this stage's portion of the value */
        uint64_t stage_value = (value >> remaining_data_bits) & ((1ULL << stage_bits) - 1);
        
        /* Write data bits */
        bs_write_bits(buf, bit_pos, stage_value, stage_bits);
        bit_pos += stage_bits;
        
        /* 
         * Write continuation bit if not at final possible stage.
         * cont=1 means more stages follow, cont=0 means this is the last stage.
         * The final possible stage (n_stages - 1) has no continuation bit.
         */
        if (stage < params->n_stages - 1)
        {
            uint8_t cont = (stage < stages - 1) ? 1 : 0;
            bs_write_bits(buf, bit_pos, cont, 1);
            bit_pos++;
        }
        /* No continuation bit after final possible stage */
    }
    
    return bit_pos - start_pos;
}

/* ============================================================================
 * VLQ-P DECODING
 * ============================================================================
 */

/**
 * Decode a VLQ-P encoded value.
 *
 * @param buf        Input buffer
 * @param bit_pos    Starting bit position
 * @param buf_bits   Total bits available in buffer (for bounds checking)
 * @param params     VLQ-P parameters
 * @param value_out  Output: decoded value
 * @param bits_read  Output: number of bits consumed
 * @return 0 on success, -1 on error (truncated, invalid)
 */
int
vlqp_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
            const VLQPParams *params, uint64_t *value_out, size_t *bits_read)
{
    uint64_t value = 0;
    size_t start_pos = bit_pos;
    
    for (uint8_t stage = 0; stage < params->n_stages; stage++)
    {
        uint8_t stage_bits = params->stage_bits[stage];
        
        /* Bounds check */
        if (bit_pos + stage_bits > buf_bits)
            return -1;  /* Truncated */
        
        /* Read data bits for this stage */
        uint64_t stage_value = bs_read_bits(buf, bit_pos, stage_bits);
        bit_pos += stage_bits;
        
        /* Accumulate into value */
        value = (value << stage_bits) | stage_value;
        
        /* Check for continuation bit (not present after final stage) */
        if (stage < params->n_stages - 1)
        {
            if (bit_pos >= buf_bits)
                return -1;  /* Truncated - expected continuation bit */
            
            uint8_t cont = bs_read_bits(buf, bit_pos, 1);
            bit_pos++;
            
            if (cont == 0)
            {
                /* No more stages - we're done */
                *value_out = value;
                *bits_read = bit_pos - start_pos;
                return 0;
            }
            /* cont == 1: more stages follow, continue loop */
        }
        else
        {
            /* Final stage reached (no continuation bit) */
            *value_out = value;
            *bits_read = bit_pos - start_pos;
            return 0;
        }
    }
    
    /* Should not reach here */
    return -1;
}

/* ============================================================================
 * VLQ-P MINIMALITY CHECK
 *
 * Canon requires minimal encoding: value MUST use fewest stages possible.
 * A non-minimal encoding is one where the value could fit in fewer stages.
 * ============================================================================
 */

/**
 * Check if a VLQ-P encoded value uses minimal encoding.
 *
 * @param buf        Input buffer
 * @param bit_pos    Starting bit position
 * @param buf_bits   Total bits available
 * @param params     VLQ-P parameters
 * @return true if minimal, false if non-minimal or invalid
 */
bool
vlqp_is_minimal(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                const VLQPParams *params)
{
    uint64_t value;
    size_t bits_read;
    
    /* First, decode the value */
    if (vlqp_decode(buf, bit_pos, buf_bits, params, &value, &bits_read) != 0)
        return false;  /* Invalid encoding */
    
    /* Count how many stages were actually used */
    uint8_t stages_used = 0;
    size_t pos = bit_pos;
    
    for (uint8_t stage = 0; stage < params->n_stages; stage++)
    {
        pos += params->stage_bits[stage];
        stages_used++;
        
        if (stage < params->n_stages - 1)
        {
            uint8_t cont = bs_read_bits(buf, pos, 1);
            pos++;
            if (cont == 0)
                break;  /* No more stages */
        }
    }
    
    /* Check if value could fit in fewer stages */
    uint8_t min_stages = vlqp_stages_needed(value, params);
    
    return (stages_used == min_stages);
}

/**
 * Compute maximum value encodable in given number of stages.
 *
 * @param params   VLQ-P parameters
 * @param stages   Number of stages (1 to n_stages)
 * @return Maximum value that fits in that many stages
 */
uint64_t
vlqp_max_value(const VLQPParams *params, uint8_t stages)
{
    if (stages == 0 || stages > params->n_stages)
        return 0;
    
    uint64_t max_value = 0;
    for (uint8_t i = 0; i < stages; i++)
    {
        max_value = (max_value << params->stage_bits[i]) |
                    ((1ULL << params->stage_bits[i]) - 1);
    }
    
    return max_value;
}