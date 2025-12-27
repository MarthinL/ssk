/*
 * Copyright (c) 2020-2025 Marthin Laubscher
 * All rights reserved. See LICENSE for details.
 */

#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "lib/stringinfo.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "ssk.h"
#include "cdu.h"


PG_MODULE_MAGIC;

/* Extension initialization */
void _PG_init(void) {
  cdu_init();              /* CDU parameters are usd by both TRIVIAL and NON TRIVIAL */
#ifndef TRIVIAL
    ssk_combinadic_init();   /* ONLY NON-TRIVIAL Needs to initialize combinadic tables */
#endif
}

/* All function declarations (same for both TRIVIAL and NON-TRIVIAL) */
PG_FUNCTION_INFO_V1(ssk_version);
PG_FUNCTION_INFO_V1(ssk_in);
PG_FUNCTION_INFO_V1(ssk_out);
PG_FUNCTION_INFO_V1(ssk_new);
PG_FUNCTION_INFO_V1(ssk_new_single);
PG_FUNCTION_INFO_V1(ssk_add);
PG_FUNCTION_INFO_V1(ssk_remove);
PG_FUNCTION_INFO_V1(ssk_contains);
PG_FUNCTION_INFO_V1(ssk_is_contained);
PG_FUNCTION_INFO_V1(ssk_union);
PG_FUNCTION_INFO_V1(ssk_intersect);
PG_FUNCTION_INFO_V1(ssk_except);
PG_FUNCTION_INFO_V1(ssk_cardinality);
PG_FUNCTION_INFO_V1(ssk_is_empty);
PG_FUNCTION_INFO_V1(ssk_unnest);
PG_FUNCTION_INFO_V1(ssk_from_array);
PG_FUNCTION_INFO_V1(ssk_to_array);
PG_FUNCTION_INFO_V1(ssk_cmp);
PG_FUNCTION_INFO_V1(ssk_length);

/* ============================================================================
 * SSK UDT Function Implementations
 * 
 * Shared PostgreSQL boilerplate with algorithm-specific code in inner #ifdef blocks
 * ============================================================================
 */

Datum
ssk_version(PG_FUNCTION_ARGS)
{
#ifdef TRIVIAL
	PG_RETURN_TEXT_P(cstring_to_text("0.1 (Trivial)"));
#else // NON TRIVIAL
	PG_RETURN_TEXT_P(cstring_to_text("1.0"));
#endif // (NON) TRIVIAL
}

/* ssk_in - wrapper to byteain (identical external representation with different type names) */
Datum
ssk_in(PG_FUNCTION_ARGS)
{
    return byteain(fcinfo);
}

/* ssk_out - wrapper to byteaout (identical external representation with different type names) */
Datum
ssk_out(PG_FUNCTION_ARGS)
{
    return byteaout(fcinfo);
}

/* Constructor: ssk() - returns empty SSK (zero bitvector) */

Datum
ssk_new(PG_FUNCTION_ARGS)
{
#ifdef TRIVIAL
    uint64_t bits = 0;
#else
    /* NON-TRIVIAL: create empty hierarchical structure */
    uint64_t bits = 0;
#endif
    
    bytea *result = (bytea *) palloc(VARHDRSZ + 8);
    SET_VARSIZE(result, VARHDRSZ + 8);
    memcpy(VARDATA(result), &bits, 8);
    PG_RETURN_BYTEA_P(result);
}

/* Constructor: ssk(bigint) - returns SSK with single element (set bit at position id-1) */

Datum
ssk_new_single(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    
#ifdef TRIVIAL
    uint64_t bits = 0;
    /* IDs 1-64 map to bits 0-63 */
    if (id >= 1 && id <= 64) {
        bits = 1UL << (id - 1);
    }
#else
    /* NON-TRIVIAL: create single-element hierarchical structure */
    uint64_t bits = 0;
#endif
    
    bytea *result = (bytea *) palloc(VARHDRSZ + 8);
    SET_VARSIZE(result, VARHDRSZ + 8);
    memcpy(VARDATA(result), &bits, 8);
    PG_RETURN_BYTEA_P(result);
}

/* ssk_add(ssk, bigint) - set bit at position id-1 */

Datum
ssk_add(PG_FUNCTION_ARGS)
{
    bytea *input = PG_GETARG_BYTEA_PP(0);
    int64 id = PG_GETARG_INT64(1);
    
#ifdef TRIVIAL
    uint64_t bits = 0;
    if (VARSIZE_ANY_EXHDR(input) >= 8)
        memcpy(&bits, VARDATA_ANY(input), 8);
    
    if (id >= 1 && id <= 64)
        bits |= (1UL << (id - 1));
#else
    /* NON-TRIVIAL: add element */
    uint64_t bits = 0;
#endif
    
    bytea *result = (bytea *) palloc(VARHDRSZ + 8);
    SET_VARSIZE(result, VARHDRSZ + 8);
    memcpy(VARDATA(result), &bits, 8);
    PG_RETURN_BYTEA_P(result);
}

/* ssk_remove(ssk, bigint) - clear bit at position id-1 */

Datum
ssk_remove(PG_FUNCTION_ARGS)
{
    bytea *input = PG_GETARG_BYTEA_PP(0);
    int64 id = PG_GETARG_INT64(1);
    
#ifdef TRIVIAL
    uint64_t bits = 0;
    if (VARSIZE_ANY_EXHDR(input) >= 8)
        memcpy(&bits, VARDATA_ANY(input), 8);
    
    if (id >= 1 && id <= 64)
        bits &= ~(1UL << (id - 1));
#else
    /* NON-TRIVIAL: remove element with id */
    uint64_t bits = 0;
#endif
    
    bytea *result = (bytea *) palloc(VARHDRSZ + 8);
    SET_VARSIZE(result, VARHDRSZ + 8);
    memcpy(VARDATA(result), &bits, 8);
    PG_RETURN_BYTEA_P(result);
}

/* ssk_contains(ssk, bigint) - test if bit at position id-1 is set */

Datum
ssk_contains(PG_FUNCTION_ARGS)
{
    bytea *input = PG_GETARG_BYTEA_PP(0);
    int64 id = PG_GETARG_INT64(1);
    
#ifdef TRIVIAL
    uint64_t bits = 0;
    if (VARSIZE_ANY_EXHDR(input) >= 8)
        memcpy(&bits, VARDATA_ANY(input), 8);
    
    if (id >= 1 && id <= 64)
        PG_RETURN_BOOL((bits & (1UL << (id - 1))) != 0);
    PG_RETURN_BOOL(false);
#else
    /* NON-TRIVIAL: test if element is in hierarchical structure */
    PG_RETURN_BOOL(false);
#endif
}

/* ssk_is_contained(bigint, ssk) - test if id is in SSK */

Datum
ssk_is_contained(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    bytea *input = PG_GETARG_BYTEA_PP(1);
    
#ifdef TRIVIAL
    uint64_t bits = 0;
    if (VARSIZE_ANY_EXHDR(input) >= 8)
        memcpy(&bits, VARDATA_ANY(input), 8);
    
    if (id >= 1 && id <= 64)
        PG_RETURN_BOOL((bits & (1UL << (id - 1))) != 0);
    PG_RETURN_BOOL(false);
#else
    /* NON-TRIVIAL: test if element is in hierarchical structure */
    PG_RETURN_BOOL(false);
#endif
}

/* ssk_union(ssk, ssk) - bitwise OR */

Datum
ssk_union(PG_FUNCTION_ARGS)
{
    bytea *left = PG_GETARG_BYTEA_PP(0);
    bytea *right = PG_GETARG_BYTEA_PP(1);
    
#ifdef TRIVIAL
    uint64_t left_bits = 0, right_bits = 0;
    if (VARSIZE_ANY_EXHDR(left) >= 8)
        memcpy(&left_bits, VARDATA_ANY(left), 8);
    if (VARSIZE_ANY_EXHDR(right) >= 8)
        memcpy(&right_bits, VARDATA_ANY(right), 8);
    uint64_t result_bits = left_bits | right_bits;
#else
    /* NON-TRIVIAL: union of hierarchical structures */
    uint64_t result_bits = 0;
#endif
    
    bytea *result = (bytea *) palloc(VARHDRSZ + 8);
    SET_VARSIZE(result, VARHDRSZ + 8);
    memcpy(VARDATA(result), &result_bits, 8);
    PG_RETURN_BYTEA_P(result);
}

/* ssk_intersect(ssk, ssk) - bitwise AND */

Datum
ssk_intersect(PG_FUNCTION_ARGS)
{
    bytea *left = PG_GETARG_BYTEA_PP(0);
    bytea *right = PG_GETARG_BYTEA_PP(1);
    
#ifdef TRIVIAL
    uint64_t left_bits = 0, right_bits = 0;
    if (VARSIZE_ANY_EXHDR(left) >= 8)
        memcpy(&left_bits, VARDATA_ANY(left), 8);
    if (VARSIZE_ANY_EXHDR(right) >= 8)
        memcpy(&right_bits, VARDATA_ANY(right), 8);
    uint64_t result_bits = left_bits & right_bits;
#else
    /* NON-TRIVIAL: intersection of hierarchical structures */
    uint64_t result_bits = 0;
#endif
    
    bytea *result = (bytea *) palloc(VARHDRSZ + 8);
    SET_VARSIZE(result, VARHDRSZ + 8);
    memcpy(VARDATA(result), &result_bits, 8);
    PG_RETURN_BYTEA_P(result);
}

/* ssk_except(ssk, ssk) - bitwise AND with complement */

Datum
ssk_except(PG_FUNCTION_ARGS)
{
    bytea *left = PG_GETARG_BYTEA_PP(0);
    bytea *right = PG_GETARG_BYTEA_PP(1);
    
#ifdef TRIVIAL
    uint64_t left_bits = 0, right_bits = 0;
    if (VARSIZE_ANY_EXHDR(left) >= 8)
        memcpy(&left_bits, VARDATA_ANY(left), 8);
    if (VARSIZE_ANY_EXHDR(right) >= 8)
        memcpy(&right_bits, VARDATA_ANY(right), 8);
    uint64_t result_bits = left_bits & ~right_bits;
#else
    /* NON-TRIVIAL: difference of hierarchical structures */
    uint64_t result_bits = 0;
#endif
    
    bytea *result = (bytea *) palloc(VARHDRSZ + 8);
    SET_VARSIZE(result, VARHDRSZ + 8);
    memcpy(VARDATA(result), &result_bits, 8);
    PG_RETURN_BYTEA_P(result);
}

/* ssk_cardinality(ssk) - count set bits (popcount) */

Datum
ssk_cardinality(PG_FUNCTION_ARGS)
{
    bytea *input = PG_GETARG_BYTEA_PP(0);
    
#ifdef TRIVIAL
    uint64_t bits = 0;
    if (VARSIZE_ANY_EXHDR(input) >= 8) {
        memcpy(&bits, VARDATA_ANY(input), 8);
    }
    int count = __builtin_popcountll(bits);
#else
    /* NON-TRIVIAL: count elements in hierarchical structure */
    int count = 0;
#endif
    
    PG_RETURN_INT64(count);
}

/* ssk_is_empty(ssk) - test if all bits are zero */

Datum
ssk_is_empty(PG_FUNCTION_ARGS)
{
    bytea *input = PG_GETARG_BYTEA_PP(0);
    
#ifdef TRIVIAL
    uint64_t bits = 0;
    if (VARSIZE_ANY_EXHDR(input) >= 8) {
        memcpy(&bits, VARDATA_ANY(input), 8);
    }
    PG_RETURN_BOOL(bits == 0);
#else
    /* NON-TRIVIAL: test if hierarchical structure is empty */
    PG_RETURN_BOOL(true);
#endif
}

/* ssk_unnest(ssk) - yields all set bit positions as IDs (1-64) */

Datum
ssk_unnest(PG_FUNCTION_ARGS)
{
    bytea *input = PG_GETARG_BYTEA_PP(0);
    FuncCallContext *funcctx;
    MemoryContext oldcontext;
    int64 *bit_positions;
    int num_bits;
    
    if (SRF_IS_FIRSTCALL()) {
        funcctx = SRF_FIRSTCALL_INIT();
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
        
#ifdef TRIVIAL
        uint64_t bits = 0;
        if (VARSIZE_ANY_EXHDR(input) >= 8) {
            memcpy(&bits, VARDATA_ANY(input), 8);
        }
        
        /* Count and collect set bits */
        num_bits = __builtin_popcountll(bits);
        bit_positions = (int64 *) palloc(num_bits * sizeof(int64));
        
        int idx = 0;
        for (int bit = 0; bit < 64; bit++) {
            if (bits & (1UL << bit)) {
                bit_positions[idx++] = bit + 1;
            }
        }
#else
        /* NON-TRIVIAL: enumerate elements from hierarchical structure */
        num_bits = 0;
        bit_positions = (int64 *) palloc(0);
#endif
        
        funcctx->user_fctx = (void *) bit_positions;
        funcctx->max_calls = num_bits;
        
        MemoryContextSwitchTo(oldcontext);
    }
    
    funcctx = SRF_PERCALL_SETUP();
    bit_positions = (int64 *) funcctx->user_fctx;
    
    if (funcctx->call_cntr < funcctx->max_calls) {
        int64 result = bit_positions[funcctx->call_cntr];
        SRF_RETURN_NEXT(funcctx, Int64GetDatum(result));
    } else {
        SRF_RETURN_DONE(funcctx);
    }
}

/* ssk_from_array(bigint[]) - construct SSK from array of IDs */

Datum
ssk_from_array(PG_FUNCTION_ARGS)
{
    ArrayType *arr = PG_GETARG_ARRAYTYPE_P(0);
    
    /* Reject multi-dimensional arrays - only 1D or empty allowed */
    if (ARR_NDIM(arr) > 1) {
        ereport(ERROR,
                (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                 errmsg("ssk_from_array: multi-dimensional arrays not supported")));
    }
    
#ifdef TRIVIAL
    uint64_t bits = 0;
    
    if (ARR_NDIM(arr) == 0) {
        /* Empty array */
    } else {
        int16 typlen;
        bool typbyval;
        char typalign;
        Datum *values;
        bool *nulls;
        int nitems;
        
        /* Get type information for array elements */
        get_typlenbyvalalign(ARR_ELEMTYPE(arr), &typlen, &typbyval, &typalign);
        
        /* Deconstruct the array */
        deconstruct_array(arr, ARR_ELEMTYPE(arr), typlen, typbyval, typalign,
                         &values, &nulls, &nitems);
        
        /* Process each element */
        for (int i = 0; i < nitems; i++) {
            if (!nulls[i]) {
                int64 id = DatumGetInt64(values[i]);
                if (id >= 1 && id <= 64) {
                    bits |= (1UL << (id - 1));
                }
            }
        }
        
        pfree(values);
        pfree(nulls);
    }
#else
    /* NON-TRIVIAL: build hierarchical structure from array */
    uint64_t bits = 0;
#endif
    
    bytea *result = (bytea *) palloc(VARHDRSZ + 8);
    SET_VARSIZE(result, VARHDRSZ + 8);
    memcpy(VARDATA(result), &bits, 8);
    PG_RETURN_BYTEA_P(result);
}

/* ssk_to_array(ssk) - convert SSK to array of IDs */

Datum
ssk_to_array(PG_FUNCTION_ARGS)
{
    bytea *input = PG_GETARG_BYTEA_PP(0);
    ArrayBuildState *astate;
    
    astate = initArrayResult(INT8OID, CurrentMemoryContext, false);
    
#ifdef TRIVIAL
    uint64_t bits = 0;
    if (VARSIZE_ANY_EXHDR(input) >= 8) {
        memcpy(&bits, VARDATA_ANY(input), 8);
    }
    
    for (int bit = 0; bit < 64; bit++) {
        if (bits & (1UL << bit)) {
            astate = accumArrayResult(astate, Int64GetDatum(bit + 1), false, 
                                     INT8OID, CurrentMemoryContext);
        }
    }
#else
    /* NON-TRIVIAL: convert hierarchical structure to array */
#endif
    
    PG_RETURN_DATUM(makeArrayResult(astate, CurrentMemoryContext));
}

/* ssk_cmp - wrapper to byteacmp (byte-by-byte comparison, identical external representation with different type names) */
Datum
ssk_cmp(PG_FUNCTION_ARGS)
{
    PG_RETURN_INT32(byteacmp(fcinfo));
}

/* ssk_length(ssk) - returns byte length of SSK value */

Datum
ssk_length(PG_FUNCTION_ARGS)
{
    bytea *input = PG_GETARG_BYTEA_PP(0);
    
#ifdef TRIVIAL
    int64 len = VARSIZE_ANY_EXHDR(input);
#else
    /* NON-TRIVIAL: return size of hierarchical structure */
    int64 len = VARSIZE_ANY_EXHDR(input);
#endif
    
    PG_RETURN_INT64(len);
}
