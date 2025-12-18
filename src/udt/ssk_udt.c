/*
 * src/udt/ssk_udt.c
 *
 * Stub implementations for SSK UDT functions.
 * All functions return minimal valid SSK values (Format 0 header with zero partitions).
 * Real implementations will be added as codec layer matures.
 */

#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "lib/stringinfo.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "ssk.h"
#include "cdu.h"

PG_MODULE_MAGIC;

/* Extension initialization */
void _PG_init(void) {
    cdu_init();              // Initialize CDU parameters
    ssk_combinadic_init();   // Initialize combinadic tables
}

/*
 * ssk_version - return the extension version string
 */
PG_FUNCTION_INFO_V1(ssk_version);
Datum
ssk_version(PG_FUNCTION_ARGS)
{
    PG_RETURN_TEXT_P(cstring_to_text("1.0"));
}

/* I/O Functions */

/* ssk_in(cstring) - Input function (text to bytea) */
PG_FUNCTION_INFO_V1(ssk_in);
Datum
ssk_in(PG_FUNCTION_ARGS)
{
	char *str = PG_GETARG_CSTRING(0);
	/* Delegate to byteain for now */
	return byteain(fcinfo);
}

/* ssk_out(ssk) - Output function (bytea to text) */
PG_FUNCTION_INFO_V1(ssk_out);
Datum
ssk_out(PG_FUNCTION_ARGS)
{
	/* Delegate to byteaout for now */
	return byteaout(fcinfo);
}

/*
 * Helper: Create empty SSK (Format 0 header only)
 */
static bytea *
make_empty_ssk(void)
{
	bytea *result;
	result = (bytea *) palloc(VARHDRSZ + 1);
	SET_VARSIZE(result, VARHDRSZ + 1);
	*((unsigned char *) VARDATA(result)) = 0x00;
	return result;
}

/*
 * Helper: Copy SSK value
 */
static bytea *
copy_ssk(bytea *input)
{
	bytea *result;
	size_t sz = VARSIZE_ANY_EXHDR(input);
	result = (bytea *) palloc(sz + VARHDRSZ);
	SET_VARSIZE(result, sz + VARHDRSZ);
	memcpy(VARDATA(result), VARDATA_ANY(input), sz);
	return result;
}

/* Constructor: ssk() - returns empty SSK */
PG_FUNCTION_INFO_V1(ssk_new);
Datum
ssk_new(PG_FUNCTION_ARGS)
{
	PG_RETURN_BYTEA_P(make_empty_ssk());
}

/* Constructor: ssk(bigint) - returns SSK with single element (stub: ignores value) */
PG_FUNCTION_INFO_V1(ssk_new_single);
Datum
ssk_new_single(PG_FUNCTION_ARGS)
{
	/* For now, same as empty SSK - codec will be implemented later */
	PG_RETURN_BYTEA_P(make_empty_ssk());
}

/* ssk_add(ssk, bigint) - returns copy of input SSK (stub: ignores element) */
PG_FUNCTION_INFO_V1(ssk_add);
Datum
ssk_add(PG_FUNCTION_ARGS)
{
	bytea *input = PG_GETARG_BYTEA_PP(0);
	PG_RETURN_BYTEA_P(copy_ssk(input));
}

/* ssk_remove(ssk, bigint) - returns copy of input SSK (stub: ignores element) */
PG_FUNCTION_INFO_V1(ssk_remove);
Datum
ssk_remove(PG_FUNCTION_ARGS)
{
	bytea *input = PG_GETARG_BYTEA_PP(0);
	PG_RETURN_BYTEA_P(copy_ssk(input));
}

/* ssk_contains(ssk, bigint) - always returns false (stub) */
PG_FUNCTION_INFO_V1(ssk_contains);
Datum
ssk_contains(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(false);
}

/* ssk_is_contained(bigint, ssk) - always returns false (stub) */
PG_FUNCTION_INFO_V1(ssk_is_contained);
Datum
ssk_is_contained(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(false);
}

/* ssk_union(ssk, ssk) - returns first argument (stub) */
PG_FUNCTION_INFO_V1(ssk_union);
Datum
ssk_union(PG_FUNCTION_ARGS)
{
	bytea *left = PG_GETARG_BYTEA_PP(0);
	PG_RETURN_BYTEA_P(copy_ssk(left));
}

/* ssk_intersect(ssk, ssk) - returns empty SSK (stub) */
PG_FUNCTION_INFO_V1(ssk_intersect);
Datum
ssk_intersect(PG_FUNCTION_ARGS)
{
	PG_RETURN_BYTEA_P(make_empty_ssk());
}

/* ssk_except(ssk, ssk) - returns first argument (stub) */
PG_FUNCTION_INFO_V1(ssk_except);
Datum
ssk_except(PG_FUNCTION_ARGS)
{
	bytea *left = PG_GETARG_BYTEA_PP(0);
	PG_RETURN_BYTEA_P(copy_ssk(left));
}

/* ssk_cardinality(ssk) - always returns 0 (stub) */
PG_FUNCTION_INFO_V1(ssk_cardinality);
Datum
ssk_cardinality(PG_FUNCTION_ARGS)
{
	PG_RETURN_INT64(0);
}

/* ssk_is_empty(ssk) - always returns true (stub) */
PG_FUNCTION_INFO_V1(ssk_is_empty);
Datum
ssk_is_empty(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(true);
}

/* ssk_unnest(ssk) - returns no rows (stub) */
PG_FUNCTION_INFO_V1(ssk_unnest);
Datum
ssk_unnest(PG_FUNCTION_ARGS)
{
	/* Stub: return no rows */
	ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	if (rsinfo)
		rsinfo->isDone = ExprEndResult;
	return (Datum) 0;
}

/* ssk_from_array(bigint[]) - converts array to SSK (stub: returns empty) */
PG_FUNCTION_INFO_V1(ssk_from_array);
Datum
ssk_from_array(PG_FUNCTION_ARGS)
{
	PG_RETURN_BYTEA_P(make_empty_ssk());
}

/* ssk_to_array(ssk) - converts SSK to array (stub: returns empty array) */
PG_FUNCTION_INFO_V1(ssk_to_array);
Datum
ssk_to_array(PG_FUNCTION_ARGS)
{
	ArrayBuildState *astate = initArrayResult(INT8OID, CurrentMemoryContext, false);
	PG_RETURN_DATUM(makeArrayResult(astate, CurrentMemoryContext));
}

/* ssk_cmp(ssk, ssk) - lexicographic comparison */
PG_FUNCTION_INFO_V1(ssk_cmp);
Datum
ssk_cmp(PG_FUNCTION_ARGS)
{
	bytea *left = PG_GETARG_BYTEA_PP(0);
	bytea *right = PG_GETARG_BYTEA_PP(1);
	size_t left_len = VARSIZE_ANY_EXHDR(left);
	size_t right_len = VARSIZE_ANY_EXHDR(right);
	size_t min_len = Min(left_len, right_len);
	int cmp;

	cmp = memcmp(VARDATA_ANY(left), VARDATA_ANY(right), min_len);

	if (cmp != 0)
		PG_RETURN_INT32(cmp > 0 ? 1 : -1);

	/* If prefixes match, shorter is less */
	if (left_len < right_len)
		PG_RETURN_INT32(-1);
	if (left_len > right_len)
		PG_RETURN_INT32(1);

	PG_RETURN_INT32(0);
}

/* ssk_length(ssk) - returns byte length of SSK */
PG_FUNCTION_INFO_V1(ssk_length);
Datum
ssk_length(PG_FUNCTION_ARGS)
{
	bytea *input = PG_GETARG_BYTEA_PP(0);
	int64 len = VARSIZE_ANY_EXHDR(input);
	PG_RETURN_INT64(len);
}