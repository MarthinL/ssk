/*
 * Copyright (c) 2020-2025 Marthin Laubscher
 * All rights reserved. See LICENSE for details.
 */

#ifndef SSK_H
#define SSK_H

#ifdef TRIVIAL

#include "abv_format.h" // FOR TRIVIAL, essentially bakes "Format 1023" into the code

/*
 * The TRIVIAL implenentation of SSK operates in a constrained domain of IDs 1..64.
 * In this mode, the Abstract bit Vector is represented directly as a single
 * uint64_t value, where each bit corresponds directly to an ID in the domain.
 */

#else // NON TRIVIAL

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "abv_format.h" // For NON TRIVIAL, bakes "Format 0" into the code
#include "cdu.h"

/**
 * @file ssk.h
 * @brief SSK (SubSet Key) primary header
 *
 * CONCERN CONTEXT:
 * This header spans multiple implementation concerns:
 * - Integration: IMP/A0:The SSK Extension (PostgreSQL UDT types and function declarations)
 * - Operations: IMP/A2:Function Processor (Set algebra function signatures)
 * - Persistence: IMP/A1:Value Decoder, IMP/A3:Value Encoder (Encode/decode entry points)
 *
 * See ARCHITECTURE.md for formal concern model.
 *
 * THE CORE CONCEPT:
 * SSK establishes a bijection between subsets of database IDs and scalar values.
 * Each possible subset of a table's primary keys maps to exactly one SSK value.
 * This is NOT a hash (no collisions) and NOT compression (perfectly reversible).
 *
 * CONCEPTUAL MODEL:
 * An SSK is abstractly equivalent to an abstract bit vector (abvector) with one bit
 * per possible ID in the domain (0 to 2^64-1). The abvector is astronomically
 * sparse, so SSK uses hierarchical encoding (partitions → segments → chunks) to
 * represent only the non-empty regions efficiently.
 *
 * For detailed format specifications, memory layout, and design rationale,
 * see ssk_format.h and SPECIFICATION.md.
 */

// ============================================================================
// ENCODING / DECODING
// ============================================================================

/**
 * Encode an abstract bit vector (AbV) into binary SSK format.
 * 
 * @param abv Abstract bit vector to encode
 * @param buffer Output buffer for encoded SSK bytes
 * @param buffer_size Size of output buffer
 * @param target_format Format version to encode as (0 = Format 0)
 * @return Bytes written, or -1 on error
 *
 * Transforms an in-memory AbV into a canonical byte sequence suitable for
 * storage as an SSK value. The encoding is deterministic: same AbV always
 * produces identical bytes (bijection property).
 */
int abv_encode(const AbV abv, uint8_t *buffer, size_t buffer_size, uint16_t target_format);

/**
 * Decode SSK bytes into an abstract bit vector (AbV) for manipulation.
 *
 * @param buffer Input buffer containing encoded SSK bytes
 * @param buffer_size Size of input buffer
 * @param abv Output pointer for decoded AbV (allocated by this function)
 * @return Bytes read, or -1 on error
 *
 * Transforms a byte sequence into an in-memory AbV structure for operations
 * like set membership queries or modifications. Caller must free returned AbV
 * with abv_free(). Supports decoding any known format version.
 */
int abv_decode(const uint8_t *buffer, size_t buffer_size, AbV *abv);

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
 * Set a bit (add an ID) in an abstract bit vector.
 *
 * @param abv Abstract bit vector (may be reallocated)
 * @param id ID to add
 * @return Updated AbV pointer (may differ from input), or NULL on error
 */
AbV abv_set_bit(AbV abv, uint64_t id);

/**
 * Get a bit (check if ID is present) in an abstract bit vector.
 *
 * @param abv Abstract bit vector
 * @param id ID to check
 * @return 1 if present, 0 if absent
 */
int abv_get_bit(const AbV abv, uint64_t id);

/**
 * Count set bits (cardinality) of an abstract bit vector.
 *
 * @param abv Abstract bit vector
 * @return Number of IDs in the subset
 */
uint64_t abv_popcount(const AbV abv);

// ============================================================================
// CACHE/STORE (Stubbed for v0.1)
// ============================================================================

/**
 * Retrieve cached abstract bit vector from session keystore.
 *
 * @param ssk Encoded SSK bytes (Datum representation)
 * @return Cached AbV, or NULL if not cached
 *
 * PostgreSQL only: session-local cache to avoid repeated abv_decode() calls.
 * v0.1: Always returns NULL (stub).
 */
AbV abv_cache_get(void *ssk);

/**
 * Store abstract bit vector in session keystore.
 *
 * @param ssk Encoded SSK bytes (Datum representation)
 * @param abv Abstract bit vector to cache
 *
 * PostgreSQL only: caches AbV for session lifetime to avoid re-decoding
 * the same SSK value repeatedly. v0.1: No-op (stub).
 */
void abv_cache_put(void *ssk, AbV abv);

/**
 * Adopt abstract bit vector from aggregate context into session cache.
 * Called by aggregate finalfunc to enable future sharing across queries.
 *
 * @param abv Abstract bit vector from aggregate state
 *
 * v0.1: No-op (stub).
 */
void abv_cache_adopt_from_aggregate(AbV abv);

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

#endif // (NON) TRIVIAL

#endif // SSK_H