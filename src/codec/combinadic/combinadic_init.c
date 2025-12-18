/*
 * Copyright (c) 2020-2025 Marthin Laubscher
 * All rights reserved. See LICENSE for details.
 */

/*
 * src/codec/combinadic/combinadic_init.c
 *
 * Combinadic encoding for sparse bit patterns.
 *
 * The combinadic system represents a k-subset of {0, 1, ..., n-1} as a single
 * integer "rank" in the range [0, C(n,k)-1]. This provides optimal compression
 * for chunks with known popcount k.
 *
 * Key concepts:
 *   - C(n,k) = binomial coefficient = n! / (k! * (n-k)!)
 *   - rank_bits[n][k] = ceil(log2(C(n,k))) = bits needed to encode any rank
 *   - rank(bits, n, k) = unique index for this specific k-subset
 *   - unrank(rank, n, k) = reconstruct the k-subset from its rank
 *
 * SSK uses combinadic for ENUM tokens where k <= K_CHUNK_ENUM_MAX (18).
 * The precomputed tables enable fast rank/unrank operations.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef USE_PG
#include "postgres.h"
#else
#include <stdlib.h>
#endif

#include "ssk_format.h"
#include "ssk_constants.h"

/* ============================================================================
 * PRECOMPUTED TABLES
 *
 * binomial[k][n] = C(n,k) for k in [0,18], n in [0,64]  // k-major for cache-friendly unrank
 * binomial_nmajor[n][k] = C(n,k) for n in [0,64], k in [0,18]  // n-major for comparison
 * ssk_rank_bits[k][n] = ceil(log2(C(n,k))) for same range (exported in ssk_format.h)
 *
 * Total memory: 2 * (65 * 19 * 8 bytes) (binomial) + 65 * 19 * 1 byte (rank_bits)
 *             = 19,760 + 1,235 = ~21 KB
 * ============================================================================
 */

/* Binomial coefficients C(n,k) - k-major layout (cache-friendly for unrank) */
uint64_t binomial[SSK_RANK_BITS_K_MAX + 1][SSK_RANK_BITS_N_MAX + 1];

/* Binomial coefficients C(n,k) - n-major layout (original layout for comparison) */
uint64_t binomial_nmajor[SSK_RANK_BITS_N_MAX + 1][SSK_RANK_BITS_K_MAX + 1];

/* Binomial coefficients C(n,k) - reversed layout optimized for forward unrank traversal */
uint64_t binomial_unrank[SSK_RANK_BITS_K_MAX + 1][SSK_RANK_BITS_N_MAX + 1];

/* Bits needed to encode rank for C(n,k) - exported global (declared in ssk_format.h) */

__attribute__((visibility("default"))) uint8_t ssk_rank_bits[SSK_RANK_BITS_K_MAX + 1][SSK_RANK_BITS_N_MAX + 1];

/* Initialization flag */
bool tables_initialized = false;

/* ============================================================================
 * TABLE INITIALIZATION
 * ============================================================================
 */

/**
 * Compute ceil(log2(x)) - minimum bits to represent values 0 to x-1.
 */
static uint8_t
ceil_log2(uint64_t x)
{
    if (x <= 1)
        return 0;
    
    uint8_t bits = 0;
    uint64_t v = x - 1;  /* For ceiling, we need bits for x-1 */
    
    while (v > 0)
    {
        bits++;
        v >>= 1;
    }
    
    return bits;
}

/**
 * Initialize binomial coefficient and ssk_rank_bits tables.
 *
 * Uses Pascal's triangle recurrence: C(n,k) = C(n-1,k-1) + C(n-1,k)
 * This avoids overflow issues with factorial computation.
 *
 * Initializes both k-major (cache-friendly) and n-major (original) layouts.
 * Call once at module load. Safe to call multiple times (idempotent).
 */
void
ssk_combinadic_init(void)
{
    if (tables_initialized)
        return;
    
    /* Initialize binomial coefficients using Pascal's triangle - k-major layout */
    for (int k = 0; k <= SSK_RANK_BITS_K_MAX; k++)      // Outer loop: k (rows)
    {
        for (int n = 0; n <= SSK_RANK_BITS_N_MAX; n++)  // Inner loop: n (columns)
        {
            if (k > n)
            {
                /* C(n,k) = 0 when k > n */
                binomial[k][n] = 0;
                ssk_rank_bits[k][n] = 0;
            }
            else if (k == 0 || k == n)
            {
                /* C(n,0) = C(n,n) = 1 */
                binomial[k][n] = 1;
                ssk_rank_bits[k][n] = 0;  /* Only one possibility, 0 bits needed */
            }
            else
            {
                /* Pascal's recurrence: C(n,k) = C(n-1,k-1) + C(n-1,k) */
                binomial[k][n] = binomial[k-1][n-1] + binomial[k][n-1];
                ssk_rank_bits[k][n] = ceil_log2(binomial[k][n]);
            }
        }
    }
    
    /* Initialize binomial coefficients using Pascal's triangle - n-major layout */
    for (int n = 0; n <= SSK_RANK_BITS_N_MAX; n++)      // Outer loop: n (rows)
    {
        for (int k = 0; k <= SSK_RANK_BITS_K_MAX; k++)  // Inner loop: k (columns)
        {
            if (k > n)
            {
                /* C(n,k) = 0 when k > n */
                binomial_nmajor[n][k] = 0;
            }
            else if (k == 0 || k == n)
            {
                /* C(n,0) = C(n,n) = 1 */
                binomial_nmajor[n][k] = 1;
            }
            else
            {
                /* Pascal's recurrence: C(n,k) = C(n-1,k-1) + C(n-1,k) */
                binomial_nmajor[n][k] = binomial_nmajor[n-1][k-1] + binomial_nmajor[n-1][k];
            }
        }
    }

    /* Initialize binomial coefficients - reversed layout for forward unrank traversal */
    /* binomial_unrank[k_idx][pos_idx] = C(pos, k) where:
     *   pos = SSK_RANK_BITS_N_MAX - pos_idx
     *   k = SSK_RANK_BITS_K_MAX - k_idx
     */
    for (int k_idx = 0; k_idx <= SSK_RANK_BITS_K_MAX; k_idx++)
    {
        int k = SSK_RANK_BITS_K_MAX - k_idx;
        for (int pos_idx = 0; pos_idx <= SSK_RANK_BITS_N_MAX; pos_idx++)
        {
            int pos = SSK_RANK_BITS_N_MAX - pos_idx;
            binomial_unrank[k_idx][pos_idx] = binomial[k][pos];
        }
    }
    
    tables_initialized = true;
}

/**
 * Get binomial coefficient C(n,k).
 *
 * @param n  Total positions (0 to 64)
 * @param k  Number of set bits (0 to 18)
 * @return C(n,k), or 0 if out of range
 */
uint64_t
ssk_binomial(uint8_t n, uint8_t k)
{
    if (!tables_initialized)
        ssk_combinadic_init();
    
    if (n > SSK_RANK_BITS_N_MAX || k > SSK_RANK_BITS_K_MAX)
        return 0;
    
    return binomial_nmajor[n][k];
}

/**
 * Get number of bits needed to encode rank for C(n,k).
 *
 * @param n  Total positions (0 to 64)
 * @param k  Number of set bits (0 to 18)
 * @return Bits needed, or 0 if out of range
 */
uint8_t
ssk_get_rank_bits(uint8_t n, uint8_t k)
{
    if (!tables_initialized)
        ssk_combinadic_init();
    
    if (n > SSK_RANK_BITS_N_MAX || k > SSK_RANK_BITS_K_MAX)
        return 0;
    
    return ssk_rank_bits[k][n];
}



