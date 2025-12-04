/*
 * src/codec/partition.c
 *
 * Partition encoding/decoding for SSK
 *
 * A partition represents a 2^32 ID range within the full 2^64 domain.
 * Partitions are sparse - we only encode partitions that contain at least one ID.
 *
 * Partition Format:
 *   [VLQ-P] partition_delta - gap from previous partition (or 0 for first)
 *   [VLQ-P] segment_count - number of segments in this partition
 *   [segments...] - segment_count segments, each RLE or MIX
 *
 * Canon rules:
 *   - Partitions are strictly ascending by partition ID
 *   - No empty partitions (segment_count >= 1)
 *   - Deltas encoded minimally (VLQ-P canon)
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
 * As per Spec Format 0: partition_delta uses VLQP_LARGE_INT,
 *                       segment_count uses VLQP_SMALL_INT.
 */
size_t
partition_header_bits(uint32_t partition_delta, uint16_t segment_count)
{
    size_t delta_bits = vlqp_encoded_bits(partition_delta, &SSK_VLQP_PARTITION_DELTA);
    size_t count_bits = vlqp_encoded_bits(segment_count, &SSK_VLQP_SEGMENT_COUNT);
    
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
 * As per Spec Format 0: partition_delta uses VLQP_LARGE_INT,
 *                       segment_count uses VLQP_SMALL_INT.
 */
size_t
partition_header_encode(uint32_t partition_delta, uint16_t segment_count,
                        uint8_t *buf, size_t bit_pos)
{
    size_t start_pos = bit_pos;
    
    /* 1. Partition delta: VLQP_LARGE_INT */
    size_t delta_written = vlqp_encode(partition_delta, &SSK_VLQP_PARTITION_DELTA, 
                                       buf, bit_pos);
    bit_pos += delta_written;
    
    /* 2. Segment count: VLQP_SMALL_INT */
    size_t count_written = vlqp_encode(segment_count, &SSK_VLQP_SEGMENT_COUNT,
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
 * As per Spec Format 0: partition_delta uses VLQP_LARGE_INT,
 *                       segment_count uses VLQP_SMALL_INT.
 */
int
partition_header_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                        uint32_t *delta_out, uint16_t *seg_count_out,
                        size_t *bits_read)
{
    size_t start_pos = bit_pos;
    
    /* 1. Read partition delta: VLQP_LARGE_INT */
    uint64_t delta;
    size_t vlqp_read;
    int rc = vlqp_decode(buf, bit_pos, buf_bits, &SSK_VLQP_PARTITION_DELTA, 
                         &delta, &vlqp_read);
    if (rc != 0)
        return -1;
    bit_pos += vlqp_read;
    
    /* Validate delta fits in uint32_t */
    if (delta > UINT32_MAX)
        return -1;
    
    /* 2. Read segment count: VLQP_SMALL_INT */
    uint64_t seg_count;
    rc = vlqp_decode(buf, bit_pos, buf_bits, &SSK_VLQP_SEGMENT_COUNT,
                     &seg_count, &vlqp_read);
    if (rc != 0)
        return -1;
    bit_pos += vlqp_read;
    
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
 *   [VLQ-P] format_version
 *   [VLQ-P] n_partitions
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
    size_t version_bits = vlqp_encoded_bits(format_version, spec->vlqp_format_version);
    size_t count_bits = vlqp_encoded_bits(n_partitions, spec->vlqp_segment_count);
    
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
    
    /* 1. Format version (VLQ-P) */
    size_t version_written = vlqp_encode(format_version, spec->vlqp_format_version,
                                         buf, bit_pos);
    bit_pos += version_written;
    
    /* 2. Partition count (VLQ-P) */
    size_t count_written = vlqp_encode(n_partitions, spec->vlqp_segment_count,
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
    
    /* 1. Read format version (VLQ-P with VLQP_DEFAULT) */
    uint64_t format_version;
    size_t vlqp_read;
    int rc = vlqp_decode(buf, bit_pos, buf_bits, &VLQP_DEFAULT, 
                         &format_version, &vlqp_read);
    if (rc != 0)
        return -1;
    bit_pos += vlqp_read;
    
    /* Validate format version */
    if (format_version > 0)
        return -1;  /* Only Format 0 supported */
    
    /* 2. Read partition count (VLQ-P) */
    /* Use VLQP_SMALL_INT for partition count */
    uint64_t part_count;
    rc = vlqp_decode(buf, bit_pos, buf_bits, &VLQP_SMALL_INT,
                     &part_count, &vlqp_read);
    if (rc != 0)
        return -1;
    bit_pos += vlqp_read;
    
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
