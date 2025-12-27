#ifndef SSK_DECODED_H
#define SSK_DECODED_H
/*
 * Copyright (c) 2020-2025 Marthin Laubscher
 * All rights reserved. See LICENSE for details.
 */

/*
 * include/ssk_decoded.h
 *
 * In-memory representation of abstract bit vectors (AbV).
 * Defines C structures for both TRIVIAL (uint64_t) and NON-TRIVIAL (hierarchical)
 * domains, plus operations to construct and manipulate them.
 */
#ifdef TRIVIAL

#include <stdint.h>

/*
 * The dirty little secret of the trivial implementation is that the Abstract bit Vector that
 * in the full implementation would be such a centre of attention, is not abstract at all,
 * but a simple, 64 bit physical bit vector represented by a single uint64_t.
 *
 */

typedef uint64_t AbV;

#else // NON TRIVIAL

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * Design Principles:
 * ==================
 * 1. Single contiguous allocation - entire AbV lives in one memory block
 * 2. No pointers - only offsets relative to defined bases (realloc-safe)
 * 3. Hierarchical offset arrays - each level has FAM for offsets to children
 * 4. Separated metadata from payload - chunk bitmaps are contiguous for SIMD/memcpy
 * 5. Grow-in-place - write into virgin territory, finalize when done
 *
 * Memory Layout (conceptual):
 * ===========================
 * 
 * AbV (root):
 *   [header fields]
 *   [partition_offs[]: uint32_t FAM - offsets to AbVPartition structs]
 *   ... variable data: AbVPartition structs and their children ...
 *
 * AbVPartition:
 *   [header fields]  
 *   [segment_offs[]: uint32_t FAM - offsets to AbVSegment structs]
 *   ... variable data: AbVSegment structs and their children ...
 *
 * AbVSegment (MIX type):
 *   [header fields including n_chunks, blocks_off]
 *   [data[]: uint64_t FAM]
 *      - First: metadata[] (packed 2-bit per chunk: type + dirty flag)
 *      - Optional padding for in-place growth
 *      - Then at blocks_off: blocks[] (the actual 64-bit bitmaps, contiguous)
 *
 * AbVSegment (RLE type):
 *   [header fields only - no chunks, n_chunks=0, data[] empty]
 *
 * Offset Semantics:
 * =================
 * - AbV.partition_offs[i]: byte offset from &partition_offs[0]
 * - AbVPartition.segment_offs[i]: byte offset from &segment_offs[0]
 * - AbVSegment.blocks_off: byte offset from &data[0] to blocks array
 * - All offsets are uint32_t (4GB max per SSK, more than sufficient)
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================================
 * CHUNK METADATA
 *
 * In-memory, each chunk has 2 bits of metadata packed into the segment's
 * metadata array (first part of AbVSegment.data[]):
 *
 *   Bit 0: token_type (0=ENUM, 1=RAW)
 *   Bit 1: dirty_flag (0=clean, 1=needs re-normalization)
 *
 * In encoded form, token_tag is 2 bits with different semantics:
 *   00=ENUM, 01=RAW, 10=RAW_RUN, 11=ENUM_RUN
 *
 * The RUN variants are derived during encoding from consecutive identical
 * in-memory entries; they expand back during decoding.
 *
 * Packing: 32 chunks per uint64_t (2 bits × 32 = 64 bits)
 * ============================================================================
 */

#define CHUNK_TYPE_ENUM  0
#define CHUNK_TYPE_RAW   1

#define CHUNK_FLAG_CLEAN 0
#define CHUNK_FLAG_DIRTY 1

/* Extract token type from 2-bit metadata value */
static inline uint8_t
chunk_meta_type(uint8_t meta)
{
    return meta & 0x01;
}

/* Extract dirty flag from 2-bit metadata value */
static inline uint8_t
chunk_meta_dirty(uint8_t meta)
{
    return (meta >> 1) & 0x01;
}

/* Pack token type and dirty flag into 2-bit metadata */
static inline uint8_t
chunk_meta_pack(uint8_t type, uint8_t dirty)
{
    return ((dirty & 0x01) << 1) | (type & 0x01);
}

/* ============================================================================
 * SEGMENT-LEVEL STRUCTURE
 *
 * A segment covers a contiguous range of bit positions within a partition.
 *
 * RLE segments: All bits in range have same value (rare_bit).
 *               No chunks - all data encoded in header (n_chunks = 0).
 *
 * MIX segments: Variable bit patterns, stored as chunks.
 *               FAM contains: metadata[] (packed 2-bit) first, then after
 *               blocks_off, the uint64_t blocks[] (actual bitmaps).
 *
 * Memory layout for MIX:
 *   [AbVSegment header]
 *   [data[0..meta_words-1]: packed 2-bit metadata]
 *   [... optional padding for growth ...]
 *   [blocks[0..n_chunks-1]: uint64_t bitmaps, starting at blocks_off]
 *
 * This layout allows:
 *   - Metadata array can grow in-place if blocks_off > meta_words*8
 *   - Blocks are contiguous for efficient RAW_RUN memcpy/SIMD
 * ============================================================================
 */

/* Segment types */
#define SEG_TYPE_RLE  0
#define SEG_TYPE_MIX  1

typedef struct AbVSegment {
    uint32_t n_bits;             /* Total bits in this segment (source of truth) */
    uint32_t blocks_off;         /* Byte offset from &data[0] to blocks[] */
    uint32_t blocks_allocated;   /* Number of blocks allocated (for growth) */
    uint32_t start_bit;          /* Starting bit position within partition */
    uint32_t cardinality;        /* Count of set bits (cached, max 2^32) */
    uint8_t  segment_type;       /* SEG_TYPE_RLE or SEG_TYPE_MIX */
    uint8_t  rare_bit;           /* 0 or 1, whichever is rare, repeated in RLE, popcounted in MIX */
    uint8_t  _pad[2];            /* Alignment padding */
    
    /*
     * FAM layout (MIX segments):
     *   data[0 .. meta_alloc_words-1]: packed 2-bit metadata per chunk
     *   data[blocks_off/8 .. ]:        uint64_t blocks[blocks_allocated]
     *
     * blocks_off is set to allow growth in both regions:
     *   - Metadata can grow up to blocks_off / 8 words
     *   - Blocks can grow up to blocks_allocated entries
     *
     * For RLE segments: n_bits > 0 but no chunks, blocks_off=0, data[] is empty.
     *
     * Derived values:
     *   n_chunks = (n_bits + 63) / 64
     *   last_chunk_nbits = ((n_bits - 1) % 64) + 1  (for n_bits > 0)
     */
    uint64_t data[];
} AbVSegment;

/* ============================================================================
 * CHUNK ACCESS API
 *
 * Both metadata and blocks are accessed using the same chunk index i.
 * This ensures consistent addressing across the two parallel arrays.
 * ============================================================================
 */

/* Number of 64-bit chunks for n_bits */
static inline uint32_t
segment_n_chunks(uint32_t n_bits)
{
    return (n_bits + 63) / 64;
}

/* Bits valid in final chunk (1-64), assumes n_bits > 0 */
static inline uint8_t
segment_last_chunk_nbits(uint32_t n_bits)
{
    return ((n_bits - 1) % 64) + 1;
}

/* Number of uint64_t words needed for n_bits worth of 2-bit chunk metadata */
static inline uint32_t
segment_meta_words(uint32_t n_bits)
{
    uint32_t n_chunks = segment_n_chunks(n_bits);
    return (n_chunks + 31) / 32;  /* 32 chunks per uint64_t at 2 bits each */
}

/* Get 2-bit metadata for chunk i */
static inline uint8_t
segment_chunk_meta_get(const AbVSegment *seg, uint32_t i)
{
    uint32_t word_idx = i / 32;
    uint32_t bit_idx = (i % 32) * 2;
    return (seg->data[word_idx] >> bit_idx) & 0x03;
}

/* Set 2-bit metadata for chunk i */
static inline void
segment_chunk_meta_set(AbVSegment *seg, uint32_t i, uint8_t meta)
{
    uint32_t word_idx = i / 32;
    uint32_t bit_idx = (i % 32) * 2;
    uint64_t mask = ~(0x03ULL << bit_idx);
    seg->data[word_idx] = (seg->data[word_idx] & mask) | ((uint64_t)(meta & 0x03) << bit_idx);
}

/* Check if chunk i has token type RAW */
static inline bool
segment_chunk_is_raw(const AbVSegment *seg, uint32_t i)
{
    return chunk_meta_type(segment_chunk_meta_get(seg, i)) == CHUNK_TYPE_RAW;
}

/* Check if chunk i is dirty */
static inline bool
segment_chunk_is_dirty(const AbVSegment *seg, uint32_t i)
{
    return chunk_meta_dirty(segment_chunk_meta_get(seg, i)) == CHUNK_FLAG_DIRTY;
}

/* Get pointer to block i (the actual 64-bit bitmap) */
static inline uint64_t *
segment_chunk_block(AbVSegment *seg, uint32_t i)
{
    uint64_t *blocks = (uint64_t *)((uint8_t *)seg->data + seg->blocks_off);
    return &blocks[i];
}

/* Get block value for chunk i (read-only convenience) */
static inline uint64_t
segment_chunk_block_get(const AbVSegment *seg, uint32_t i)
{
    const uint64_t *blocks = (const uint64_t *)((const uint8_t *)seg->data + seg->blocks_off);
    return blocks[i];
}

/* Set block value for chunk i */
static inline void
segment_chunk_block_set(AbVSegment *seg, uint32_t i, uint64_t value)
{
    uint64_t *blocks = (uint64_t *)((uint8_t *)seg->data + seg->blocks_off);
    blocks[i] = value;
}

/* Total size in bytes of an AbVSegment with given allocation */
static inline size_t
segment_size(uint32_t blocks_off, uint32_t blocks_allocated)
{
    return sizeof(AbVSegment) + blocks_off + blocks_allocated * sizeof(uint64_t);
}

/* Minimum blocks_off for segment with n_bits (no growth padding) */
static inline uint32_t
segment_min_blocks_off(uint32_t n_bits)
{
    return segment_meta_words(n_bits) * sizeof(uint64_t);
}

/* ============================================================================
 * PARTITION-LEVEL STRUCTURE
 *
 * A partition covers 2^32 consecutive IDs.
 * partition_id indexes which 2^32-ID range this partition covers.
 *
 * Contains an offset array pointing to AbVSegment structures, followed by
 * the segments themselves in the variable-length area.
 * ============================================================================
 */

typedef struct AbVPartition {
    uint32_t partition_id;       /* Which partition (indexes 2^32 ID ranges) */
    uint32_t n_segments;         /* Number of segments */
    uint32_t var_data_off;       /* Byte offset from segment_offs[0] to var data start */
    uint32_t var_data_used;      /* Bytes used in var data area */
    uint32_t var_data_allocated; /* Bytes allocated for var data area */
    uint32_t cardinality;        /* Count of set bits (cached, max 2^32) */
    uint8_t  rare_bit;           /* Partition rare bit (when partition crosses dominance threshold) */
    uint8_t  _pad[3];            /* Alignment */
    
    /*
     * FAM: Offset array followed by variable-length segment data
     *
     *   uint32_t segment_offs[n_segments];  -- offsets from &segment_offs[0]
     *   ... AbVSegment structs (variable length) ...
     *
     * Access: segment_offs[i] gives byte offset to AbVSegment i
     */
    uint32_t segment_offs[];
} AbVPartition;

/* Get pointer to segment i within this partition */
static inline AbVSegment *
partition_segment(AbVPartition *part, uint32_t i)
{
    uint8_t *base = (uint8_t *)part->segment_offs;
    return (AbVSegment *)(base + part->segment_offs[i]);
}

/* Minimum size of AbVPartition header with n_segments offset slots */
static inline size_t
partition_header_size(uint32_t n_segments)
{
    return sizeof(AbVPartition) + n_segments * sizeof(uint32_t);
}

/* ============================================================================
 * ROOT-LEVEL STRUCTURE: AbV
 *
 * The top-level container for a decoded SSK.
 * Single allocation containing all partitions, segments, and chunk data.
 * ============================================================================
 */

typedef struct AbVRoot {
    uint16_t format_version;     /* Format this was decoded from (or will encode to) */
    uint8_t  rare_bit;           /* Global rare bit (0 or 1) - backstop for complement */
    uint8_t  _pad1;
    uint32_t n_partitions;       /* Number of partitions present in this SSK */
    uint32_t var_data_off;       /* Byte offset from partition_offs[0] to var data start */
    uint32_t var_data_used;      /* Bytes used in var data area */
    uint32_t var_data_allocated; /* Bytes allocated for var data area */
    uint32_t total_allocated;    /* Bytes allocated in total */
    uint64_t cardinality;        /* Total count of set bits across all partitions */
    
    /*
     * FAM: Offset array followed by variable-length partition data
     *
     *   uint32_t partition_offs[n_partitions];  -- offsets from &partition_offs[0]
     *   ... AbVPartition structs (variable length, containing segments) ...
     *
     * Access: partition_offs[i] gives byte offset to AbVPartition i
     */
    uint32_t partition_offs[];
} AbVRoot;

typedef AbVRoot *AbV;

/* Get pointer to partition i */
static inline AbVPartition *
decoded_partition(AbV abv, uint32_t i)
{
    uint8_t *base = (uint8_t *)abv->partition_offs;
    return (AbVPartition *)(base + abv->partition_offs[i]);
}

/* Minimum size of AbVRoot header with n_partitions offset slots */
static inline size_t
decoded_header_size(uint32_t n_partitions)
{
    return sizeof(AbVRoot) + n_partitions * sizeof(uint32_t);
}

/* ============================================================================
 * MEMORY MANAGEMENT
 *
 * Growth model:
 * - Start with initial allocation
 * - Write sequentially into var_data area
 * - If more space needed, realloc entire block (offsets remain valid)
 * - Finalize partition/segment when complete, then start next
 * ============================================================================
 */

/**
 * Allocate a new empty AbV.
 *
 * @param format_version  Format version (typically 0)
 * @param initial_size    Initial total allocation in bytes
 * @return Newly allocated structure, or NULL on failure
 */
AbV abv_alloc(uint16_t format_version, size_t initial_size);

/**
 * Grow AbV to accommodate more data.
 *
 * @param abv            Existing structure (may be reallocated)
 * @param needed_bytes    Additional bytes needed
 * @return New pointer (may differ from input), or NULL on failure
 *
 * All offsets remain valid after reallocation.
 */
AbV abv_grow(AbV abv, size_t needed_bytes);

/**
 * Free an AbV structure.
 */
void abv_free(AbV abv);

/* ============================================================================
 * BUILDER HELPERS
 *
 * These assist in constructing AbV incrementally during decoding
 * or programmatic construction.
 * ============================================================================
 */

/**
 * Begin a new partition within the AbV.
 *
 * @param abv            Parent structure (may be reallocated)
 * @param partition_id   0 or 1
 * @param initial_segments  Expected segment count (can grow)
 * @return Updated abv pointer, or NULL on failure
 */
AbV abv_begin_partition(AbV abv, uint32_t partition_id, 
                                 uint32_t initial_segments);

/**
 * Begin a new MIX segment within current partition.
 *
 * @param dec            Parent structure (may be reallocated)
 * @param start_bit      Starting bit position
 * @param n_chunks       Number of 64-bit chunks
 * @param last_nbits     Valid bits in final chunk (1-64)
 * @return Updated dec pointer, or NULL on failure
 */
AbV abv_begin_mix_segment(AbV abv, uint32_t start_bit,
                                   uint32_t n_chunks, uint8_t last_nbits);

/**
 * Add an RLE segment to current partition.
 *
 * @param dec            Parent structure (may be reallocated)
 * @param start_bit      Starting bit position
 * @param n_bits         Number of bits (all same value)
 * @param rare_bit       The uniform bit value (0 or 1)
 * @return Updated dec pointer, or NULL on failure
 */
AbV abv_add_rle_segment(AbV abv, uint32_t start_bit,
                                 uint32_t n_bits, uint8_t rare_bit);

/**
 * Finalize current segment (update metadata, cardinality).
 */
void abv_finalize_segment(AbV abv);

/**
 * Finalize current partition.
 */
void abv_finalize_partition(AbV abv);

#endif // (NON) TRIVIAL

#endif /* SSK_DECODED_H */
