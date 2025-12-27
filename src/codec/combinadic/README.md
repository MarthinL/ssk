# Combinadic (Combinatorial Number System)

Rank and unrank operations for sparse bit patterns.

## Responsibilities

- Initialize C(n,k) and rank_bits[n][k] tables at module load
- Rank: Convert bit pattern to combinadic rank (chunk → integer)
- Unrank: Convert combinadic rank to bit pattern (integer → chunk)

## Theory

Combinadic numbers provide a bijection between:
- k-element subsets of an n-element set
- Integers in range [0, C(n,k)-1]

For SSK: n ∈ [1..64] (chunk size), k ∈ [0..18] (popcount threshold)

## Precomputation

Tables computed once at extension load using Pascal's triangle:
- binomial_coeff[n][k]: C(n,k) values
- rank_bits[n][k]: ceil(log2(C(n,k))) - bits needed to encode rank

## Files

- combinadic_init.c: Table initialization (Pascal's triangle)
- combinadic_rank.c: Rank operation (bit pattern → rank)
- combinadic_unrank.c: Unrank operation (rank → bit pattern)

## Related Concerns

- ../chunks/: Uses combinadic for ENUM token encoding
