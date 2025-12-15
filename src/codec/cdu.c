/*
 * src/codec/cdu.c
 *
 * Canonical Data Unit (CDU) encoding and decoding.
 *
 * CDU is a variable-length encoding with fixed-length fallback.
 * Single-pass encode and decode, no helper functions, minimal logic.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "cdu.h"
#include "bitblocks.h"

/* CDU parameter table: one entry per type */
const CDUParam cdu_params[CDU_NUM_SUBTYPES] = {
    [CDU_TYPE_DEFAULT]       = {.base_bits = 16, .first =  0, .fixed = 0, .step_size =  3, .max_steps = 6, .steps = {0, 3, 3, 3, 3, 4}},
    [CDU_TYPE_RAW1]          = {.base_bits =  1, .first =  0, .fixed = 1, .step_size =  0, .max_steps = 0},
    [CDU_TYPE_RAW2]          = {.base_bits =  2, .first =  0, .fixed = 1, .step_size =  0, .max_steps = 0},
    [CDU_TYPE_RAW64]         = {.base_bits = 64, .first =  0, .fixed = 1, .step_size =  0, .max_steps = 0},
    [CDU_TYPE_SMALL_INT]     = {.base_bits = 16, .first =  4, .fixed = 0, .step_size =  6, .max_steps = 3, .steps = {4, 6, 6}},
    [CDU_TYPE_MEDIUM_INT]    = {.base_bits = 20, .first =  6, .fixed = 0, .step_size =  7, .max_steps = 3, .steps = {6, 7, 7}},
    [CDU_TYPE_LARGE_INT]     = {.base_bits = 32, .first =  5, .fixed = 0, .step_size =  7, .max_steps = 4, .steps = {5, 8, 8, 11}},
    [CDU_TYPE_ENUM_COMBINED] = {.base_bits = 56, .first =  8, .fixed = 0, .step_size = 12, .max_steps = 4, .steps = {8, 12, 14, 18}},
    [CDU_TYPE_ENUM_K]        = {.base_bits = 32, .first =  4, .fixed = 0, .step_size =  5, .max_steps = 7, .steps = {4, 5, 6, 7, 8, 9, 10}},
    [CDU_TYPE_ENUM_RANK]     = {.base_bits = 48, .first =  8, .fixed = 0, .step_size = 12, .max_steps = 4, .steps = {8, 12, 14, 18}},
    [CDU_TYPE_INITIAL_DELTA] = {.base_bits = 32, .first =  3, .fixed = 0, .step_size =  8, .max_steps = 4, .steps = {3, 8, 8, 13}}
};

/*
 * cdu_encode -- Encode a value using CDU (fixed or variable).
 *
 * Fixed encoding: write base_bits directly, return base_bits.
 * Variable encoding: single pass, extract first bits with continuation,
 * then step_size bits per step with continuation, until value exhausted
 * or max_steps reached. Final segment may be remainder (variable width).
 * Return total bits written.
 * 
 * UPDATE: In recognition of LEB128  performance, CDU will adopt,
 * as its own, the core approach used by LEB128, adapted to CDU's parameterization.
 * 
 * while (value >= 128) {
 *   write_byte((value & 127) | 128);
 *   value >>= 7;
 * }
 * write_byte(value);
 * 
 */
size_t
cdu_encode(uint64_t value, CDUtype type, uint8_t *buf, size_t bit_pos)
{
    const CDUParam *p = &cdu_params[type];
    int bits_used  = 0;

    if (p->fixed) {      // Fixed encoding: write base_bits, one block, no continuation bit overhead

        bb_place_fl_block(buf, bit_pos, value, p->base_bits);               // write the raw block, one time!
        bits_used = p->base_bits;                                       // set bits_used for return value              

    } else if (!value) {  // Variable encoding: simplified

      bb_place_vl_encoding(buf, bit_pos, value, p->first + 1);          // write the zero block with 0 as continuation
      bits_used = p->first + 1;                                         // set bits_used for return value

    } else if (value) {  // Variable encoding, write value in steps, save space by stopping early
    
      //********************************************************* CORE variable length encoding algorithm starts here
      const uint8_t *steps      = p->steps;
      int      step_i     = 0;                                          // step index

      uint8_t  step_len   = steps[step_i];
      uint64_t morebit;
      uint64_t encoded    = 0ULL;                                       // accumulator for encoded value
      do {
        if (step_len != steps[step_i]) {
          step_len  = steps[step_i];
          morebit = 1ULL << step_len;
        }
        encoded |= (value | ((value > morebit-1) ? morebit : 0)) << bits_used; 
        bits_used += step_len + 1; 
        value >>= step_len;
        step_i++;
      } while (value && step_i < p->max_steps);

      bb_place_vl_encoding(buf, bit_pos, encoded, bits_used);
      //********************************************************* CORE variable length encoding algorithm complete

    }

    return bits_used;
}
/*
 * cdu_decode -- Decode a CDU encoded value (fixed or variable).
 *
 * Mirrors encoder: single-pass decode, LSB-first accumulation.
 * Return bits consumed, or 0 on error.
 * 
 * UPDATE: In recognition of LEB128  performance, CDU will adopt,
 * as its own, the core approach used by LEB128, adapted to CDU's parameterization.
 * 
 * result = 0;
 * shift = 0;
 * do {
 *     byte = read_byte();
 *     result |= (byte & 127) << shift;
 *     shift += 7;
 * } while (byte & 128);
 * 
 */
size_t
cdu_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
           CDUtype type, uint64_t *value_out)
{
    const CDUParam *p = &cdu_params[type];
    size_t bits_used = 0;

    if (p->fixed) {      // Fixed decoding: read base_bits directly
        *value_out = bb_fetch_fl_block(buf, bit_pos, p->base_bits);
        bits_used += p->base_bits;
    } else {             // Variable decoding: read in steps until continuation == 0 (sentinel semantics) 
      int      step         = 0;
      uint64_t buffer       = bb_fetch_vl_block(buf, bit_pos);                       // preload buffer, done once;
      uint64_t value        = 0ULL;
      int      shift        = 0;
      uint64_t continued    = 0;
      //********************************************************* CORE variable length decoding algorithm starts here
      do {
          bits_used += p->steps[step] + 1;                                           // track bits used
          continued = cdu_vl_n_decode_body(p->steps[step], &value, buffer, &shift);  // decode step
          step++;
          buffer >>= p->steps[step-1] + 1;                                           // advance buffer for next step
      } while (continued && p->steps[step]);                                         // while continuation bit set, protect against invalid data

      if (continued) { return 0; } // error: ran out of steps but continuation still set

      *value_out = value;
      //********************************************************* CORE variable length decoding algorithm complete

    }
    return bits_used;
}

