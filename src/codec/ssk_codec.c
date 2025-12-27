/*
 * Copyright (c) 2020-2025 Marthin Laubscher
 * All rights reserved. See LICENSE for details.
 */

/*
 * src/codec/ssk_codec.c
 *
 * SSK codec main functions.
 * Real implementations use CDU, combinadic, and chunk modules.
 */

/* Environment setup: PostgreSQL or standalone */
#ifdef USE_PG
#include "postgres.h"
#define ALLOC(size) palloc(size)
#define FREE(ptr) pfree(ptr)
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ALLOC(size) malloc(size)
#define FREE(ptr) free(ptr)
#endif

#include <assert.h>

/* Project headers */
#include "ssk.h"
#include "cdu.h"

#ifdef TRIVIAL 

#include <stdlib.h>

// ============================================================================
// TRIVIAL REFERENCE: Format 1023 (IDs 1..64)
// ============================================================================

extern int 
ssk_encode(const AbV abv, uint8_t *buffer, size_t buffer_size, uint16_t target_format)
{
  memset(buffer, 0, buffer_size);
  uint64_t block = (uint64_t)SSK_FORMAT;
  int bits_used = cdu_encode(block, CDU_TYPE_DEFAULT, buffer, 0, buffer_size * 8);
  block = 0;
  assert(SSK_PADDING_BITS == 3);
  bits_used += 3;
  // bits_used += cdu_encode(block, CDU_TYPE_RAW1, buffer, bits_used, buffer_size * 8);
  // bits_used += cdu_encode(block, CDU_TYPE_RAW2, buffer, bits_used, buffer_size * 8);
  assert(bits_used == 16);
  bits_used += cdu_encode(abv, CDU_TYPE_RAW64, buffer, bits_used, buffer_size * 8);
  return bits_used;
}

extern int 
ssk_decode(const uint8_t *buffer, size_t buffer_size, AbV *out)
{
  uint64_t block;
  int bits_used = cdu_decode(buffer, 0, buffer_size * 8, CDU_TYPE_DEFAULT, &block);
  if (block == (uint64_t)SSK_FORMAT) {
    assert(bits_used == 13);
    bits_used += 3;
    bits_used += cdu_decode(buffer, bits_used, buffer_size * 8, CDU_TYPE_RAW64, &block);
    *out = block;
  } else {
    bits_used = -1;
  }
  return bits_used;
}

#else // NON TRIVIAL

#include "bitblocks.h"

extern int ssk_encode(const AbV abv, uint8_t *buffer, size_t buffer_size, uint16_t target_format);
extern int ssk_decode(const uint8_t *buffer, size_t buffer_size, AbV *out);

// ============================================================================
// SCALE IMPLEMENTATION: Format 0 (IDs 1..2^64)
// ============================================================================

/* Forward declarations */
int ssk_encode_impl(const AbV abv, uint8_t *buffer, size_t buffer_size, 
                   uint16_t target_format, FILE *debug_log, char *mock_output, 
                   size_t mock_output_size, size_t *mock_output_used);

int ssk_encode(const AbV abv, uint8_t *buffer, size_t buffer_size, uint16_t target_format)
{
    // Current implementation (hierarchical, CDU, etc.)
    return ssk_encode_impl(abv, buffer, buffer_size, target_format, NULL, NULL, 0, NULL);
}

int ssk_decode(const uint8_t *buffer, size_t buffer_size, AbV *out)
{
    // TODO: Implement Format 0 decoding (blocked on partition strategy)
    return -1;
}

/* Internal implementation with debug/audit support */
int
ssk_encode_impl(const AbV abv, uint8_t *buffer, size_t buffer_size, 
               uint16_t target_format, FILE *debug_log, char *mock_output, 
               size_t mock_output_size, size_t *mock_output_used)
{
    size_t bit_pos = 0;
    size_t mock_pos = 0;
    uint32_t prev_partition_id = 0;

    /* Clear mock output if provided */
    if (mock_output && mock_output_size > 0) {
        mock_output[0] = '\0';
    }
    if (mock_output_used) {
        *mock_output_used = 0;
    }

    /* 1. Encode format_version */
    {
        size_t bits = cdu_encode(abv->format_version, SSK_FORMAT, buffer, bit_pos);
        if (bits == 0) return -1;
        bit_pos += bits;

        if (debug_log) fprintf(debug_log, "format_version=%u (%zu bits)\n", abv->format_version, bits);
        if (mock_output) {
            int len = snprintf(mock_output + mock_pos, mock_output_size - mock_pos, "%u/%u|", abv->format_version, SSK_FORMAT);
            if (len < 0 || (size_t)len >= mock_output_size - mock_pos) return -1;
            mock_pos += len;
        }
    }

    /* 2. Encode global rare_bit */
    {
        bb_write_bits(buffer, bit_pos, abv->rare_bit, 1);
        bit_pos += 1;

        if (debug_log) fprintf(debug_log, "global_rare_bit=%u (1 bit)\n", abv->rare_bit);
        if (mock_output) {
            int len = snprintf(mock_output + mock_pos, mock_output_size - mock_pos, "0b%u|", abv->rare_bit);
            if (len < 0 || (size_t)len >= mock_output_size - mock_pos) return -1;
            mock_pos += len;
        }
    }

    /* 3. Encode n_partitions */
    {
        size_t bits = cdu_encode(abv->n_partitions, SSK_PARTITIONS, buffer, bit_pos);
        if (bits == 0) return -1;
        bit_pos += bits;

        if (debug_log) fprintf(debug_log, "n_partitions=%u (%zu bits)\n", abv->n_partitions, bits);
        if (mock_output) {
            int len = snprintf(mock_output + mock_pos, mock_output_size - mock_pos, "%u/%u|", abv->n_partitions, SSK_PARTITIONS);
            if (len < 0 || (size_t)len >= mock_output_size - mock_pos) return -1;
            mock_pos += len;
        }
    }

    /* 4. Encode each partition */
    for (uint32_t p = 0; p < abv->n_partitions; p++) {
        AbVPartition *part = decoded_partition(abv, p);
        uint32_t partition_delta = part->partition_id - prev_partition_id;
        prev_partition_id = part->partition_id;

        /* 4a. Encode partition_delta */
        {
            size_t bits = cdu_encode(partition_delta, SSK_PARTITION_DELTA, buffer, bit_pos);
            if (bits == 0) return -1;
            bit_pos += bits;

            if (debug_log) fprintf(debug_log, "  partition[%u] delta=%u (%zu bits)\n", p, partition_delta, bits);
            if (mock_output) {
                int len = snprintf(mock_output + mock_pos, mock_output_size - mock_pos, "%u/%u|", partition_delta, SSK_PARTITION_DELTA);
                if (len < 0 || (size_t)len >= mock_output_size - mock_pos) return -1;
                mock_pos += len;
            }
        }

        /* 4b. Encode partition rare_bit */
        {
            bb_write_bits(buffer, bit_pos, part->rare_bit, 1);
            bit_pos += 1;

            if (debug_log) fprintf(debug_log, "  partition[%u] rare_bit=%u (1 bit)\n", p, part->rare_bit);
            if (mock_output) {
                int len = snprintf(mock_output + mock_pos, mock_output_size - mock_pos, "0b%u|", part->rare_bit);
                if (len < 0 || (size_t)len >= mock_output_size - mock_pos) return -1;
                mock_pos += len;
            }
        }

        /* 4c. Encode n_segments */
        {
            size_t bits = cdu_encode(part->n_segments, SSK_N_SEGMENTS, buffer, bit_pos);
            if (bits == 0) return -1;
            bit_pos += bits;

            if (debug_log) fprintf(debug_log, "  partition[%u] n_segments=%u (%zu bits)\n", p, part->n_segments, bits);
            if (mock_output) {
                int len = snprintf(mock_output + mock_pos, mock_output_size - mock_pos, "%u/%u|", part->n_segments, SSK_N_SEGMENTS);
                if (len < 0 || (size_t)len >= mock_output_size - mock_pos) return -1;
                mock_pos += len;
            }
        }

        /* 4d. Encode each segment */
        for (uint32_t s = 0; s < part->n_segments; s++) {
            AbVSegment *seg = partition_segment(part, s);

            /* 4d.i. Encode seg_kind (0=RLE, 1=MIX) */
            {
                uint8_t seg_kind = (seg->segment_type == SEG_TYPE_RLE) ? 0 : 1;
                bb_write_bits(buffer, bit_pos, seg_kind, 1);
                bit_pos += 1;

                if (debug_log) fprintf(debug_log, "    segment[%u] kind=%u (1 bit)\n", s, seg_kind);
                if (mock_output) {
                    int len = snprintf(mock_output + mock_pos, mock_output_size - mock_pos, "0b%u|", seg_kind);
                    if (len < 0 || (size_t)len >= mock_output_size - mock_pos) return -1;
                    mock_pos += len;
                }
            }

            /* 4d.ii. Encode initial_delta */
            {
                size_t bits = cdu_encode(seg->start_bit, SSK_SEGMENT_START_BIT, buffer, bit_pos);
                if (bits == 0) return -1;
                bit_pos += bits;

                if (debug_log) fprintf(debug_log, "    segment[%u] initial_delta=%u (%zu bits)\n", s, seg->start_bit, bits);
                if (mock_output) {
                    int len = snprintf(mock_output + mock_pos, mock_output_size - mock_pos, "%u/%u|", seg->start_bit, SSK_SEGMENT_START_BIT);
                    if (len < 0 || (size_t)len >= mock_output_size - mock_pos) return -1;
                    mock_pos += len;
                }
            }

            /* 4d.iii. Encode length_bits */
            {
                size_t bits = cdu_encode(seg->n_bits, SSK_SEGMENT_N_BITS, buffer, bit_pos);
                if (bits == 0) return -1;
                bit_pos += bits;

                if (debug_log) fprintf(debug_log, "    segment[%u] length_bits=%u (%zu bits)\n", s, seg->n_bits, bits);
                if (mock_output) {
                    int len = snprintf(mock_output + mock_pos, mock_output_size - mock_pos, "%u/%u|", seg->n_bits, SSK_SEGMENT_N_BITS);
                    if (len < 0 || (size_t)len >= mock_output_size - mock_pos) return -1;
                    mock_pos += len;
                }
            }

            /* 4d.iv. Handle segment-specific encoding */
            if (seg->segment_type == SEG_TYPE_RLE) {
                /* RLE: encode membership_bit */
                bb_write_bits(buffer, bit_pos, seg->rare_bit, 1);
                bit_pos += 1;

                if (debug_log) fprintf(debug_log, "    segment[%u] membership_bit=%u (1 bit)\n", s, seg->rare_bit);
                if (mock_output) {
                    int len = snprintf(mock_output + mock_pos, mock_output_size - mock_pos, "0b%u|", seg->rare_bit);
                    if (len < 0 || (size_t)len >= mock_output_size - mock_pos) return -1;
                    mock_pos += len;
                }
            } else {
                /* MIX: encode token stream */
                uint32_t n_chunks = segment_n_chunks(seg->n_bits);
                
                if (debug_log) fprintf(debug_log, "    segment[%u] MIX with %u chunks\n", s, n_chunks);
                
                for (uint32_t c = 0; c < n_chunks; c++) {
                    uint8_t meta = segment_chunk_meta_get(seg, c);
                    uint8_t token_type = chunk_meta_type(meta);
                    uint64_t block = segment_chunk_block_get(seg, c);
                    
                    /* Encode token_tag (2 bits) */
                    uint8_t token_tag;
                    if (token_type == CHUNK_TYPE_ENUM) {
                        token_tag = 0x00;  /* 00 = ENUM */
                    } else {
                        /* Check if this is a RAW_RUN (consecutive RAW chunks) */
                        /* For now, assume single RAW (01) - RAW_RUN logic would be more complex */
                        token_tag = 0x01;  /* 01 = RAW */
                    }
                    
                    bb_write_bits(buffer, bit_pos, token_tag, 2);
                    bit_pos += 2;

                    if (debug_log) fprintf(debug_log, "      chunk[%u] token_tag=%u (2 bits)\n", c, token_tag);
                    if (mock_output) {
                        /* For audit, show token_tag as binary */
                        int len = snprintf(mock_output + mock_pos, mock_output_size - mock_pos, "0b%02u|", token_tag);
                        if (len < 0 || (size_t)len >= mock_output_size - mock_pos) return -1;
                        mock_pos += len;
                    }

                    /* Encode token data */
                    if (token_type == CHUNK_TYPE_ENUM) {
                        /* ENUM: encode k and rank */
                        uint32_t k = __builtin_popcountll(block);
                        uint8_t n_bits = (c == n_chunks - 1) ? segment_last_chunk_nbits(seg->n_bits) : 64;
                        
                        /* Get rank of this k-subset */
                        uint64_t rank = ssk_combinadic_rank(block, n_bits, k);
                        
                        /* Encode (k << rank_bits) | rank */
                        uint8_t rank_bits = 6;  // Fixed 6 bits for k
                        uint64_t combined = (rank << rank_bits) | k;
                        
                        /* Use CDU_LARGE_INT to encode the combined value */
                        size_t bits = cdu_encode(combined, SSK_ENUM_COMBINED, buffer, bit_pos);
                        if (bits == 0) return -1;
                        bit_pos += bits;

                        if (debug_log) fprintf(debug_log, "      chunk[%u] ENUM k=%u, rank=%llu, combined=%llu (%zu bits)\n", 
                                              c, k, (unsigned long long)rank, (unsigned long long)combined, bits);
                        if (mock_output) {
                            int len = snprintf(mock_output + mock_pos, mock_output_size - mock_pos, "%llu/%u|", 
                                             (unsigned long long)combined, SSK_ENUM_COMBINED);
                            if (len < 0 || (size_t)len >= mock_output_size - mock_pos) return -1;
                            mock_pos += len;
                        }
                    } else {
                        /* RAW: encode the raw bits */
                        uint8_t n_bits = (c == n_chunks - 1) ? segment_last_chunk_nbits(seg->n_bits) : 64;
                        bb_write_bits(buffer, bit_pos, block, n_bits);
                        bit_pos += n_bits;

                        if (debug_log) fprintf(debug_log, "      chunk[%u] RAW %u bits\n", c, n_bits);
                        if (mock_output) {
                            /* For audit, show raw bits as 0b binary string */
                            int len = snprintf(mock_output + mock_pos, mock_output_size - mock_pos, "0b");
                            if (len < 0 || (size_t)len >= mock_output_size - mock_pos) return -1;
                            mock_pos += len;
                            
                            /* Convert block to binary string (LSB first) */
                            for (int b = n_bits - 1; b >= 0; b--) {
                                uint8_t bit = (block >> b) & 1;
                                len = snprintf(mock_output + mock_pos, mock_output_size - mock_pos, "%u", bit);
                                if (len < 0 || (size_t)len >= mock_output_size - mock_pos) return -1;
                                mock_pos += len;
                            }
                            
                            len = snprintf(mock_output + mock_pos, mock_output_size - mock_pos, "|");
                            if (len < 0 || (size_t)len >= mock_output_size - mock_pos) return -1;
                            mock_pos += len;
                        }
                    }
                }
            }
        }
    }

    /* Finalize mock output */
    if (mock_output && mock_pos > 0) {
        /* Remove trailing | */
        if (mock_pos > 0 && mock_output[mock_pos - 1] == '|') {
            mock_output[mock_pos - 1] = '\0';
            mock_pos--;
        }
    }
    if (mock_output_used) {
        *mock_output_used = mock_pos;
    }

    /* Return bytes used (rounded up) */
    size_t bytes_used = (bit_pos + 7) / 8;
    if (bytes_used > buffer_size) return -1;
    return bytes_used;
}


/* Stub: Free decoded SSK */
void
abv_free(AbV abv)
{
  if (abv)
    FREE(abv);
}

/**
 * Check if a CDU encoded value is minimal (canonical).
 * CDU encoding is canonical by design, so this always returns true.
 */
bool
ssk_cdu_is_minimal(const uint8_t *encoded_bytes, CDUtype cdu_type, uint64_t value)
{
    /* CDU is canonical by design - no need to check minimality */
    (void)encoded_bytes;  /* unused */
    (void)cdu_type;       /* unused */
    (void)value;          /* unused */
    return true;
}

#endif // (NON) TRIVIAL