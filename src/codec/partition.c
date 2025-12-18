/*
 * Copyright (c) 2020-2025 Marthin Laubscher
 * All rights reserved. See LICENSE for details.
 */

/*
 * src/codec/partition.c
 *
 * Partition encoding/decoding for SSK
 *
 * A partition represents a 2^32 ID range within the full 2^64 domain.
 * Partitions are sparse - we only encode partitions that contain at least one ID.
 *
 * Partition Format:
 *   [CDU] partition_delta - gap from previous partition (or 0 for first)
 *   [CDU] segment_count - number of segments in this partition
 *   [segments...] - segment_count segments, each RLE or MIX
 *
 * Canon rules:
 *   - Partitions are strictly ascending by partition ID
 *   - No empty partitions (segment_count >= 1)
 *   - Deltas encoded minimally (CDU canon)
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ssk.h"
#include "ssk_format.h"
#include "ssk_constants.h"

/* ============================================================================
 * PARTITION HEADER ENCODING
 * ============================================================================
 */

/**
 * Compute bits needed for partition header (excluding segments).
 *
 * @param partition_delta  Gap from previous partition ID
 * @param segment_count    Number of segments
 * @return Bits needed for header
 *
 * As per Spec Format 0: partition_delta uses CDU_LARGE_INT,
 *                       segment_count uses CDU_SMALL_INT.
 */
size_t
partition_header_bits(uint32_t partition_delta, uint16_t segment_count)
{
    uint8_t dummy_buf[8];
    size_t delta_bits = cdu_encode(partition_delta, SSK_CDU_PARTITION_DELTA, dummy_buf, 0);
    size_t count_bits = cdu_encode(segment_count, SSK_CDU_SEGMENT_COUNT, dummy_buf, 0);
    
    return delta_bits + count_bits;
}

/**
 * Encode a partition header.
 *
 * @param partition_delta  Gap from previous partition
 * @param segment_count    Number of segments
 * @param buf              Output buffer
 * @param bit_pos          Starting bit position
 * @return Bits written
 *
 * As per Spec Format 0: partition_delta uses CDU_LARGE_INT,
 *                       segment_count uses CDU_SMALL_INT.
 */
size_t
partition_header_encode(uint32_t partition_delta, uint16_t segment_count,
                        uint8_t *buf, size_t bit_pos)
{
    size_t start_pos = bit_pos;
    
    /* 1. Partition delta: CDU_LARGE_INT */
    size_t delta_written = cdu_encode(partition_delta, SSK_CDU_PARTITION_DELTA, 
                                       buf, bit_pos);
    bit_pos += delta_written;
    
    /* 2. Segment count: CDU_SMALL_INT */
    size_t count_written = cdu_encode(segment_count, SSK_CDU_SEGMENT_COUNT,
                                       buf, bit_pos);
    bit_pos += count_written;
    
    return bit_pos - start_pos;
}

/**
 * Decode a partition header.
 *
 * @param buf              Input buffer
 * @param bit_pos          Starting bit position
 * @param buf_bits         Total bits available
 * @param delta_out        Output: partition delta
 * @param seg_count_out    Output: segment count
 * @param bits_read        Output: bits consumed
 * @return 0 on success, -1 on error
 *
 * As per Spec Format 0: partition_delta uses CDU_LARGE_INT,
 *                       segment_count uses CDU_SMALL_INT.
 */
int
partition_header_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                        uint32_t *delta_out, uint16_t *seg_count_out,
                        size_t *bits_read)
{
    size_t start_pos = bit_pos;
    
    /* 1. Read partition delta: CDU_LARGE_INT */
    uint64_t delta;
    size_t cdu_read = cdu_decode(buf, bit_pos, buf_bits, SSK_CDU_PARTITION_DELTA, &delta);
    if (cdu_read == 0)
        return -1;
    bit_pos += cdu_read;
    
    /* Validate delta fits in uint32_t */
    if (delta > UINT32_MAX)
        return -1;
    
    /* 2. Read segment count: CDU_SMALL_INT */
    uint64_t seg_count;
    cdu_read = cdu_decode(buf, bit_pos, buf_bits, SSK_CDU_SEGMENT_COUNT, &seg_count);
    if (cdu_read == 0)
        return -1;
    bit_pos += cdu_read;
    
    /* Validate segment count (no empty partitions) */
    if (seg_count == 0 || seg_count > UINT16_MAX)
        return -1;
    
    *delta_out = (uint32_t)delta;
    *seg_count_out = (uint16_t)seg_count;
    *bits_read = bit_pos - start_pos;
    
    return 0;
}

/* ============================================================================
 * SSK HEADER ENCODING
 *
 * The top-level SSK structure starts with:
 *   [CDU] format_version
 *   [CDU] n_partitions
 *   [partitions...] n_partitions partitions
 * ============================================================================
 */

/**
 * Compute bits needed for SSK header (excluding partitions).
 *
 * @param format_version   Format version (typically 0)
 * @param n_partitions  Number of partitions
 * @param spec             Format specification
 * @return Bits needed for header
 */
size_t
ssk_header_bits(uint16_t format_version, uint32_t n_partitions,
                const SSKFormatSpec *spec)
{
    uint8_t dummy_buf[8];
    size_t version_bits = cdu_encode(format_version, spec->cdu_format_version, dummy_buf, 0);
    size_t count_bits = cdu_encode(n_partitions, spec->cdu_segment_count, dummy_buf, 0);
    
    return version_bits + count_bits;
}

/**
 * Encode SSK header.
 *
 * @param format_version   Format version
 * @param n_partitions  Number of partitions
 * @param spec             Format specification
 * @param buf              Output buffer
 * @param bit_pos          Starting bit position
 * @return Bits written
 */
size_t
ssk_header_encode(uint16_t format_version, uint32_t n_partitions,
                  const SSKFormatSpec *spec, uint8_t *buf, size_t bit_pos)
{
    size_t start_pos = bit_pos;
    
    /* 1. Format version (CDU) */
    size_t version_written = cdu_encode(format_version, spec->cdu_format_version,
                                         buf, bit_pos);
    bit_pos += version_written;
    
    /* 2. Partition count (CDU) */
    size_t count_written = cdu_encode(n_partitions, spec->cdu_segment_count,
                                       buf, bit_pos);
    bit_pos += count_written;
    
    return bit_pos - start_pos;
}

/**
 * Decode SSK header.
 *
 * @param buf               Input buffer
 * @param bit_pos           Starting bit position
 * @param buf_bits          Total bits available
 * @param format_out        Output: format version
 * @param part_count_out    Output: partition count
 * @param bits_read         Output: bits consumed
 * @return 0 on success, -1 on error
 *
 * Note: This function determines which format spec to use based on format_version.
 * For now, only Format 0 is supported.
 */
int
ssk_header_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                  uint16_t *format_out, uint32_t *part_count_out,
                  size_t *bits_read)
{
    size_t start_pos = bit_pos;
    
    /* 1. Read format version (CDU with CDU_DEFAULT) */
    uint64_t format_version;
    size_t cdu_read = cdu_decode(buf, bit_pos, buf_bits, CDU_TYPE_DEFAULT, &format_version);
    if (cdu_read == 0)
        return -1;
    bit_pos += cdu_read;
    
    /* Validate format version */
    if (format_version > 0)
        return -1;  /* Only Format 0 supported */
    
    /* 2. Read partition count (CDU) */
    /* Use CDU_SMALL_INT for partition count */
    uint64_t part_count;
    cdu_read = cdu_decode(buf, bit_pos, buf_bits, CDU_TYPE_SMALL_INT, &part_count);
    if (cdu_read == 0)
        return -1;
    bit_pos += cdu_read;
    
    /* Validate partition count fits in uint32_t */
    if (part_count > UINT32_MAX)
        return -1;
    
    *format_out = (uint16_t)format_version;
    *part_count_out = (uint32_t)part_count;
    *bits_read = bit_pos - start_pos;
    
    return 0;
}

/* ============================================================================
 * PARTITION VALIDATION
 * ============================================================================
 */

/**
 * Compute partition ID from delta and previous partition.
 *
 * @param prev_partition   Previous partition ID (UINT32_MAX if none)
 * @param delta            Delta from previous
 * @param partition_out    Output: computed partition ID
 * @return 0 on success, -1 on overflow
 */
int
partition_id_from_delta(uint32_t prev_partition, uint32_t delta,
                        uint32_t *partition_out)
{
    if (prev_partition == UINT32_MAX)
    {
        /* First partition - delta is the partition ID */
        *partition_out = delta;
        return 0;
    }
    
    /* Check for overflow */
    uint64_t new_id = (uint64_t)prev_partition + delta + 1;
    if (new_id > UINT32_MAX)
        return -1;  /* Overflow */
    
    *partition_out = (uint32_t)new_id;
    return 0;
}

/**
 * Compute delta from current and previous partition IDs.
 *
 * @param prev_partition   Previous partition ID (UINT32_MAX if none)
 * @param curr_partition   Current partition ID
 * @return Delta value to encode
 */
uint32_t
partition_delta(uint32_t prev_partition, uint32_t curr_partition)
{
    if (prev_partition == UINT32_MAX)
    {
        /* First partition - delta is the partition ID */
        return curr_partition;
    }
    
    /* Delta is gap minus 1 (consecutive partitions have delta 0) */
    return curr_partition - prev_partition - 1;
}
