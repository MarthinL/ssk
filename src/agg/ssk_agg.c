/*
 * Copyright (c) 2020-2025 Marthin Laubscher
 * All rights reserved. See LICENSE for details.
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "ssk.h"
#include "cdu.h"

PG_MODULE_MAGIC;

/* All aggregate function declarations (same for both TRIVIAL and NON-TRIVIAL) */
PG_FUNCTION_INFO_V1(ssk_sfunc);
PG_FUNCTION_INFO_V1(ssk_finalfunc);

#ifdef TRIVIAL

/* ============================================================================
 * TRIVIAL MODE IMPLEMENTATIONS
 * 
 * Direct uint64_t bitvector accumulation via bitwise OR
 * ============================================================================
 */

/* Helper: Create empty SSK (zero bitvector) */
static bytea *
make_empty_ssk(void)
{
	uint64_t bits = 0;
	bytea *result = (bytea *) palloc(VARHDRSZ + 8);
	SET_VARSIZE(result, VARHDRSZ + 8);
	memcpy(VARDATA(result), &bits, 8);
	return result;
}

/* Helper: Copy SSK value */
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

Datum
ssk_sfunc(PG_FUNCTION_ARGS)
{
	bytea *state;
	uint64_t bits = 0;
	int64 id;

	if (PG_ARGISNULL(0)) {
		state = make_empty_ssk();
	} else {
		state = PG_GETARG_BYTEA_PP(0);
		if (VARSIZE_ANY_EXHDR(state) >= 8) {
			memcpy(&bits, VARDATA_ANY(state), 8);
		}
	}

	if (!PG_ARGISNULL(1)) {
		id = PG_GETARG_INT64(1);
		if (id >= 1 && id <= 64) {
			bits |= (1UL << (id - 1));
		}
	}

	bytea *result = (bytea *) palloc(VARHDRSZ + 8);
	SET_VARSIZE(result, VARHDRSZ + 8);
	memcpy(VARDATA(result), &bits, 8);
	PG_RETURN_BYTEA_P(result);
}

Datum
ssk_finalfunc(PG_FUNCTION_ARGS)
{
	bytea *state = PG_GETARG_BYTEA_PP(0);
	PG_RETURN_BYTEA_P(copy_ssk(state));
}

#else /* NON-TRIVIAL */

/* ============================================================================
 * NON-TRIVIAL MODE IMPLEMENTATIONS
 * 
 * Hierarchical codec accumulation.
 * Stub implementations for development.
 * ============================================================================
 */

/* Helper: Create empty SSK (Format 0 header only) */
static bytea *
make_empty_ssk(void)
{
	bytea *result;
	result = (bytea *) palloc(VARHDRSZ + 1);
	SET_VARSIZE(result, VARHDRSZ + 1);
	*((unsigned char *) VARDATA(result)) = 0x00;
	return result;
}

/* Helper: Copy SSK value */
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

Datum
ssk_finalfunc(PG_FUNCTION_ARGS)
{
	bytea *state = PG_GETARG_BYTEA_PP(0);
	PG_RETURN_BYTEA_P(copy_ssk(state));
}

#endif /* TRIVIAL */