/*
 * Copyright (c) 2020-2025 Marthin Laubscher
 * All rights reserved. See LICENSE for details.
 */

#ifndef SSK_H
#define SSK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ssk_format.h"
#include "cdu.h"

/**
 * @file ssk.h
 * @brief Public interface for the SSK library.
 *
 * Subset Keys (SSK) represent a subset of database ID values as a single scalar value,
 * enabling efficient storage, set operations, and analytics in SQL.
 *
 * CONCEPTUAL MODEL:
 * An SSK is abstractly equivalent to an abstract bit vector (abvector) with one bit
 * per possible ID in the domain (0 to 2^64-1). The abvector is astronomically
 * sparse, so SSK uses hierarchical compression (partitions → segments → chunks) to
 * represent only the non-empty regions efficiently.
 *
 * For detailed format specifications, memory layout, and design rationale,
 * see ssk_format.h and copilot-instructions.md.
 */

// ============================================================================
// ENCODING / DECODING
// ============================================================================

/**
 * Encode a decoded SSK into binary format.
 * 
 * @param ssk Decoded SSK structure
 * @param buffer Output buffer for encoded data
 * @param buffer_size Size of output buffer
 * @param target_format Format version to encode as (0 = use SSK's current format)
 * @return Bytes written, or -1 on error
 *
 * If target_format is 0, encodes using the format the SSK was decoded from.
 * Otherwise, converts to the specified format (if supported).
 */
int ssk_encode(const SSKDecoded *ssk, uint8_t *buffer, size_t buffer_size, uint16_t target_format);

/**
 * Decode a binary SSK into memory structures.
 *
 * @param buffer Input buffer containing encoded SSK
 * @param buffer_size Size of input buffer
 * @param ssk Output pointer for decoded SSK (allocated by this function)
 * @return Bytes read, or -1 on error
 *
 * Caller must free returned SSK with ssk_free_decoded().
 * Supports decoding any known format version.
 */
int ssk_decode(const uint8_t *buffer, size_t buffer_size, SSKDecoded **ssk);

// ============================================================================
// CDU CODEC
// ============================================================================

// ============================================================================
// COMBINADIC CODEC
// ============================================================================

/**
 * Initialize combinadic tables (binomial coefficients, rank_bits).
 * Call once at module load. Safe to call multiple times.
 */
void ssk_combinadic_init(void);

/**
 * Get binomial coefficient C(n,k).
 *
 * @param n Total positions (0 to 64)
 * @param k Number of selections (0 to 18)
 * @return C(n,k), or 0 if out of range
 */
uint64_t ssk_binomial(uint8_t n, uint8_t k);

/**
 * Get bits needed to encode combinadic rank for C(n,k).
 *
 * @param n Total positions
 * @param k Number of set bits
 * @return Bits needed for rank encoding
 */
uint8_t ssk_get_rank_bits(uint8_t n, uint8_t k);

/**
 * Compute combinadic rank of a bit pattern.
 *
 * @param bits Bit pattern (k bits set out of n positions)
 * @param n Number of bit positions (1 to 64)
 * @param k Number of set bits (popcount)
 * @return Rank in [0, C(n,k)-1]
 */
uint64_t ssk_combinadic_rank(uint64_t bits, uint8_t n, uint8_t k);

/**
 * Reconstruct bit pattern from combinadic rank.
 *
 * @param rank Rank value [0, C(n,k)-1]
 * @param n Number of bit positions
 * @param k Number of set bits to reconstruct
 * @return Bit pattern with exactly k bits set
 */
uint64_t ssk_combinadic_unrank(uint64_t rank, uint8_t n, uint8_t k);

/**
 * Count set bits (popcount) in a 64-bit value.
 */
uint8_t ssk_popcount64(uint64_t x);

/**
 * Validate that a rank is within bounds for C(n,k).
 */
bool ssk_combinadic_rank_valid(uint64_t rank, uint8_t n, uint8_t k);

// ============================================================================
// SSK OPERATIONS
// ============================================================================

/**
 * Set a bit (add an ID) in decoded SSK.
 *
 * @param ssk Decoded SSK (may be reallocated)
 * @param id ID to add
 * @return Updated SSK pointer (may differ from input), or NULL on error
 */
SSKDecoded *ssk_set_bit(SSKDecoded *ssk, uint64_t id);

/**
 * Get a bit (check if ID is present) in decoded SSK.
 *
 * @param ssk Decoded SSK
 * @param id ID to check
 * @return 1 if present, 0 if absent
 */
int ssk_get_bit(const SSKDecoded *ssk, uint64_t id);

/**
 * Count set bits (cardinality) of decoded SSK.
 *
 * @param ssk Decoded SSK
 * @return Number of IDs in the subset
 */
uint64_t ssk_popcount(const SSKDecoded *ssk);

// ============================================================================
// CACHE/STORE (Stubbed for v0.1)
// ============================================================================

/**
 * Retrieve cached decoded SSK from keystore.
 *
 * @param key Encoded SSK (Datum representation)
 * @return Cached decoded SSK, or NULL if not cached
 *
 * PostgreSQL only: session-local cache to avoid repeated decoding.
 * v0.1: Always returns NULL (stub).
 */
SSKDecoded *ssk_cache_get(void *key);

/**
 * Store decoded SSK in keystore.
 *
 * @param key Encoded SSK (Datum representation)
 * @param obj Decoded SSK to cache
 *
 * PostgreSQL only: caches decoded SSK for session lifetime.
 * v0.1: No-op (stub).
 */
void ssk_cache_put(void *key, SSKDecoded *obj);

/**
 * Adopt decoded SSK from aggregate context into cache.
 * Called by aggregate finalize to enable future sharing.
 *
 * @param obj Decoded SSK from aggregate state
 *
 * v0.1: No-op (stub).
 */
void ssk_cache_adopt_from_aggregate(SSKDecoded *obj);

// ============================================================================
// ORDINALITY CODEC (Legacy - may be superseded by CDU)
// ============================================================================

/**
 * Encode ordinality array (list of IDs).
 *
 * @param ordinality Array of IDs
 * @param length Number of IDs
 * @param buffer Output buffer
 * @return Bytes written, or -1 on error
 */
int ordinality_encode(const uint32_t *ordinality, int length, uint8_t *buffer);

/**
 * Decode ordinality array.
 *
 * @param buffer Input buffer
 * @param ordinality Output array (allocated by caller)
 * @param length Output length
 * @return Bytes read, or -1 on error
 */
int ordinality_decode(const uint8_t *buffer, uint32_t *ordinality, int *length);

#endif // SSK_H