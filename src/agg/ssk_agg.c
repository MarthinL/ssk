/*
 * src/agg/ssk_agg.c
 *
 * Stub implementations for SSK aggregate functions.
 * ssk_agg(bigint) accumulates values into an SSK.
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "ssk.h"

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

/*
 * Aggregate state function: ssk_agg_sfunc(ssk, bigint) -> ssk
 * Adds a bigint value to the accumulating SSK.
 * Stub: ignores the value, just passes through the SSK.
 */
PG_FUNCTION_INFO_V1(ssk_sfunc);
Datum
ssk_sfunc(PG_FUNCTION_ARGS)
{
	bytea *state;

	if (PG_ARGISNULL(0))
		state = make_empty_ssk();
	else
		state = PG_GETARG_BYTEA_PP(0);

	/* For now, just copy state - real implementation will add element */
	PG_RETURN_BYTEA_P(copy_ssk(state));
}

/*
 * Aggregate final function: ssk_agg_finalfunc(ssk) -> ssk
 * Returns the final accumulated SSK.
 * Stub: just returns the state as-is.
 */
PG_FUNCTION_INFO_V1(ssk_finalfunc);
Datum
ssk_finalfunc(PG_FUNCTION_ARGS)
{
	bytea *state = PG_GETARG_BYTEA_PP(0);
	PG_RETURN_BYTEA_P(copy_ssk(state));
}