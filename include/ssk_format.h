#ifndef SSK_FORMAT_H
#define SSK_FORMAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ssk_decoded.h"
#include "cdu.h"

/**
 * @file ssk_format.h
 * @brief SSK encoding format specifications and decoded memory structures
 *
 * SSK CONCEPTUAL MODEL:
 * =====================
 * An SSK represents a subset as an "abstract bit vector" (abvector) - conceptually,
 * one bit per possible primary key value in the domain (0 to 2^64-1). Each abstact
 * bit (absbit) indicates presence (1) or absence (0) of that ID in the subset.
 *
 * The abvector is astronomically sparse (like matter in space), so SSK exploits
 * this emptiness through hierarchical compression:
 *   Domain (2^64 values) → Partitions (2^32 each) → Segments → Chunks
 *
 * CANONICITY - THE FOUNDATION:
 * ============================
 * An encoded SSK is CANONICAL: there is exactly ONE valid encoding for any given
 * subset under a specific format version. This bijection is critical:
 *
 *   Subset of IDs <-> Encoded Bytes
 *
 * Canon ensures:
 *   - Same subset ALWAYS produces identical bytes
 *   - Different subsets ALWAYS produce different bytes  
 *   - Encoding is independent of construction history
 *   - Equality/hashing/indexing work correctly
 *
 * Breaking canon breaks SSK. All encoding decisions MUST be deterministic:
 *   - Segment boundaries: Split at dominant gaps >= DOMINANT_RUN_THRESHOLD
 *   - Segment kind: RLE if rare_run >= RARE_RUN_THRESHOLD and spans segment
 *   - Token kind: ENUM if popcount <= K_ENUM_MAX, else RAW
 *   - RAW coalescing: Consecutive RAW -> RAW_RUN (mandatory)
 *   - Ordering: Partitions and segments strictly ascending
 *   - CDU: Minimally encoded (no unnecessary continuation bits)
 *
 * Canon parameters (chunk size, thresholds, etc.) are IMMUTABLE per format.
 * Changing any parameter requires a new format version.
 *
 * ENCODING vs DECODING:
 * =====================
 * ENCODED: Space-optimized, variable-length, no alignment, canonical (immutable order)
 * DECODED: Memory-optimized, fixed offsets, aligned, uses offsets not pointers
 *
 * Decoded structures use offsets instead of pointers because:
 *   a) Smaller (32-bit vs 64-bit)
 *   b) Survive realloc without recalculation
 *
 * Memory layout uses flexible array members (FAM) pattern:
 *   - Fixed-size headers as searchable arrays (binary search)
 *   - Variable-length data in FAM sections
 *
 * FORMAT VERSIONING:
 * ==================
 * Format version is encoded at the start of every SSK. Different formats
 * can coexist - decode any supported format, encode back to same format or
 * default encoding format. Format changes should only occur during major system
 * upgrades with bulk re-encoding.
 *
 * Format 0 is intended to remain the default indefinitely unless a critical
 * flaw necessitates change. The goal: ONE FORMAT, FOREVER.
 */

// ============================================================================
// CDU (Canonical Data Unit)
// ============================================================================

/**
 * CDU (Canonical Data Unit)
 * 
 * CDU provides both fixed-length and variable-length encoding with minimal logic.
 * Each CDU type is defined by CDUParam structure specifying bit allocation.
 * 
 * For variable-length: Uses continuation bits to extend encoding across segments.
 * For fixed-length: Writes exactly the specified number of bits.
 * 
 * TUNING PHILOSOPHY:
 * CDU types are tuned based on empirical data from real-world SSK encodings.
 * Initial types are educated guesses; refine after profiling production data.
 * 
 * The "linking problem" (which type for which field) is empirical:
 * assign based on observed characteristics, measure results, adjust as needed.
 */

// ============================================================================
// CDU FIELD MAPPINGS
// ============================================================================

// Field-to-CDU-type mapping for Format 0
#define SSK_FORMAT            CDU_TYPE_DEFAULT
#define SSK_PARTITIONS        CDU_TYPE_SMALL_INT
#define SSK_PARTITION_DELTA   CDU_TYPE_LARGE_INT
#define SSK_N_SEGMENTS        CDU_TYPE_SMALL_INT
#define SSK_SEGMENT_START_BIT CDU_TYPE_INITIAL_DELTA
#define SSK_SEGMENT_N_BITS    CDU_TYPE_MEDIUM_INT
#define SSK_ENUM_K            CDU_TYPE_ENUM_K
#define SSK_ENUM_RANK         CDU_TYPE_ENUM_RANK
#define SSK_ENUM_COMBINED     CDU_TYPE_ENUM_COMBINED
#define SSK_RAW_RUN_LEN       CDU_TYPE_SMALL_INT

// ============================================================================
// FORMAT CONSTANTS AND THRESHOLDS
// ============================================================================

// Chunking and tokenization
#define SSK_DEFAULT_CHUNK_BITS      64    // Fixed chunk size for MIX segments
#define SSK_K_CHUNK_ENUM_MAX        18    // Max popcount for ENUM (rank-encoded) chunks

// Segmentation thresholds
#define SSK_DOMINANT_RUN_THRESHOLD  96    // Min bits to omit dominant-only gap
#define SSK_RARE_RUN_THRESHOLD      64    // Min bits for RLE segment
#define SSK_MAX_SEGMENT_LEN_HINT  2048    // Hint for MIX segment splitting (not hard limit)

// ============================================================================
// FORMAT SPECIFICATION STRUCTURE
// ============================================================================

/**
 * SSKFormatSpec defines the complete canonicity rules for a specific format.
 * 
 * CRITICAL: These parameters define the CANON for each format version.
 * Changing ANY parameter breaks canonicity and requires a new format version.
 * 
 * Canon ensures bijection: Subset <-> Encoded Bytes
 * - Same subset MUST produce identical bytes
 * - Different subsets MUST produce different bytes
 * - Construction history (operation order) MUST NOT affect encoding
 * 
 * Format 0 parameters are FROZEN. They define the canonical encoding forever.
 * Only critical flaws justify introducing Format 1 with different parameters.
 */
typedef struct SSKFormatSpec {
    // Format identity
    uint16_t format_version;              // Format identifier (0 = Format 0)
    uint64_t max_abits;                   // Maximum abstract bits (2^64-1 for full domain)
    uint8_t  partition_size_bits;         // Bits per partition (32 = 2^32 IDs per partition)
    
    // Chunk and token parameters (deterministic encoding decisions)
    uint16_t chunk_bits;                  // Fixed chunk size (Format 0: 64)
    uint8_t  k_enum_max;                  // Max popcount for ENUM token (Format 0: 18)
                                          // Chunks with k <= k_enum_max -> ENUM (combinadic)
                                          // Chunks with k > k_enum_max  -> RAW (raw bits)
    uint8_t  n_bits_for_k;                // Bits to encode k in ENUM (Format 0: 6)
    
    // Segmentation thresholds (deterministic segment boundary rules)
    uint16_t dominant_run_threshold;      // Min bits to split at dominant gap (Format 0: 96)
                                          // Gaps >= threshold MUST split segments
    uint16_t rare_run_threshold;          // Min bits for RLE segment (Format 0: 64)
                                          // Rare runs >= threshold AND full-span -> RLE
                                          // Otherwise -> MIX
    uint16_t max_segment_len_hint;        // Soft limit for MIX segment (Format 0: 2048)
                                          // Hint only; encoder may exceed if no natural split
    
    // Small Non-Trivial (SNT) handling
    uint16_t snt_limit;                   // Spans <= snt_limit use simplified path (Format 0: 64)
    
    // CDU linking (assigned types for each encoded field)
    // These define which CDU parameters are used for each field type.
    // Once stabilized, these become immutable for the format.
    CDUtype cdu_format_version;           // For format version field
    CDUtype cdu_partition_delta;          // For partition gaps
    CDUtype cdu_segment_count;            // For segment counts
    CDUtype cdu_initial_delta;            // For segment offset gaps
    CDUtype cdu_length_bits;              // For segment lengths
    CDUtype cdu_popcount;                 // For popcount encoding
    CDUtype cdu_enum_combined;            // For (k << rank_bits) | rank
    CDUtype cdu_raw_run_len;              // For RAW_RUN length
    
    // Bit ordering (for cross-format compatibility)
    uint8_t  chunk_bit_order;             // 0=MSB-first (Format 0), 1=LSB-first
    
    // Future extension point
    void *reserved;
} SSKFormatSpec;

// Default encoding format (what new SSKs are encoded as)
extern uint16_t SSK_DEFAULT_ENCODING_FORMAT;

// ============================================================================
// DECODED MEMORY STRUCTURES
// ============================================================================

/**
 * Segment kinds (encoded as 1-bit tag)
 */
typedef enum {
    SEG_RLE = 0,  // Run-Length Encoding: consecutive membership bits (rare runs)
    SEG_MIX = 1   // Mixed: both 0s and 1s, stored as tokens
} SegKind;

/**
 * Token kinds (encoded as 2-bit tags)
 */
typedef enum {
    TOK_ENUM    = 0,  // 00: Rank-encoded sparse chunk (k ≤ K_CHUNK_ENUM_MAX)
    TOK_RAW     = 1,  // 01: Raw bitstring for single chunk
    TOK_RAW_RUN = 2,  // 10: Consecutive RAW chunks (coalesced)
    TOK_RESERVED = 3  // 11: Reserved (reject if encountered)
} TokenKind;

// ============================================================================
// DECODED MEMORY STRUCTURES
// ============================================================================
// All structures use uint32 offsets instead of pointers for realloc safety.
// Top-level handle is a pointer; all nested references are offsets into FAM.

/**
 * RLE segment payload (decoded)
 */
typedef struct SSKRlePayload {
    uint8_t membership_bit;   // 0 or 1 (the rare bit value)
} SSKRlePayload;

/**
 * ENUM token data (decoded)
 * Represents a sparse chunk as combinadic rank
 */
typedef struct SSKEnumData {
    uint8_t  nbits;           // Chunk length (1..64)
    uint8_t  k;               // Popcount (0..K_CHUNK_ENUM_MAX)
    uint64_t rank;            // Combinadic rank (MSB→LSB positions)
} SSKEnumData;

/**
 * RAW token data (decoded)
 * Single chunk stored as raw bits
 */
typedef struct SSKRawData {
    uint8_t  nbits;           // Chunk length (1..64)
    uint32_t bits_off;        // Offset to packed bits (MSB-first)
} SSKRawData;

/**
 * RAW_RUN token data (decoded)
 * Multiple consecutive RAW chunks coalesced
 */
typedef struct SSKRawRunData {
    uint16_t run_len;               // Number of full chunks
    uint8_t  has_final_incomplete;  // 0/1: does run include final incomplete chunk?
    uint8_t  final_nbits;           // If has_final_incomplete: 1..63
    uint32_t bits_off;              // Offset to run_len*64 + optional final_nbits bits
} SSKRawRunData;

/**
 * Decoded token (part of MIX segment)
 */
typedef struct SSKToken {
    uint8_t  kind;            // TokenKind (ENUM, RAW, RAW_RUN)
    uint8_t  dirty;           // Decoded-only dirty bit (0/1); never encoded
    uint32_t popcount;        // Decoded-only fast path for membership count
    uint32_t data_off;        // Offset to SSKEnumData, SSKRawData, or SSKRawRunData
} SSKToken;

/**
 * MIX segment payload (decoded)
 */
typedef struct SSKMixPayload {
    uint16_t last_chunk_nbits;  // 1..64: size of final chunk
    uint32_t token_count;       // Number of tokens
    uint32_t tokens_off;        // Offset to SSKToken array
} SSKMixPayload;

// Note: SSKSegment, SSKPartition, SSKDecoded are defined in ssk_decoded.h

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

/**
 * Allocate decoded SSK with initial capacity.
 * Memory layout calculated during decoding; may need realloc as decoding progresses.
 */
SSKDecoded *ssk_alloc_decoded(uint16_t format_version, size_t initial_size);

/**
 * Grow decoded SSK memory if needed.
 * Returns new pointer (may differ from old due to realloc).
 */
SSKDecoded *ssk_grow_decoded(SSKDecoded *ssk, size_t additional_size);

/**
 * Free decoded SSK and all associated memory.
 */
void ssk_free_decoded(SSKDecoded *ssk);

// Note: Use decoded_partition() and partition_segment() from ssk_decoded.h
// for traversing the decoded structure.

// ============================================================================
// RANK/UNRANK (Combinadic Encoding)
// ============================================================================

/**
 * Precomputed rank_bits table: ceil(log2(C(n, k)))
 * Indexed as ssk_rank_bits[k][n] for k=0..18, n=0..64 (k-major layout)
 * 
 * Must be initialized at module load via ssk_combinadic_init().
 */
extern uint8_t ssk_rank_bits[19][65];

/**
 * Initialize combinadic tables (call once at extension load)
 * Populates ssk_rank_bits and internal binomial tables.
 */
void ssk_combinadic_init(void);

/**
 * Compute combinadic rank for a chunk
 * 
 * @param bits Chunk bits (n bits, MSB-first)
 * @param n Chunk size (1..64)
 * @param k Popcount (0..K_CHUNK_ENUM_MAX)
 * @return Rank (0-based index in combinadic ordering)
 */
uint64_t ssk_combinadic_rank(uint64_t bits, uint8_t n, uint8_t k);

/**
 * Unrank combinadic encoding back to chunk bits
 * 
 * @param rank Combinadic rank
 * @param n Chunk size (1..64)
 * @param k Popcount (0..K_CHUNK_ENUM_MAX)
 * @return Chunk bits (n bits, MSB-first)
 */
uint64_t ssk_combinadic_unrank(uint64_t rank, uint8_t n, uint8_t k);

// ============================================================================
// CANON VALIDATION
// ============================================================================

/**
 * Validate that an encoded SSK satisfies all canonicity requirements.
 * 
 * This function performs comprehensive validation:
 *   - Partitions strictly ascending
 *   - Segments strictly ascending within partitions
 *   - CDU values minimally encoded
 *   - Segment kinds match deterministic rules
 *   - Token kinds match deterministic rules  
 *   - RAW tokens properly coalesced
 *   - All offsets in-bounds
 *   - rare_bit == (~dominant_bit & 1)
 * 
 * @param encoded_bytes Encoded SSK bytes
 * @param len Length of encoded data
 * @param format_spec Format specification (defines canon rules)
 * @return true if canonical, false if any violation detected
 * 
 * Note: Decoder should call this during decode and reject non-canonical
 * encodings with ERRCODE_DATA_CORRUPTED.
 */
bool ssk_validate_canon(const uint8_t *encoded_bytes, size_t len, 
                        const SSKFormatSpec *format_spec);

/**
 * Get format specification for a given version.
 * Returns NULL if version is not supported.
 */
const SSKFormatSpec *ssk_get_format_spec(uint16_t version);

/**
 * Check if a CDU encoded value is minimal (canonical).
 * CDU encoding is canonical by design, so this always returns true.
 * 
 * @param encoded_bytes Start of CDU encoded value
 * @param cdu_type CDU type for this field
 * @param value The decoded value
 * @return true (CDU is always minimal)
 */
bool ssk_cdu_is_minimal(const uint8_t *encoded_bytes, 
                        CDUtype cdu_type,
                        uint64_t value);

#endif // SSK_FORMAT_H