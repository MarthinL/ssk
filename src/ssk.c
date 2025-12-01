/*
 * ssk.c
 *      SSK (SubSet Key) PostgreSQL extension
 *
 * Copyright (c) 2025, SSK Contributors
 * Released to the public domain.
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;

/*
 * ssk_version - return the extension version string
 */
PG_FUNCTION_INFO_V1(ssk_version);

Datum
ssk_version(PG_FUNCTION_ARGS)
{
    PG_RETURN_TEXT_P(cstring_to_text("1.0"));
}