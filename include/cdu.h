/*
 * include/cdu.h
 *
 * Canonical Data Unit (CDU) encoding/decoding.
 *
 * CDU provides variable-length and fixed-length encoding.
 * Simple single-pass encode/decode with minimal logic.
 */

#ifndef SSK_CDU_H
#define SSK_CDU_H

#include <stdint.h>
#include <stddef.h>

/**
 * CDU Parameters - Describes encoding for a value.
 *
 * For fixed encoding: write exactly base_bits, fixed=1.
 * For variable encoding: first segment of first bits, then step_size bits
 * per continuation segment, up to max_steps. Each segment except the last
 * is followed by a continuation bit (1=more segments, 0=done).
 * first=0 is special: value 0 fits in first step with just continuation=0.
 */
typedef struct {
    uint8_t  base_bits;        /* Field width in bits */
    uint8_t  first : 7;        /* First segment bits (0 means special case) */
    uint8_t  fixed : 1;        /* 1 for fixed-length, 0 for variable */
    uint8_t  step_size;        /* Subsequent segment bits */
    uint8_t  max_steps;        /* Total allowed segments */
    uint8_t  steps[8];         /* sequnce of step lengths */
} CDUParam;

/**
 * CDU Type Enumeration.
 */
typedef enum {
    CDU_TYPE_DEFAULT          =  0,
    CDU_TYPE_RAW1             =  1,
    CDU_TYPE_RAW2             =  2,
    CDU_TYPE_RAW64            =  3,
    CDU_TYPE_SMALL_INT        =  4,
    CDU_TYPE_MEDIUM_INT       =  5,
    CDU_TYPE_LARGE_INT        =  6,
    CDU_TYPE_INITIAL_DELTA    =  7,
    CDU_TYPE_ENUM_K           =  8,
    CDU_TYPE_ENUM_RANK        =  9,
    CDU_TYPE_ENUM_COMBINED    = 10,
    CDU_NUM_SUBTYPES
} CDUtype;

extern const CDUParam cdu_params[CDU_NUM_SUBTYPES];

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
size_t cdu_encode(uint64_t value, CDUtype type, uint8_t *buf, size_t bit_pos);

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

static inline uint64_t 
cdu_vl_n_decode_body(unsigned int n, uint64_t *valuep, uint64_t buffer, int * shiftp) {
    *valuep |= (buffer & (1ULL<<n)-1) << *shiftp; 
    *shiftp += n; 
    return buffer & 1ULL<<n;
}

static inline uint64_t
cdu_vl_n_encode_body(unsigned int n, uint64_t *encodedp, uint64_t value, int *bits_usedp) {
   uint64_t morebit = 1ULL << n;
   *encodedp |= (value | ((value > morebit-1) ? morebit : 0)) << *bits_usedp; 
   *bits_usedp += n + 1; 
   return value >> n;
}

#endif /* SSK_CDU_H */
