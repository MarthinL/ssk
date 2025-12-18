/*
 * Copyright (c) 2020-2025 Marthin Laubscher
 * All rights reserved. See LICENSE for details.
 */

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
/* Variables use max_mids as input limit; actual middle_steps calculated at init */
CDUParam cdu_params[CDU_NUM_SUBTYPES] = {
    //                           base_bits, first, fixed, step_size, max_mids
    [CDU_TYPE_DEFAULT]       = {        16,     0,     0,         3,         5},  // rem>=3
    [CDU_TYPE_SMALL_INT]     = {        32,     4,     0,         6,         2},  // rem>=6
    [CDU_TYPE_MEDIUM_INT]    = {        32,     6,     0,         7,         2},  // rem>=7
    [CDU_TYPE_LARGE_INT]     = {        32,     5,     0,         7,         2},  // rem>=7
    [CDU_TYPE_ENUM_K]        = {        32,     4,     0,         5,         4},  // rem>=5
    [CDU_TYPE_ENUM_RANK]     = {        48,     8,     0,        12,         3},  // rem>=12
    [CDU_TYPE_INITIAL_DELTA] = {        32,     3,     0,         8,         2},  // rem>=8
    /* Fixed-length types */
    [CDU_TYPE_RAW1]          = {         1,     0,     1,         0,         0},
    [CDU_TYPE_RAW2]          = {         2,     0,     1,         0,         0},
    [CDU_TYPE_RAW64]         = {        64,     0,     1,         0,         0},
    [CDU_TYPE_ENUM_COMBINED] = {        48,     8,     1,         0,         0}
};



// CDUParam cdu_params[CDU_MAX_TYPES];

/* Initialize CDU parameters */
void cdu_init(void) {
    /* PHASE 1: VALIDATE ALL TYPES BEFORE MODIFYING MEMORY */
    /* ===================================================== */
    uint8_t max_def_steps = 0;
    
    for (int i = 0; i < CDU_NUM_SUBTYPES; i++) {
        CDUParam * p = &cdu_params[i];
        
        /* Skip types with no definition */
        if (p->base_bits == 0) continue;
        
        /* Fixed-length types: just validate base_bits */
        if (p->fixed) {
            assert(p->base_bits <= 64 && "Fixed type exceeds 64 bits");
            continue;
        }
        
        /* Variable-length types: validate parameter consistency */
        assert(p->first < 64 && "first must be 0..63");
        assert(p->step_size > 0 && p->step_size < 64 && "step_size must be 1..63");
        assert(p->max_mids <= CDU_MAX_MAX_STEPS - 2 && "max_mids too large (need room for first+remainder)");
        assert(p->base_bits >= p->first + p->step_size && "base_bits must be >= first + step_size");
        
        /* Calculate actual middle_steps: as many as fit while final step >= step_size */
        /* CANONICAL RULE (from SPECIFICATION.md):
         * The final step (remainder) MUST be >= step_size.
         * This ensures:
         *   1. Canonality: No ambiguity in how to encode a value
         *   2. No waste: Prevents tiny final steps (1-2 bits) that waste space
         *   3. Deterministic decoding: Unambiguous reconstruction of value
         *
         * If remainder < step_size, decrement middle_steps until valid.
         */
        uint8_t bits_after_first = p->base_bits - p->first;
        uint8_t middle_steps = bits_after_first / p->step_size;
        if (middle_steps > p->max_mids) middle_steps = p->max_mids;
        
        /* Adjust downward if remainder would be < step_size */
        while (middle_steps > 0) {
            uint8_t remainder = bits_after_first - (middle_steps * p->step_size);
            if (remainder >= p->step_size) break;
            middle_steps--;
        }
        
        /* Final sanity check: remainder must be >= step_size */
        uint8_t remainder = bits_after_first - (middle_steps * p->step_size);
        assert(remainder >= p->step_size && "Remainder calculation failed");
        
        /* Calculate def_steps and validate it fits */
        uint8_t def_steps = 1 + middle_steps + 1;  // first + middles + remainder
        assert(def_steps <= CDU_MAX_MAX_STEPS && "def_steps exceeds CDU_MAX_MAX_STEPS");
        
        /* Validate encoded size fits in 64 bits */
        int total_bits = 0;
        for (int s = 0; s < middle_steps; s++) {
            total_bits += p->step_size + 1;  /* middle step + continuation */
        }
        total_bits += p->first + 1;           /* first step + continuation */
        total_bits += remainder + 1;          /* remainder step + continuation */
        assert(total_bits <= 64 && "Encoded value exceeds 64 bits");
        
        /* Track max for final validation */
        if (def_steps > max_def_steps) max_def_steps = def_steps;
    }
    
    /* Validate CDU_MAX_MAX_STEPS is sufficient */
    assert(max_def_steps <= CDU_MAX_MAX_STEPS && 
           "CDU_MAX_MAX_STEPS too small; increase it");
    
    /* PHASE 2: INITIALIZE PARAMETERS WITH VALIDATED DATA */
    /* ===================================================== */
    for (int i = 0; i < CDU_NUM_SUBTYPES; i++) {
        CDUParam * p = &cdu_params[i];
        
        /* Skip types with no definition */
        if (p->base_bits == 0) continue;
        
        /* Fixed-length types: masks only */
        if (p->fixed) {
            p->bmask = (1LL << p->base_bits) - 1;
            p->emask = p->bmask;  /* No continuation bits */
            continue;
        }
        
        /* Variable-length types: build steps array and masks */
        uint8_t bits_after_first = p->base_bits - p->first;
        uint8_t middle_steps = bits_after_first / p->step_size;
        if (middle_steps > p->max_mids) middle_steps = p->max_mids;
        
        /* Adjust downward if remainder < step_size */
        while (middle_steps > 0) {
            uint8_t remainder = bits_after_first - (middle_steps * p->step_size);
            if (remainder >= p->step_size) break;
            middle_steps--;
        }
        
        uint8_t remainder = bits_after_first - (middle_steps * p->step_size);
        
        /* Store actual middle_steps for encoder/decoder */
        p->middle_steps = middle_steps;
        p->def_steps = 1 + middle_steps + 1;
        
        /* Build steps array: [first, ...middles..., remainder] */
        p->steps[0] = p->first;
        for (int s = 1; s <= middle_steps; s++) {
            p->steps[s] = p->step_size;
        }
        p->steps[middle_steps + 1] = remainder;
        
        /* Build continuation bit mask */
        int cpos = 0;
        p->conti = 0;
        for (int s = 0; s < p->def_steps; s++) {
            p->conti |= (1LL << (cpos + p->steps[s]));
            cpos += p->steps[s] + 1;  /* +1 for continuation bit */
        }
        
        /* Build value and encoded masks */
        p->bmask = (1LL << p->base_bits) - 1;
        p->emask = (1LL << (p->base_bits + p->def_steps)) - 1;
    }
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

