/*
 * Copyright (c) 2020-2025 Marthin Laubscher
 * All rights reserved. See LICENSE for details.
 */

#ifndef SSK_CDU_H
#define SSK_CDU_H

#ifdef TRIVIAL
#define SAFE_BLOCKS
/*
 * CDU works the same in TRIVIAL and NON TRIVIAL cases.
 * with one non-funcitonal exception: SAFE_BLOCKS is defined in TRIVIAL
 * to enable extra safety checks in bitblock accesses, and the original
 * NON TRIVIAL code still runs without it as it does not receive the block
 * lengths as parameters yet. 
 * When TRIVIAL code if being ported back to the NON TRIVIAL block, the
 * intention would be to keep SAFE_BLOCKS enabled there as well, and 
 * implement the upstream calculation of the buffer bits as it should be.
 */

#else // NON TRIVIAL
// #define SAFE_BLOCKS
#endif // (NON) TRIVIAL


/*
 * include/cdu.h
 *
 * Canonical Data Unit (CDU) encoding/decoding.
 *
 * CONCERN: Persistence Layer - inner codec (IMP/A1, IMP/A3)
 *
 * CDU is the low-level codec for encoding variable-length and fixed-length
 * data units within Format 0. It handles:
 * - Partition deltas, segment counts, lengths
 * - Combinadic k and rank values
 * - Raw bit blocks
 *
 * CDU maintains canonicality through strict minimality rules. Any value
 * encodes to exactly one byte sequence.
 *
 * CDU replaced VLQ-P (2-bit continuation) for simpler code and better performance.
 *
 * RELATIONSHIP TO FORMAT 0:
 * Format 0 defines WHAT data to encode (hierarchy structure).
 * CDU defines HOW to encode each data unit (bit packing).
 */

#include <stdint.h>
#include <stddef.h>

/**
 * CDU Parameters - Describes encoding for a value.
 *
 * For fixed encoding: write exactly base_bits, fixed=1.
 * For variable encoding:
 *   - Step 0: first bits (with continuation bit)
 *   - Steps 1 to middle_steps: step_size bits each (with continuation bit)
 *   - Final step: remainder_bits = base_bits - first - (middle_steps * step_size)
 *   - CANONICAL RULE: final step (remainder) MUST be >= step_size to ensure
 *     no ambiguity in encoding and no waste from tiny remainder steps.
 *   - def_steps: Total defined steps = 1 + middle_steps + 1
 *   - All steps have continuation bits; sequence stops when value fits in current step
 * Special case: first=0 means value 0 requires only continuation=0 sentinel.
 */

#define CDU_MAX_TYPES     11
#define CDU_MAX_STEPS  8
#define CDU_MAX_MAX_STEPS 8

typedef struct {
    uint8_t  base_bits;                 // Field width in bits
    uint8_t  first;                     // First step bits
    uint8_t  fixed;                     // 1 for fixed-length, 0 for variable
    uint8_t  step_size;                 // Uniform middle step bits
    uint8_t  max_mids;                  // Maximum middle steps (limit; actual may be less)
    uint8_t  middle_steps;              // Actual number of middle steps after init (output)
    uint8_t  def_steps;                 // Total defined steps: first + middle + remainder (output)
    uint8_t  steps[CDU_MAX_MAX_STEPS];  // Step lengths [0..def_steps-1] valid; [def_steps] sentinel
    int64_t  conti;                     // CONTInuation bits in the encoded format
    int64_t  bmask;                     // value mask (binary value)
    int64_t  emask;                     // value mask (encoded value)
    int64_t  masks[CDU_MAX_MAX_STEPS];  // Precomputed masks per type
} CDUParam;

/**
 * CDU Type Enumeration - Variable-length types first, then fixed-length.
 * Values auto-assigned by compiler; explicit assignment of CDU_TYPE_RAW1
 * serves as verification that CDU_MAX_VL_TYPES boundary is correct.
 */
typedef enum {
    /* Variable-length types (auto-assign 0..CDU_MAX_VL_TYPES-1) */
    CDU_TYPE_DEFAULT,
    CDU_TYPE_SMALL_INT,
    CDU_TYPE_MEDIUM_INT,
    CDU_TYPE_LARGE_INT,
    CDU_TYPE_ENUM_K,
    CDU_TYPE_ENUM_RANK,
    CDU_TYPE_INITIAL_DELTA,
    CDU_MAX_VL_TYPES,           /* Sentinel: number of variable-length types */
    
    /* Fixed-length types (start at CDU_MAX_VL_TYPES) */
    CDU_TYPE_RAW1 = CDU_MAX_VL_TYPES,
    CDU_TYPE_RAW2,
    CDU_TYPE_RAW64,
    CDU_TYPE_ENUM_COMBINED,
    
    CDU_TYPE_COUNT              /* Sentinel: total number of types (for array sizing) */
} CDUtype;

/* Verify constants for debugging */
#define CDU_NUM_SUBTYPES CDU_TYPE_COUNT

/**
 * Initialize CDU parameters. Call once at startup.
 */
void cdu_init(void);

extern CDUParam cdu_params[CDU_NUM_SUBTYPES];

/* Convenience macros */
#define CDU_DEFAULT         (&cdu_params[CDU_TYPE_DEFAULT])
#define CDU_RAW1            (&cdu_params[CDU_TYPE_RAW1])
#define CDU_RAW2            (&cdu_params[CDU_TYPE_RAW2])
#define CDU_RAW64           (&cdu_params[CDU_TYPE_RAW64])
#define CDU_SMALL_INT       (&cdu_params[CDU_TYPE_SMALL_INT])
#define CDU_MEDIUM_INT      (&cdu_params[CDU_TYPE_MEDIUM_INT])
#define CDU_LARGE_INT       (&cdu_params[CDU_TYPE_LARGE_INT])
#define CDU_INITIAL_DELTA   (&cdu_params[CDU_TYPE_INITIAL_DELTA])
#define CDU_ENUM_K          (&cdu_params[CDU_TYPE_ENUM_K])
#define CDU_ENUM_RANK       (&cdu_params[CDU_TYPE_ENUM_RANK])
#define CDU_ENUM_COMBINED   (&cdu_params[CDU_TYPE_ENUM_COMBINED])

#define CDU_FIXED_MAXBYTES        9  // for 64-bit block with 1 overhang byte for alignment
#define CDU_VARIABLE_MAXBYTES     8  // max 36 bits (LARGE_INT/INITIAL_DELTA) + safety margin

// NOTE: Cannot have compile-time assertion checking cdu_params array contents
// because C doesn't allow compile-time evaluation of runtime data structures.
// The 36-bit maximum is manually verified and documented here.
// If CDUParam for variable-length types ever exceed 64 bits overall,
// special handling must be added to bb_fetch_vl_block and related functions.

/**
 * cdu_encode - Encode a value using CDU.
 *
 * @param value    Value to encode
 * @param type     CDU type
 * @param buf      Output buffer
 * @param bit_pos  Starting bit position
 * @return Bits written
 */
#ifdef SAFE_BLOCKS
size_t cdu_encode(uint64_t value, CDUtype type, uint8_t *buf, size_t bit_pos, size_t buf_bits);
#else
size_t cdu_encode(uint64_t value, CDUtype type, uint8_t *buf, size_t bit_pos);
#endif //SAFE_BLOCKS

/**
 * cdu_decode - Decode a CDU encoded value.
 *
 * @param buf        Input buffer
 * @param bit_pos    Starting bit position
 * @param buf_bits   Total bits available
 * @param type       CDU type
 * @param value_out  Output: decoded value
 * @return Bits consumed, or 0 on error
 */
size_t cdu_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                  CDUtype type, uint64_t *value_out);

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#endif /* SSK_CDU_H */
