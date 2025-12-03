/*
 * src/codec/ssk_codec.c
 *
 * Stub for SSK codec main functions.
 * Real implementations will be in VLQ-P, combinadic, and chunk modules.
 */

#include "ssk.h"
#include "ssk_format.h"

#ifdef USE_PG
#include "postgres.h"
#define ALLOC(size) palloc(size)
#define FREE(ptr) pfree(ptr)
#else
#include <stdlib.h>
#include <string.h>
#define ALLOC(size) malloc(size)
#define FREE(ptr) free(ptr)
#endif

/* Stub: Encode SSK to binary Format 0 (minimal) */
int
ssk_encode(const SSKDecoded *ssk, uint8_t *buffer, size_t buffer_size, uint16_t target_format)
{
if (buffer_size < 1)
return -1;
buffer[0] = 0x00;  /* Format 0 header */
return 1;
}

/* Stub: Decode binary to SSK memory structures */
int
ssk_decode(const uint8_t *buffer, size_t buffer_size, SSKDecoded **ssk)
{
if (buffer_size < 1)
return -1;

	*ssk = (SSKDecoded *) ALLOC(sizeof(SSKDecoded));
	if (!*ssk)
		return -1;

	(*ssk)->format_version = buffer[0] & 0x0F;
	(*ssk)->n_partitions = 0;
	(*ssk)->var_data_off = 0;
	(*ssk)->var_data_used = 0;
	(*ssk)->var_data_allocated = 0;
	(*ssk)->cardinality = 0;
	(*ssk)->rare_bit = 0;
	return 1;
}

/* Stub: Free decoded SSK */
void
ssk_free_decoded(SSKDecoded *ssk)
{
if (ssk)
FREE(ssk);
}
