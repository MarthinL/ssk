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

/* All AbV mediation between Postgres and SSK internals happen here (same for both TRIVIAL and NON-TRIVIAL) */
// PG_FUNCTION_INFO_V1(ssk_sfunc);
// PG_FUNCTION_INFO_V1(ssk_finalfunc);

#ifdef TRIVIAL

/* ============================================================================
 * TRIVIAL MODE IMPLEMENTATIONS
 * 
 * ============================================================================
 */

static bytea *
encode_abv(const AbV abv, bytea ** destptr)
{
  /* TODO: Implement TRIVIAL AbV to bytea encoding */
	return NULL;
}


#else /* NON-TRIVIAL */

/* ============================================================================
 * NON-TRIVIAL MODE IMPLEMENTATIONS
 * 
 * ============================================================================
 */

static bytea *
encode_abv(const AbV abv, bytea ** destptr)
{
  /* TODO: Implement NON-TRIVIAL AbV to bytea encoding */
	return NULL;
}

#endif /* TRIVIAL */