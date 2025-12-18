/*
 * Copyright (c) 2020-2025 Marthin Laubscher
 * All rights reserved. See LICENSE for details.
 */

#ifndef SSK_CONSTANTS_H
#define SSK_CONSTANTS_H

/**
 * @file ssk_constants.h
 * @brief Canonical SSK constants and thresholds
 *
 * These constants define the canonical encoding behavior.
 * DO NOT modify these values without understanding the impact on:
 *   - Canonical encoding/decoding bijection
 *   - Backward compatibility with existing encoded SSKs
 *   - Test suite expectations
 */

// ============================================================================
// CHUNKING AND TOKENIZATION
// ============================================================================

/**
 * Fixed chunk size for MIX segment interior (excludes boundary rare bits)
 * Must be power of 2 for efficient bit operations
 */
#define SSK_DEFAULT_CHUNK_BITS  64

/**
 * Maximum popcount for ENUM (rank-encoded) chunks
 * Chunks with k ≤ K_CHUNK_ENUM_MAX use combinadic encoding
 * Chunks with k > K_CHUNK_ENUM_MAX use RAW storage
 * 
 * Choice of 18: Balance between:
 *   - Combinadic rank_bits table size (precomputed C(n,k) for n≤64, k≤18)
 *   - Compression effectiveness (rank vs raw bits)
 *   - Decoding speed (unrank computation)
 *
 * As per Spec Format 0.
 */
#define SSK_K_CHUNK_ENUM_MAX    18

/**
 * Bits required to encode k value in ENUM token
 * 
 * For k=0..18, need at least 5 bits (2^5 = 32 > 18).
 * Format 0 uses 6 bits for alignment and future-proofing.
 *
 * As per Spec Format 0.
 */
#define SSK_N_BITS_FOR_K        6

// ============================================================================
// SEGMENTATION THRESHOLDS
// ============================================================================

/**
 * Dominant-only run threshold (bits)
 * Gaps of dominant bits with length ≥ DOMINANT_RUN_THRESHOLD are:
 *   - Omitted entirely from encoding (implicit)
 *   - Not stored as RLE or MIX segments
 * 
 * Must be > RARE_RUN_THRESHOLD to avoid ambiguity.
 */
#define SSK_DOMINANT_RUN_THRESHOLD  96

/**
 * Rare-bit run threshold (bits)
 * Runs of rare bits with length ≥ RARE_RUN_THRESHOLD are:
 *   - Encoded as RLE segments (single membership_bit)
 *   - More efficient than MIX for long consecutive runs
 */
#define SSK_RARE_RUN_THRESHOLD      64

/**
 * Maximum segment length hint (bits)
 * MIX segments exceeding this hint should be split at rare-rare boundaries
 * (split opportunities). Split algorithm:
 *   1. If segment would exceed hint, find first split opportunity BEFORE the hint
 *   2. If none exists before, use first split opportunity AFTER the hint
 *   3. If no split opportunity exists at all, output as single long segment
 * 
 * NOT a hard limit - segments may exceed when no split opportunity exists.
 * 
 * Balances:
 *   - Decoding parallelism (smaller segments)
 *   - Encoding overhead (segment headers)
 *   - Cache locality
 */
#define SSK_MAX_SEGMENT_LEN_HINT  2048

// ============================================================================
// CDU LINKING (which CDU subtype for each encoded field)
// ============================================================================

/**
 * CDU subtype assignments for encoded fields.
 * 
 * The "linking problem": determining which CDU subtype (set of parameters)
 * to use for each encoded integer field to minimize total encoding size.
 * 
 * EMPIRICAL TUNING APPROACH:
 *   1. Profile representative SSK workloads (e.g., 1M real subsets)
 *   2. Generate histograms of actual values for each field:
 *      - partition_delta: How often 0? 1? 100? 10000?
 *      - segment_count: Distribution of segments per partition
 *      - length_bits: Distribution of segment lengths
 *      etc.
 *   3. Design/adjust stage_bits[] arrays to match observed distributions
 *   4. Measure total encoding size with different linkings
 *   5. Iterate: adjust subtypes, re-measure, converge on optimal
 * 
 * Current assignments are PLACEHOLDERS based on intuition.
 * Replace with profiling-driven choices after collecting real data.
 * 
 * Expected characteristics (pre-profiling assumptions):
 *   - partition_delta: Often 0 (contiguous), occasionally huge (sparse IDs)
 *   - segment_count: Mostly 1-5, rarely >20
 *   - initial_delta: Often 0 (adjacent), sometimes large (gaps)
 *   - length_bits: Mostly 64-512, occasionally 2048+
 *   - popcount: Varies wildly (depends on density)
 *   - raw_run_len: Mostly 1-3, rarely >10
 *   - enum_combined: Depends on k (0-18) and rank_bits (0-60)
 */

// Field → CDU type mapping
#define SSK_CDU_PARTITION_DELTA   SSK_PARTITION_DELTA
#define SSK_CDU_SEGMENT_COUNT     SSK_N_SEGMENTS
#define SSK_CDU_INITIAL_DELTA     SSK_SEGMENT_START_BIT
#define SSK_CDU_LENGTH_BITS       SSK_SEGMENT_N_BITS
#define SSK_CDU_POPCOUNT          CDU_TYPE_MEDIUM_INT   // Not used in Format 0
#define SSK_CDU_RAW_RUN_LEN       CDU_TYPE_SMALL_INT    // Not used in Format 0
#define SSK_CDU_ENUM_COMBINED     SSK_ENUM_COMBINED

// ============================================================================
// TAG VALUES (Segment and Token Kinds)
// ============================================================================

/**
 * Segment kind tag (1 bit)
 * Encoded in segment header tag byte
 */
#define SSK_SEG_TAG_RLE  0  // 0 = RLE (rare run)
#define SSK_SEG_TAG_MIX  1  // 1 = MIX (mixed 0s and 1s)

/**
 * Token kind tags (2 bits)
 * Encoded in MIX segment token stream
 */
#define SSK_TOK_TAG_ENUM     0  // 00 = ENUM (rank-encoded)
#define SSK_TOK_TAG_RAW      1  // 01 = RAW (single chunk)
#define SSK_TOK_TAG_RAW_RUN  2  // 10 = RAW_RUN (coalesced)
#define SSK_TOK_TAG_RESERVED 3  // 11 = Reserved (reject)

// ============================================================================
// BIT ORDER
// ============================================================================

/**
 * Bit ordering convention:
 *   - Within bytes: MSB-first (most significant bit written first)
 *   - Byte sequence: Big-endian in bit significance
 *   - Combinadic positions: [0..n-1] from MSB to LSB
 * 
 * No platform endianness assumptions; portable across architectures.
 */

// ============================================================================
// ERROR HANDLING
// ============================================================================

/**
 * Canonical violation error code
 * Used for ereport() when decoding detects non-canonical encoding
 */
#ifdef USE_PG
#include "postgres.h"
#define SSK_ERRCODE_CANONICAL  ERRCODE_DATA_CORRUPTED
#else
#define SSK_ERRCODE_CANONICAL  (-1)
#endif

// ============================================================================
// COMBINADIC RANK BITS TABLE
// ============================================================================

/**
 * Precomputed rank_bits[n][k] = ceil(log2(C(n, k)))
 * 
 * Dimensions:
 *   - n: 1..64 (chunk size)
 *   - k: 0..min(n, K_CHUNK_ENUM_MAX) = 0..18
 * 
 * Total table size: 65 × 19 × 1 byte = 1,235 bytes
 * 
 * Initialized once at module load via ssk_rank_init().
 */
#define SSK_RANK_BITS_N_MAX  64
#define SSK_RANK_BITS_K_MAX  (SSK_K_CHUNK_ENUM_MAX)

// ============================================================================
// VALIDATION FLAGS
// ============================================================================

/**
 * Flags for canonical validation during decode
 * Can be combined (bitwise OR) for fine-grained control
 */
typedef enum {
    SSK_VALIDATE_NONE           = 0x00,  // No validation (unsafe!)
    SSK_VALIDATE_CDU_MINIMAL    = 0x01,  // Check CDU minimality
    SSK_VALIDATE_ORDERING       = 0x02,  // Check partition/segment ordering
    SSK_VALIDATE_RARE_BIT       = 0x04,  // Check rare_bit == ~dominant_bit & 1
    SSK_VALIDATE_THRESHOLD      = 0x08,  // Check run length thresholds
    SSK_VALIDATE_TOKEN_BOUNDS   = 0x10,  // Check ENUM rank bounds
    SSK_VALIDATE_ALL            = 0xFF   // Full canonical validation
} SskValidateFlags;

// Default validation level (full validation in debug, minimal in release)
#ifdef NDEBUG
#define SSK_VALIDATE_DEFAULT  (SSK_VALIDATE_CDU_MINIMAL | SSK_VALIDATE_ORDERING)
#else
#define SSK_VALIDATE_DEFAULT  SSK_VALIDATE_ALL
#endif

#endif // SSK_CONSTANTS_H
