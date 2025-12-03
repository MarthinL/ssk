/*
 * include/bitstream.h
 *
 * Bit-level operations for SSK encoding/decoding.
 *
 * SSK encodes set membership based on an abstract bit vector (Knuth's term),
 * NOT a SQL bitstring. Key distinction:
 *   - SQL bitstrings: human-readable literals, big-endian (left-to-right)
 *   - Abstract bit vector: machine-efficient set membership, little-endian
 *
 * The abstract bit vector is never fully realized in memory (would be 2^64 bits).
 * SSK exists only as compacted binary encoding.
 *
 * Bit ordering is LSB-first (little-endian), matching PostgreSQL's bitmapset.c:
 *   - Bit position 0 = bit 0 of byte 0 (least significant) = lowest ID
 *   - Bit position 7 = bit 7 of byte 0 (most significant)
 *   - Bit position 8 = bit 0 of byte 1
 *   - etc.
 *
 * This means: element ID maps directly to bit position (ID n → bit n).
 * CTZ (count trailing zeros) finds the lowest ID in a set.
 * CLZ (count leading zeros) finds the highest ID in a set.
 *
 * These functions are inline for performance - they're called frequently in
 * tight encode/decode loops and benefit from direct register access.
 *
 * IMPLEMENTATION NOTES:
 * - Uses 64-bit block operations where possible, not bit-by-bit loops
 * - Leverages GCC/Clang intrinsics for CLZ, CTZ, POPCOUNT
 * - Handles arbitrary bit alignment via shift/mask operations
 */

#ifndef BITSTREAM_H
#define BITSTREAM_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ============================================================================
 * COMPILER INTRINSICS
 * ============================================================================
 */

/**
 * Count leading zeros in a 64-bit value.
 * Returns 64 if value is 0.
 */
static inline int
bs_clz64(uint64_t x)
{
    return x ? __builtin_clzll(x) : 64;
}

/**
 * Count trailing zeros in a 64-bit value.
 * Returns 64 if value is 0.
 */
static inline int
bs_ctz64(uint64_t x)
{
    return x ? __builtin_ctzll(x) : 64;
}

/**
 * Count set bits (popcount) in a 64-bit value.
 */
static inline int
bs_popcount64(uint64_t x)
{
    return __builtin_popcountll(x);
}

/**
 * Find position of first (lowest) set bit.
 * Returns 0-63 for the bit position, or -1 if no bits set.
 * Position 0 is LSB (lowest ID), position 63 is MSB (highest ID).
 */
static inline int
bs_first_set_bit(uint64_t x)
{
    return x ? bs_ctz64(x) : -1;
}

/**
 * Find position of last (highest) set bit.
 * Returns 0-63 for the bit position, or -1 if no bits set.
 * Position 0 is LSB (lowest ID), position 63 is MSB (highest ID).
 */
static inline int
bs_last_set_bit(uint64_t x)
{
    return x ? (63 - bs_clz64(x)) : -1;
}

/* ============================================================================
 * 64-BIT BLOCK OPERATIONS
 * ============================================================================
 */

/**
 * Load a 64-bit block from buffer at byte-aligned position.
 * Little-endian: LSB of value = byte 0, preserves ID-to-bit mapping.
 */
static inline uint64_t
bs_load_block_aligned(const uint8_t *buf)
{
    uint64_t val;
    memcpy(&val, buf, 8);
    /* Little-endian: no conversion needed on x86/ARM, native order matches */
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    val = __builtin_bswap64(val);
#endif
    return val;
}

/**
 * Store a 64-bit block to buffer at byte-aligned position.
 * Little-endian: LSB of value → byte 0, preserves ID-to-bit mapping.
 */
static inline void
bs_store_block_aligned(uint8_t *buf, uint64_t val)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    val = __builtin_bswap64(val);
#endif
    memcpy(buf, &val, 8);
}

/**
 * Extract up to 64 bits from buffer at arbitrary bit position.
 * LSB-first: bit_pos 0 = bit 0 of byte 0 (lowest ID).
 *
 * @param buf       Source buffer (must have enough bytes)
 * @param bit_pos   Starting bit position (0 = LSB of byte 0)
 * @param n_bits    Number of bits to extract (1-64)
 * @return Value with bits right-aligned (LSBs contain the data)
 */
static inline uint64_t
bs_extract_bits(const uint8_t *buf, size_t bit_pos, uint8_t n_bits)
{
    if (n_bits == 0)
        return 0;
    
    size_t byte_offset = bit_pos / 8;
    unsigned int bit_offset = bit_pos % 8;  /* 0-7: position within first byte */
    
    unsigned int total_bits_in_bytes = bit_offset + n_bits;
    unsigned int bytes_needed = (total_bits_in_bytes + 7) / 8;
    
    if (bytes_needed <= 8)
    {
        /* Common case: fits in 8 bytes */
        /* Load bytes into a uint64_t, little-endian style */
        uint64_t raw = 0;
        for (unsigned int i = 0; i < bytes_needed; i++)
        {
            raw |= ((uint64_t)buf[byte_offset + i]) << (i * 8);
        }
        
        /* Shift down to align with bit_offset, then mask to n_bits */
        raw >>= bit_offset;
        
        if (n_bits < 64)
            raw &= (1ULL << n_bits) - 1;
        
        return raw;
    }
    else
    {
        /*
         * Rare case: spans 9 bytes (bit_offset > 0 and n_bits == 64)
         * Load 9 bytes, shift down by bit_offset.
         */
        uint64_t raw = 0;
        for (unsigned int i = 0; i < 8; i++)
        {
            raw |= ((uint64_t)buf[byte_offset + i]) << (i * 8);
        }
        
        /* Shift down by bit_offset */
        raw >>= bit_offset;
        
        /* Get remaining high bits from byte 9 */
        uint8_t byte9 = buf[byte_offset + 8];
        uint64_t high_bits = (uint64_t)byte9 << (64 - bit_offset);
        
        return raw | high_bits;
    }
}

/**
 * Insert up to 64 bits into buffer at arbitrary bit position.
 * LSB-first: bit_pos 0 = bit 0 of byte 0 (lowest ID).
 *
 * @param buf       Destination buffer (must have enough bytes)
 * @param bit_pos   Starting bit position (0 = LSB of byte 0)
 * @param value     Value to insert (right-aligned, LSBs contain the data)
 * @param n_bits    Number of bits to insert (1-64)
 */
static inline void
bs_insert_bits(uint8_t *buf, size_t bit_pos, uint64_t value, uint8_t n_bits)
{
    if (n_bits == 0)
        return;
    
    size_t byte_offset = bit_pos / 8;
    unsigned int bit_offset = bit_pos % 8;
    
    /* Mask value to n_bits */
    if (n_bits < 64)
        value &= (1ULL << n_bits) - 1;
    
    unsigned int total_bits_in_bytes = bit_offset + n_bits;
    unsigned int bytes_needed = (total_bits_in_bytes + 7) / 8;
    
    if (bytes_needed <= 8)
    {
        /* Common case: fits in 8 bytes */
        /* Load existing bytes, little-endian */
        uint64_t raw = 0;
        for (unsigned int i = 0; i < bytes_needed; i++)
        {
            raw |= ((uint64_t)buf[byte_offset + i]) << (i * 8);
        }
        
        /* Create mask for the bits we're replacing */
        uint64_t mask = ((n_bits < 64) ? ((1ULL << n_bits) - 1) : ~0ULL) << bit_offset;
        
        /* Clear old bits, insert new (shifted up by bit_offset) */
        raw = (raw & ~mask) | (value << bit_offset);
        
        /* Store back, little-endian */
        for (unsigned int i = 0; i < bytes_needed; i++)
        {
            buf[byte_offset + i] = (raw >> (i * 8)) & 0xFF;
        }
    }
    else
    {
        /*
         * Rare case: spans 9 bytes (bit_offset > 0 and n_bits == 64)
         * Write low portion to first 8 bytes, high portion to byte 9.
         */
        /* Load first 8 bytes */
        uint64_t raw = 0;
        for (unsigned int i = 0; i < 8; i++)
        {
            raw |= ((uint64_t)buf[byte_offset + i]) << (i * 8);
        }
        
        /* Mask for bits in first 8 bytes (from bit_offset to end) */
        uint64_t mask_low = ~((1ULL << bit_offset) - 1);
        raw = (raw & ~mask_low) | (value << bit_offset);
        
        /* Store first 8 bytes */
        for (unsigned int i = 0; i < 8; i++)
        {
            buf[byte_offset + i] = (raw >> (i * 8)) & 0xFF;
        }
        
        /* Handle byte 9: high bits of value go into low bits of byte 9 */
        unsigned int high_bits_count = bit_offset;  /* bits that spill into byte 9 */
        uint64_t high_val = value >> (64 - bit_offset);
        uint8_t byte9 = buf[byte_offset + 8];
        uint8_t mask9 = (1 << high_bits_count) - 1;
        byte9 = (byte9 & ~mask9) | (high_val & mask9);
        buf[byte_offset + 8] = byte9;
    }
}

/* ============================================================================
 * PUBLIC API - MAIN FUNCTIONS
 * ============================================================================
 */

/**
 * Write bits to a buffer at a specified bit position.
 *
 * @param dst       Destination buffer (must have space for bits being written)
 * @param dst_bit   Starting bit position in destination (0 = LSB of byte 0)
 * @param value     Value to write (right-aligned, i.e., LSBs contain the data)
 * @param n_bits    Number of bits to write (0-64)
 */
static inline void
bs_write_bits(uint8_t *dst, size_t dst_bit, uint64_t value, uint8_t n_bits)
{
    bs_insert_bits(dst, dst_bit, value, n_bits);
}

/**
 * Read bits from a buffer at a specified bit position.
 *
 * @param src       Source buffer
 * @param src_bit   Starting bit position in source (0 = LSB of byte 0)
 * @param n_bits    Number of bits to read (0-64)
 * @return Value with bits right-aligned (LSBs contain the data)
 */
static inline uint64_t
bs_read_bits(const uint8_t *src, size_t src_bit, uint8_t n_bits)
{
    return bs_extract_bits(src, src_bit, n_bits);
}

/**
 * Copy bits from one buffer position to another.
 *
 * @param src       Source buffer
 * @param src_bit   Starting bit position in source
 * @param dst       Destination buffer (may be same as src if ranges don't overlap)
 * @param dst_bit   Starting bit position in destination
 * @param n_bits    Number of bits to copy
 */
static inline void
bs_copy_bits(const uint8_t *src, size_t src_bit,
             uint8_t *dst, size_t dst_bit, size_t n_bits)
{
    /* Copy in 64-bit chunks where possible */
    while (n_bits >= 64)
    {
        uint64_t block = bs_extract_bits(src, src_bit, 64);
        bs_insert_bits(dst, dst_bit, block, 64);
        src_bit += 64;
        dst_bit += 64;
        n_bits -= 64;
    }
    
    /* Handle remaining bits */
    if (n_bits > 0)
    {
        uint64_t block = bs_extract_bits(src, src_bit, (uint8_t)n_bits);
        bs_insert_bits(dst, dst_bit, block, (uint8_t)n_bits);
    }
}

/**
 * Compute bytes needed to hold n bits.
 *
 * @param n_bits Number of bits
 * @return Minimum bytes needed (ceiling division)
 */
static inline size_t
bs_bytes_for_bits(size_t n_bits)
{
    return (n_bits + 7) / 8;
}

/**
 * Set a single bit in a buffer.
 *
 * @param buf      Buffer
 * @param bit_pos  Bit position (0 = LSB of byte 0 = lowest ID)
 */
static inline void
bs_set_bit(uint8_t *buf, size_t bit_pos)
{
    size_t byte_idx = bit_pos / 8;
    unsigned int bit_idx = bit_pos % 8;
    buf[byte_idx] |= (1 << bit_idx);
}

/**
 * Clear a single bit in a buffer.
 *
 * @param buf      Buffer
 * @param bit_pos  Bit position (0 = LSB of byte 0 = lowest ID)
 */
static inline void
bs_clear_bit(uint8_t *buf, size_t bit_pos)
{
    size_t byte_idx = bit_pos / 8;
    unsigned int bit_idx = bit_pos % 8;
    buf[byte_idx] &= ~(1 << bit_idx);
}

/**
 * Test a single bit in a buffer.
 *
 * @param buf      Buffer
 * @param bit_pos  Bit position (0 = LSB of byte 0 = lowest ID)
 * @return 1 if bit is set, 0 if clear
 */
static inline int
bs_test_bit(const uint8_t *buf, size_t bit_pos)
{
    size_t byte_idx = bit_pos / 8;
    unsigned int bit_idx = bit_pos % 8;
    return (buf[byte_idx] >> bit_idx) & 1;
}

/* ============================================================================
 * BLOCK ANALYSIS PRIMITIVES (POLARITY-AWARE, PARTIAL-BLOCK)
 * ============================================================================
 *
 * SSK encoding works in terms of "dominant" and "rare" bits, not "set" and
 * "unset". The dominant bit is whichever value (0 or 1) appears more often
 * in the data - this determines encoding polarity.
 *
 * These primitives:
 *   1. Accept a bit count (n_bits) for partial blocks - only the LOW n_bits
 *      are valid, the rest are masked out before analysis.
 *   2. Accept a dominant parameter (0 or 1) to define polarity.
 *   3. Return positions relative to LSB (position 0 = bit 0 = lowest ID).
 *
 * A "rare" bit is any bit that differs from the dominant value.
 * When dominant=1, rare bits are 0s. When dominant=0, rare bits are 1s.
 */

/**
 * Mask a block to its valid low bits.
 * Given n_bits valid bits, clears the high (64-n_bits) bits.
 *
 * @param block   The 64-bit block (data in LOW bits, LSB-first)
 * @param n_bits  Number of valid bits (1-64), positioned at LSB
 * @return Block with only valid bits, rest cleared
 */
static inline uint64_t
bs_mask_block(uint64_t block, uint8_t n_bits)
{
    if (n_bits >= 64)
        return block;
    if (n_bits == 0)
        return 0;
    /* Clear high (64 - n_bits) bits */
    return block & ((1ULL << n_bits) - 1);
}

/**
 * Get a block with rare bits marked as 1s.
 * Converts the block based on polarity so that 1s represent rare positions.
 *
 * @param block     The 64-bit block (masked to valid bits)
 * @param dominant  The dominant bit value (0 or 1)
 * @return Block where 1s mark rare bit positions
 */
static inline uint64_t
bs_rare_bits(uint64_t block, int dominant)
{
    return dominant ? ~block : block;
}

/**
 * Count rare bits in a partial block.
 *
 * @param block     The 64-bit block
 * @param n_bits    Number of valid bits (1-64)
 * @param dominant  The dominant bit value (0 or 1)
 * @return Number of rare bits in the valid portion
 */
static inline int
bs_count_rare(uint64_t block, uint8_t n_bits, int dominant)
{
    uint64_t masked = bs_mask_block(block, n_bits);
    uint64_t rare = bs_rare_bits(masked, dominant);
    /* If dominant=1, we inverted, so need to re-mask (inversion flips low bits too) */
    if (dominant)
        rare = bs_mask_block(rare, n_bits);
    return bs_popcount64(rare);
}

/**
 * Find position of first (lowest) rare bit in a partial block.
 *
 * @param block     The 64-bit block
 * @param n_bits    Number of valid bits (1-64)
 * @param dominant  The dominant bit value (0 or 1)
 * @return Position 0-(n_bits-1) of first rare bit, or -1 if none
 *         Position 0 is LSB (lowest ID).
 */
static inline int
bs_first_rare(uint64_t block, uint8_t n_bits, int dominant)
{
    uint64_t masked = bs_mask_block(block, n_bits);
    uint64_t rare = bs_rare_bits(masked, dominant);
    if (dominant)
        rare = bs_mask_block(rare, n_bits);
    
    if (rare == 0)
        return -1;
    
    /* CTZ gives position from LSB (lowest ID) */
    return bs_ctz64(rare);
}

/**
 * Find position of last (highest) rare bit in a partial block.
 *
 * @param block     The 64-bit block
 * @param n_bits    Number of valid bits (1-64)
 * @param dominant  The dominant bit value (0 or 1)
 * @return Position 0-(n_bits-1) of last rare bit, or -1 if none
 *         Position 0 is LSB (lowest ID).
 */
static inline int
bs_last_rare(uint64_t block, uint8_t n_bits, int dominant)
{
    uint64_t masked = bs_mask_block(block, n_bits);
    uint64_t rare = bs_rare_bits(masked, dominant);
    if (dominant)
        rare = bs_mask_block(rare, n_bits);
    
    if (rare == 0)
        return -1;
    
    /* CLZ from MSB, convert to position from LSB */
    return 63 - bs_clz64(rare);
}

/**
 * Check if a partial block is homogeneous (all dominant bits).
 *
 * @param block     The 64-bit block
 * @param n_bits    Number of valid bits (1-64)
 * @return 1 if all valid bits are the same (all 0 or all 1), 0 otherwise
 *         Also returns the dominant value via pointer if non-NULL.
 */
static inline int
bs_is_homogeneous(uint64_t block, uint8_t n_bits, int *dominant_out)
{
    uint64_t masked = bs_mask_block(block, n_bits);
    
    /* All zeros? */
    if (masked == 0)
    {
        if (dominant_out) *dominant_out = 0;
        return 1;
    }
    
    /* All ones in valid region? */
    uint64_t all_ones = (n_bits >= 64) ? ~0ULL : ((1ULL << n_bits) - 1);
    if (masked == all_ones)
    {
        if (dominant_out) *dominant_out = 1;
        return 1;
    }
    
    return 0;
}

/**
 * Determine dominant bit value for a partial block.
 * Returns whichever value (0 or 1) appears more often.
 * Ties go to 0 (arbitrary but consistent).
 *
 * @param block     The 64-bit block
 * @param n_bits    Number of valid bits (1-64)
 * @return 0 if zeros dominate or tie, 1 if ones dominate
 */
static inline int
bs_dominant_bit(uint64_t block, uint8_t n_bits)
{
    uint64_t masked = bs_mask_block(block, n_bits);
    int ones = bs_popcount64(masked);
    int zeros = n_bits - ones;
    return (ones > zeros) ? 1 : 0;
}

/**
 * Full block analysis structure for encoding decisions.
 */
typedef struct {
    uint64_t value;        /* The masked block value */
    uint8_t  n_bits;       /* Valid bits in this block */
    int      dominant;     /* Dominant bit value (0 or 1) */
    int      rare_count;   /* Number of rare bits */
    int      first_rare;   /* Position of first rare bit, or -1 */
    int      last_rare;    /* Position of last rare bit, or -1 */
} BlockAnalysis;

/**
 * Analyze a partial block for encoding decisions.
 * Determines polarity automatically based on bit counts.
 *
 * @param block   The 64-bit block (data in LOW bits)
 * @param n_bits  Number of valid bits (1-64)
 * @return Full analysis with polarity, rare bit positions, etc.
 */
static inline BlockAnalysis
bs_analyze_block(uint64_t block, uint8_t n_bits)
{
    BlockAnalysis a;
    a.value = bs_mask_block(block, n_bits);
    a.n_bits = n_bits;
    a.dominant = bs_dominant_bit(a.value, n_bits);
    a.rare_count = bs_count_rare(a.value, n_bits, a.dominant);
    a.first_rare = bs_first_rare(a.value, n_bits, a.dominant);
    a.last_rare = bs_last_rare(a.value, n_bits, a.dominant);
    return a;
}

/**
 * Analyze a partial block with explicit polarity.
 * Use when polarity is predetermined (e.g., from partition header).
 *
 * @param block     The 64-bit block (data in LOW bits)
 * @param n_bits    Number of valid bits (1-64)
 * @param dominant  Predetermined dominant bit value
 * @return Full analysis using the given polarity
 */
static inline BlockAnalysis
bs_analyze_block_polar(uint64_t block, uint8_t n_bits, int dominant)
{
    BlockAnalysis a;
    a.value = bs_mask_block(block, n_bits);
    a.n_bits = n_bits;
    a.dominant = dominant;
    a.rare_count = bs_count_rare(a.value, n_bits, dominant);
    a.first_rare = bs_first_rare(a.value, n_bits, dominant);
    a.last_rare = bs_last_rare(a.value, n_bits, dominant);
    return a;
}

/* ============================================================================
 * RUN DETECTION (FOR RLE ENCODING)
 * ============================================================================
 */

/**
 * Count leading dominant bits in a partial block.
 * Returns how many consecutive dominant bits from the LSB (lowest ID).
 *
 * @param block     The 64-bit block
 * @param n_bits    Number of valid bits (1-64)
 * @param dominant  The dominant bit value (0 or 1)
 * @return Count of leading dominant bits (0 to n_bits)
 */
static inline int
bs_leading_dominant(uint64_t block, uint8_t n_bits, int dominant)
{
    int first = bs_first_rare(block, n_bits, dominant);
    return (first < 0) ? n_bits : first;
}

/**
 * Count trailing dominant bits in a partial block.
 * Returns how many consecutive dominant bits from the MSB (highest ID) of valid region.
 *
 * @param block     The 64-bit block
 * @param n_bits    Number of valid bits (1-64)
 * @param dominant  The dominant bit value (0 or 1)
 * @return Count of trailing dominant bits (0 to n_bits)
 */
static inline int
bs_trailing_dominant(uint64_t block, uint8_t n_bits, int dominant)
{
    int last = bs_last_rare(block, n_bits, dominant);
    return (last < 0) ? n_bits : (n_bits - 1 - last);
}

#endif /* BITSTREAM_H */
