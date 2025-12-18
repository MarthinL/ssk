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
#include <assert.h>
#include "cdu.h"
#include "bitblocks.h"

/* CDU parameter table: one entry per type */
CDUParam cdu_params[CDU_MAX_TYPES] = {
    //                           base_bits, first, fixed, step_size, max_steps
    [CDU_TYPE_DEFAULT]       = {        16,     0,     0,         3,         6},
    [CDU_TYPE_RAW1]          = {         1,     0,     1,         0,         0},
    [CDU_TYPE_RAW2]          = {         2,     0,     1,         0,         0},
    [CDU_TYPE_RAW64]         = {        64,     0,     1,         0,         0},
    [CDU_TYPE_SMALL_INT]     = {        16,     4,     0,         6,         3},
    [CDU_TYPE_MEDIUM_INT]    = {        20,     6,     0,         7,         3},
    [CDU_TYPE_LARGE_INT]     = {        32,     5,     0,         7,         4},
    [CDU_TYPE_ENUM_COMBINED] = {        56,     8,     0,        12,         4},
    [CDU_TYPE_ENUM_K]        = {        32,     4,     0,         5,         7},
    [CDU_TYPE_ENUM_RANK]     = {        48,     8,     0,        12,         4},
    [CDU_TYPE_INITIAL_DELTA] = {        32,     3,     0,         8,         4}
};



// CDUParam cdu_params[CDU_MAX_TYPES];

/* Initialize CDU parameters */
void cdu_init(void) {
    for (int i = 0; i < CDU_MAX_TYPES; i++) {
        CDUParam * p       = &cdu_params[i];
        if (!p->fixed && p-> base_bits > 0) {
            int        cpos    = 0;
            /* Build steps array for variable-length types */
            for (int s = 0; s < p->max_steps; s++) {
              p->steps[s] = s ? p->first : p->step_size;
              p->conti |= (1LL << (cpos + p->steps[s]));
              cpos += p->steps[s] + 1;
            }

            assert(cpos == p->base_bits + p->max_steps);  // Sanity check
            assert(cpos <= 64);

            p->bmask = (1LL << p->base_bits) - 1;
            p->emask = (1LL << (p->base_bits + p->max_steps)) - 1;

        }
    }
    
    // cdu_params[CDU_TYPE_DEFAULT]         = (CDUParam){.base_bits = 16, .first = 0, .fixed = 0, .step_size = 3, .max_steps = 6};
    // cdu_params[CDU_TYPE_RAW1]            = (CDUParam){.base_bits = 1, .first = 0, .fixed = 1, .step_size = 0, .max_steps = 0};
    // cdu_params[CDU_TYPE_RAW2]            = (CDUParam){.base_bits = 2, .first = 0, .fixed = 1, .step_size = 0, .max_steps = 0};
    // cdu_params[CDU_TYPE_RAW64]           = (CDUParam){.base_bits = 64, .first = 0, .fixed = 1, .step_size = 0, .max_steps = 0};
    // cdu_params[CDU_TYPE_SMALL_INT]       = (CDUParam){.base_bits = 16, .first = 4, .fixed = 0, .step_size = 6, .max_steps = 3};
    // cdu_params[CDU_TYPE_MEDIUM_INT]      = (CDUParam){.base_bits = 20, .first = 6, .fixed = 0, .step_size = 7, .max_steps = 3};
    // cdu_params[CDU_TYPE_LARGE_INT]       = (CDUParam){.base_bits = 32, .first = 5, .fixed = 0, .step_size = 7, .max_steps = 4};
    // cdu_params[CDU_TYPE_ENUM_COMBINED]   = (CDUParam){.base_bits = 56, .first = 8, .fixed = 0, .step_size = 12, .max_steps = 4};
    // cdu_params[CDU_TYPE_ENUM_K]          = (CDUParam){.base_bits = 32, .first = 4, .fixed = 0, .step_size = 5, .max_steps = 7};
    // cdu_params[CDU_TYPE_ENUM_RANK]       = (CDUParam){.base_bits = 48, .first = 8, .fixed = 0, .step_size = 12, .max_steps = 4};
    // cdu_params[CDU_TYPE_INITIAL_DELTA]   = (CDUParam){.base_bits = 32, .first = 3, .fixed = 0, .step_size = 8, .max_steps = 4};
}

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
 */
size_t
cdu_encode(uint64_t value, CDUtype type, uint8_t *buf, size_t bit_pos)
{
    const CDUParam *p = &cdu_params[type];
    int bits_used  = 0;

    if (p->fixed) {      // Fixed encoding: write base_bits, one block, no continuation bit overhead

        bb_place_fl_block(buf, bit_pos, value, p->base_bits);   // write the raw block, one time!
        bits_used = p->base_bits;                               // set bits_used for return value              
        assert(bits_used <= 64);

    } else  {  // Variable encoding, write value in steps, save space by stopping early
      uint8_t *step       = (uint8_t *)p->steps;
      uint64_t morebit    = 1ULL << *step;
      uint64_t encoded    = 0ULL;                               // accumulator for encoded value
      
      //************************************** CORE variable length encoding algorithm starts here
      while (value >= morebit) {                                
        encoded |= ( (value & (morebit-1)) | morebit ) << bits_used;
        bits_used += *step + 1;
        value >>= *step;
        morebit = 1ULL << *(++step);
      }
      encoded |= (value) << bits_used;
      bits_used += *step + 1;
      //************************************** CORE variable length encoding algorithm complete

      assert(bits_used <= 64);
      bb_place_vl_encoding(buf, bit_pos, encoded, bits_used);
    }

    return bits_used;
}
/*
 * cdu_decode -- Decode a CDU encoded value (fixed or variable).
 *
 * Mirrors encoder: single-pass decode, LSB-first accumulation.
 * Return bits consumed, or 0 on error.
 *  * 
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
      int         shift     = 0;
      uint8_t   * step      = (uint8_t *)p->steps;
      uint64_t    morebit   = 1ULL << *step;
      uint64_t    buffer    = bb_fetch_vl_block(buf, bit_pos);  // preload buffer, done once;
      uint64_t    value     = 0ULL;

      //************************************** CORE variable length decoding algorithm starts here

      while (buffer & morebit) {                                // while continuation bit set
          value |= (buffer & (morebit - 1)) << shift;             // write step into value
          bits_used += *step + 1;                                 // track bits used 
          buffer >>= *step + 1;                                   // advance buffer for next step
          shift += *step;
          morebit      = (1ULL << *(++step));
      }
      value |= (buffer & (morebit - 1)) << shift;               // write step into value
      bits_used += *step + 1;                                   // track bits used 

      //************************************** CORE variable length decoding algorithm complete

      *value_out = value;


    }
    return bits_used;
}

