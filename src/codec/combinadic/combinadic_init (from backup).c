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
 * binomial[n][k] = C(n,k) for n in [0,64], k in [0,18]
 * ssk_rank_bits[n][k] = ceil(log2(C(n,k))) for same range (exported in ssk_format.h)
 *
 * Total memory: 65 * 19 * 8 bytes (binomial) + 65 * 19 * 1 byte (rank_bits)
 *             = 9,880 + 1,235 = ~11 KB
 * ============================================================================
 */

/* Binomial coefficients C(n,k) - initialized at module load */
static uint64_t binomial[SSK_RANK_BITS_N_MAX + 1][SSK_RANK_BITS_K_MAX + 1];

/* Bits needed to encode rank for C(n,k) - exported global (declared in ssk_format.h) */

__attribute__((visibility("default"))) uint8_t ssk_rank_bits[SSK_RANK_BITS_N_MAX + 1][SSK_RANK_BITS_K_MAX + 1];

/* Initialization flag */
static bool tables_initialized = false;

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
 * Call once at module load. Safe to call multiple times (idempotent).
 */
void
ssk_combinadic_init(void)
{
    if (tables_initialized)
        return;
    
    /* Initialize binomial coefficients using Pascal's triangle */
    for (int n = 0; n <= SSK_RANK_BITS_N_MAX; n++)
    {
        for (int k = 0; k <= SSK_RANK_BITS_K_MAX; k++)
        {
            if (k > n)
            {
                /* C(n,k) = 0 when k > n */
                binomial[n][k] = 0;
                ssk_rank_bits[n][k] = 0;
            }
            else if (k == 0 || k == n)
            {
                /* C(n,0) = C(n,n) = 1 */
                binomial[n][k] = 1;
                ssk_rank_bits[n][k] = 0;  /* Only one possibility, 0 bits needed */
            }
            else
            {
                /* Pascal's recurrence: C(n,k) = C(n-1,k-1) + C(n-1,k) */
                binomial[n][k] = binomial[n-1][k-1] + binomial[n-1][k];
                ssk_rank_bits[n][k] = ceil_log2(binomial[n][k]);
            }
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
    
    return binomial[n][k];
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
    
    return ssk_rank_bits[n][k];
}

/* ============================================================================
 * COMBINADIC RANK (Encode)
 *
 * Convert a k-subset (represented as a bit pattern) to its combinadic rank.
 *
 * Algorithm: Iterate through set bit positions in ascending order (LSB to MSB).
 * For each set bit at position p (0-indexed from LSB), add C(p, remaining_k).
 *
 * Example: bits=0b101010 (n=6, k=3, positions {1,3,5})
 *   Start with rank=0, remaining_k=3
 *   Position 1: rank += C(1,3) = 0
 *   Position 3: rank += C(3,2) = 3
 *   Position 5: rank += C(5,1) = 5
 *   Final rank = 8
 * ============================================================================
 */

/**
 * Compute combinadic rank of a bit pattern.
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
    if (!tables_initialized)
        ssk_combinadic_init();
    
    if (k == 0)
        return 0;  /* Only one way to choose 0 elements */
    
    if (k > SSK_RANK_BITS_K_MAX)
        return 0;  /* Out of table range - caller should use RAW encoding */
    
    uint64_t rank = 0;
    uint8_t remaining_k = k;
    
    /* Scan from LSB (position 0) up to MSB (position n-1) */
    for (int pos = 0; pos < n; pos++)
    {
        if (bits & (1ULL << pos))
        {
            /* This position is set - add C(pos, remaining_k) to rank */
            rank += binomial[pos][remaining_k];
            remaining_k--;
        }
    }
    
    return rank;
}

/* ============================================================================
 * COMBINADIC UNRANK (Decode)
 *
 * Convert a combinadic rank back to its k-subset bit pattern.
 *
 * Algorithm: Greedily find the lowest position p such that C(p, remaining_k) <= remaining_rank.
 * Set that bit, subtract C(p, remaining_k) from rank, decrement remaining_k.
 *
 * Example: rank=8, n=6, k=3
 *   remaining_k=3, remaining_rank=8
 *   Try p=0: C(0,3)=0 <= 8, set bit 0, rank=8, k=2
 *   Try p=1: C(1,2)=0 <= 8, set bit 1, rank=8, k=1
 *   Try p=2: C(2,1)=2 <= 8, set bit 2, rank=6, k=0
 *   Result: bits = 0b000111
 * ============================================================================
 */

/**
 * Reconstruct bit pattern from combinadic rank.
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
    if (!tables_initialized)
        ssk_combinadic_init();
    
    if (k == 0)
        return 0;  /* No bits to set */
    
    if (k > SSK_RANK_BITS_K_MAX || k > n)
        return 0;  /* Invalid parameters */
    
    uint64_t bits = 0;
    uint64_t remaining_rank = rank;
    uint8_t remaining_k = k;
    
    /* Scan from LSB (position 0) up to MSB (position n-1) */
    for (int pos = 0; pos < n && remaining_k > 0; pos++)
    {
        uint64_t c = binomial[pos][remaining_k];
        
        if (c <= remaining_rank)
        {
            /* Set this bit */
            bits |= (1ULL << pos);
            remaining_rank -= c;
            remaining_k--;
        }
    }
    
    return bits;
}

/* ============================================================================
 * VALIDATION HELPERS
 * ============================================================================
 */

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
    
    return rank < binomial[n][k];
}