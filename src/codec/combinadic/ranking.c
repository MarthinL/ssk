/*
 * Copyright (c) 2020-2025 Marthin Laubscher
 * All rights reserved. See LICENSE for details.
 */

#ifdef TRIVIAL

/*
 * None of what is defined in this file plays a role in the TRIVIAL case.
 * It purely addresses issues arising from upscaling the ID domain to BIGINT scale.
 */

#else // NON TRIVIAL

/*
 * src/codec/combinadic/ranking.c
 *
 * Combinadic rank and unrank functions for SSK ENUM tokens.
 *
 * These functions implement colexicographic (LSB-first) ordering,
 * which aligns with binary representation for optimal performance.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#ifdef USE_PG
#include "postgres.h"
#else
#include <stdlib.h>
#endif

#include "ssk_format.h"
#include "ssk_constants.h"

/* Extern declarations for tables from combinadic_init.c */
extern uint64_t binomial[SSK_RANK_BITS_K_MAX + 1][SSK_RANK_BITS_N_MAX + 1];  /* k-major layout */
extern uint64_t binomial_unrank[SSK_RANK_BITS_K_MAX + 1][SSK_RANK_BITS_N_MAX + 1];  /* reversed layout for forward unrank */
extern bool tables_initialized;

/* ============================================================================
 * COMBINADIC RANK (Encode) - OPTIMIZED VERSION
 *
 * Convert a k-subset (represented as a bit pattern) to its combinadic rank.
 *
 * Algorithm: Colexicographic (colex) order - LSB-first, binary-aligned.
 * OPTIMIZATION: O(k) complexity using bit scanning to skip unset bits.
 * Uses __builtin_ctzll() to find next set bit, achieving significant speedup
 * for sparse bit patterns (common in SSK ENUM chunks).
 *
 * Example: bits=0b010110 (n=6, k=3, positions {1,3,4})
 *   positions = [1,3,4]
 *   rank += C(1,1) = 1
 *   rank += C(3,2) = 3
 *   rank += C(4,3) = 4
 *   Final rank = 8
 * ============================================================================
 */

/**
 * Compute combinadic rank of a bit pattern (optimized implementation).
 *
 * OPTIMIZATION: Achieves O(k) performance by skipping unset bits using
 * hardware-accelerated bit scanning (__builtin_ctzll). This provides
 * significant speedup for sparse data while maintaining exact correctness.
 *
 * @param bits  Bit pattern (n bits, k bits set)
 * @param n     Number of bit positions (1 to 64)
 * @param k     Number of set bits (popcount)
 * @return Rank in [0, C(n,k)-1]
 *
 * Precondition: popcount(bits) == k, bits fits in n bits
 */
uint64_t
ssk_combinadic_rank(uint64_t bits, uint8_t n, uint8_t k)
{
    assert(tables_initialized && "Binomial tables must be initialized");
    
    assert(k > 0 && "k must be positive for ranking");
    
    assert(k <= SSK_RANK_BITS_K_MAX && "k exceeds supported range");
    
    uint64_t rank = 0;
    /* OPTIMIZATION: Use working copy to modify with bit clearing */
    uint64_t working_bits = bits;
    int j = 0;
    
    /* OPTIMIZATION: Bit scanning loop - O(k) instead of O(n) */
    while (working_bits != 0 && j < k)
    {
        /* OPTIMIZATION: Find lowest set bit position using hardware instruction */
        /* Standard code would be: for (int pos = 0; pos < n; pos++) if (bits & (1ULL << pos)) */
        int pos = __builtin_ctzll(working_bits);
        
        /* Ensure within n */
        if (pos >= n)
            break;
        
        /* Add contribution for colex order */
        rank += binomial[j + 1][pos];
        
        /* OPTIMIZATION: Clear processed bit to skip it in next iteration */
        /* Standard code would be: continue to next pos in loop */
        working_bits &= ~(1ULL << pos);
        j++;
    }
    
    return rank;
}

/* ============================================================================
 * COMBINADIC UNRANK (Decode) - FORWARD TRAVERSAL OPTIMIZATION
 *
 * Convert a combinadic rank back to its k-subset bit pattern.
 *
 * PHASE 1 ALGORITHM: Traverse positions from high to low, using double-cursor
 * approach with pos and remaining_k. For each position, check if C(pos, remaining_k)
 * <= remaining_rank. If yes, set the bit, subtract C(pos, remaining_k) from rank,
 * and decrement remaining_k. This changes the core algorithm from searching per k
 * to direct traversal with decisions at each position.
 *
 * Complexity: O(n) instead of O(k n), providing significant speedup.
 *
 * Example: rank=8, n=6, k=3
 *   pos=5: C(5,3)=10 >8, skip
 *   pos=4: C(4,3)=4 <=8, set bit, rank=4, k=2
 *   pos=3: C(3,2)=3 <=4, set bit, rank=1, k=1
 *   pos=2: C(2,1)=2 >1, skip
 *   pos=1: C(1,1)=1 <=1, set bit, rank=0, k=0
 *   pos=0: k=0, done
 *   Bits: positions 1,3,4 → 0b010110
 *
 * This maintains O(n) traversal but reduces comparisons for large remaining_k.
 *
 * Example: rank=8, n=6, k=3
 *   pos=5: C(5,3)=10 >8, skip
 *   pos=4: C(4,3)=4 <=8, set bit, rank=4, k=2
 *   pos=3: C(3,2)=3 <=4, set bit, rank=1, k=1
 *   pos=2: C(2,1)=2 >1, skip
 *   pos=1: C(1,1)=1 <=1, set bit, rank=0, k=0
 *   pos=0: k=0, done
 *   Bits: positions 1,3,4 → 0b010110
 * ============================================================================
 */

/**
 * Reconstruct bit pattern from combinadic rank (optimized forward traversal implementation).
 *
 * FORWARD TRAVERSAL OPTIMIZATION: Uses reversed table layout for pure forward memory access,
 * enabling optimal cache prefetching and pointer arithmetic. This provides the best performance
 * for production use.
 *
 * @param rank  Rank in [0, C(n,k)-1]
 * @param n     Number of bit positions (1 to 64)
 * @param k     Number of set bits to reconstruct
 * @return Bit pattern with exactly k bits set
 *
 * Precondition: rank < C(n,k)
 */
uint64_t
ssk_combinadic_unrank(uint64_t rank, uint8_t n, uint8_t k)
{
    assert(tables_initialized && "Binomial tables must be initialized");

    assert(k > 0 && "k must be positive for unranking");

    assert(k <= SSK_RANK_BITS_K_MAX && k <= n && "k exceeds supported range or n");

    uint64_t bits = 0;

    /* Forward traversal using reversed table layout */
    /* Start at binomial_unrank[k_idx][pos_idx] where:
     *   k_idx = SSK_RANK_BITS_K_MAX - k (high k_idx for high k)
     *   pos_idx = SSK_RANK_BITS_N_MAX - (n-1) (low pos_idx for high pos)
     */
    int initial_k_idx = SSK_RANK_BITS_K_MAX - k;
    int initial_pos_idx = SSK_RANK_BITS_N_MAX - (n - 1);
    uint64_t *binp = &binomial_unrank[initial_k_idx][initial_pos_idx];
    uint64_t posmask = 1ULL << (n - 1);  /* Start at highest position bit */

    while (k > 0)
    {
        /* Check if we can set this bit: *binp <= rank */
        uint64_t coeff = *binp;
        if (coeff <= rank)
        {
            /* Set the bit at this position */
            bits |= posmask;
            /* Reduce rank by the binomial coefficient used */
            rank -= coeff;
            /* Decrement k and move to next pos: k-- and pos-- */
            k--;
            posmask >>= 1;  /* Move to next lower position */
            binp += (SSK_RANK_BITS_N_MAX + 2);  /* Skip to next k level AND next pos slot */
        }
        else
        {
            /* Move to next lower position in same k level: pos-- */
            binp++;
            posmask >>= 1;  /* Move to next lower position */
        }
    }

    return bits;
}

/**
 * Validate that a rank is within bounds for C(n,k).
 *
 * @param rank  Rank to validate
 * @param n     Number of positions
 * @param k     Number of set bits
 * @return true if rank < C(n,k), false otherwise
 */
bool
ssk_combinadic_rank_valid(uint64_t rank, uint8_t n, uint8_t k)
{
    if (!tables_initialized)
        ssk_combinadic_init();
    
    if (n > SSK_RANK_BITS_N_MAX || k > SSK_RANK_BITS_K_MAX)
        return false;
    
    return rank < binomial[k][n];
}

/**
 * Count set bits (popcount) in a 64-bit value.
 */
uint8_t
ssk_popcount64(uint64_t x)
{
#ifdef __GNUC__
    return (uint8_t)__builtin_popcountll(x);
#else
    /* Fallback: Brian Kernighan's algorithm */
    uint8_t count = 0;
    while (x)
    {
        x &= (x - 1);
        count++;
    }
    return count;
#endif
}

#endif // (NON) TRIVIAL