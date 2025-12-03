/*
 * tests/test_codec_validation.c
 *
 * Hand-crafted SSKDecoded structures for codec validation.
 *
 * Since SSKDecoded uses a flexible array member (FAM), we cannot use
 * struct composition. Instead we use a union overlay: the fixed header
 * plus a byte buffer that contains everything including the header.
 *
 * The structure is documented inline with the data.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../include/ssk_decoded.h"
#include "../include/ssk_format.h"

/*
 * Structure sizes (verified):
 *   SSKDecoded:   32 bytes (header), data[] starts at offset 32
 *   SSKPartition: 32 bytes
 *   SSKSegment:   40 bytes
 *   SSKChunk:     24 bytes
 */
#define DECODED_HDR  32
#define PARTITION_SZ 32
#define SEGMENT_SZ   40
#define CHUNK_SZ     24

/* ============================================================================
 * BITMAP DISPLAY
 *
 * Print bitmap as list of set bit positions: {5, 10, 42}
 * ============================================================================
 */

static void
print_bitmap_as_ids(uint64_t bitmap)
{
    printf("{");
    bool first = true;
    for (int i = 0; i < 64; i++) {
        if (bitmap & (1ULL << i)) {
            if (!first) printf(", ");
            printf("%d", i);
            first = false;
        }
    }
    printf("}");
}

/* ============================================================================
 * HELPER: Write structures into a byte buffer at given offset
 * ============================================================================
 */

static inline void
put_u32(uint8_t *buf, size_t off, uint32_t val)
{
    memcpy(buf + off, &val, 4);
}

static inline void
put_u64(uint8_t *buf, size_t off, uint64_t val)
{
    memcpy(buf + off, &val, 8);
}

static inline void
put_partition(uint8_t *buf, size_t off, SSKPartition *p)
{
    memcpy(buf + off, p, PARTITION_SZ);
}

static inline void
put_segment(uint8_t *buf, size_t off, SSKSegment *s)
{
    memcpy(buf + off, s, SEGMENT_SZ);
}

static inline void
put_chunk(uint8_t *buf, size_t off, SSKChunk *c)
{
    memcpy(buf + off, c, CHUNK_SZ);
}

/* ============================================================================
 * VECTOR 1: Empty set
 * ============================================================================
 */

static uint8_t empty_buf[DECODED_HDR];
static SSKDecoded *empty_set;

static void init_empty_set(void)
{
    SSKDecoded hdr = {
        .format_version = 0,
        .n_partitions = 0,
        .partition_offs = 0,
        .var_data_off = 0,
        .var_data_allocated = 0,
        .var_data_used = 0,
        .allocated_size = sizeof(empty_buf),
    };
    memcpy(empty_buf, &hdr, DECODED_HDR);
    empty_set = (SSKDecoded *)empty_buf;
}

/* ============================================================================
 * VECTOR 2: Single element {42}
 *
 * Layout in data[]:
 *   0:   part_off[0] = 4         (offset to partition)
 *   4:   SSKPartition            (32 bytes)
 *  36:   seg_off[0] = 40-4=36    (relative to partition start)
 *  40:   SSKSegment              (40 bytes)  
 *  80:   chunk_off[0] = 84-40=44 (relative to segment start)
 *  84:   SSKChunk                (24 bytes)
 * 108:   bitmap                  (8 bytes)
 * ============================================================================
 */

#define V2_TOTAL (4 + PARTITION_SZ + 4 + SEGMENT_SZ + 4 + CHUNK_SZ + 8)
static uint8_t single_42_buf[DECODED_HDR + V2_TOTAL];
static SSKDecoded *single_42;

static void init_single_42(void)
{
    /* data[] offsets */
    const size_t part_arr   = 0;
    const size_t part_start = 4;
    const size_t seg_arr    = part_start + PARTITION_SZ;        /* 36 */
    const size_t seg_start  = seg_arr + 4;                      /* 40 */
    const size_t chunk_arr  = seg_start + SEGMENT_SZ;           /* 80 */
    const size_t chunk_start= chunk_arr + 4;                    /* 84 */
    const size_t bitmap_off = chunk_start + CHUNK_SZ;           /* 108 */
    
    uint8_t *data = single_42_buf + DECODED_HDR;
    
    /* Header */
    SSKDecoded hdr = {
        .format_version = 0,
        .n_partitions = 1,
        .partition_offs = part_arr,
        .var_data_off = part_arr,
        .var_data_used = V2_TOTAL,
        .var_data_allocated = V2_TOTAL,
        .allocated_size = sizeof(single_42_buf),
    };
    memcpy(single_42_buf, &hdr, DECODED_HDR);
    
    /* part_off[0] -> partition at offset 4 */
    put_u32(data, part_arr, part_start);
    
    /* Partition */
    SSKPartition part = {
        .partition_id = 0,
        .segment_count = 1,
        .segments_off = seg_arr - part_start,
        .var_data_off = seg_arr - part_start,
        .var_data_used = V2_TOTAL - seg_arr,
        .cardinality = 1,
    };
    put_partition(data, part_start, &part);
    
    /* seg_off[0] -> segment at offset 40 (relative to partition = 36) */
    put_u32(data, seg_arr, seg_start - part_start);
    
    /* Segment: MIX, 1 word, 1 chunk */
    SSKSegment seg = {
        .segment_type = SEG_MIX,
        .segment_len = 1,
        .chunk_count = 1,
        .chunks_off = chunk_arr - seg_start,
        .var_data_off = chunk_arr - seg_start,
        .var_data_used = bitmap_off + 8 - chunk_arr,
        .cardinality = 1,
    };
    put_segment(data, seg_start, &seg);
    
    /* chunk_off[0] -> chunk at offset 84 (relative to segment = 44) */
    put_u32(data, chunk_arr, chunk_start - seg_start);
    
    /* Chunk: ENUM, k=1 */
    SSKChunk chunk = {
        .chunk_type = TOK_ENUM,
        .token_count = 1,
        .tokens_off = bitmap_off - seg_start,
        .cardinality = 1,
    };
    put_chunk(data, chunk_start, &chunk);
    
    /* Bitmap: bit 42 set */
    put_u64(data, bitmap_off, 1ULL << 42);
    
    single_42 = (SSKDecoded *)single_42_buf;
}

/* ============================================================================
 * VECTOR 3: Sparse {10, 20, 30}
 * Same layout as single_42, just different bitmap and k=3
 * ============================================================================
 */

static uint8_t sparse_3_buf[DECODED_HDR + V2_TOTAL];
static SSKDecoded *sparse_3;

static void init_sparse_3(void)
{
    /* Copy structure from single_42, update cardinality and bitmap */
    memcpy(sparse_3_buf, single_42_buf, sizeof(sparse_3_buf));
    
    uint8_t *data = sparse_3_buf + DECODED_HDR;
    
    /* Update partition cardinality */
    SSKPartition *part = (SSKPartition *)(data + 4);
    part->cardinality = 3;
    
    /* Update segment cardinality */
    SSKSegment *seg = (SSKSegment *)(data + 40);
    seg->cardinality = 3;
    
    /* Update chunk cardinality */
    SSKChunk *chunk = (SSKChunk *)(data + 84);
    chunk->cardinality = 3;
    
    /* Update bitmap: {10, 20, 30} */
    put_u64(data, 108, (1ULL << 10) | (1ULL << 20) | (1ULL << 30));
    
    sparse_3 = (SSKDecoded *)sparse_3_buf;
}

/* ============================================================================
 * VECTOR 4: Dense word (all 64 bits)
 *
 * Layout in data[]:
 *   0:   part_off[0] = 4
 *   4:   SSKPartition (32 bytes)
 *  36:   seg_off[0]
 *  40:   SSKSegment (RLE, no chunks) (40 bytes)
 * ============================================================================
 */

#define V4_TOTAL (4 + PARTITION_SZ + 4 + SEGMENT_SZ)
static uint8_t dense_buf[DECODED_HDR + V4_TOTAL];
static SSKDecoded *dense_word;

static void init_dense_word(void)
{
    const size_t part_arr   = 0;
    const size_t part_start = 4;
    const size_t seg_arr    = part_start + PARTITION_SZ;
    const size_t seg_start  = seg_arr + 4;
    
    uint8_t *data = dense_buf + DECODED_HDR;
    
    SSKDecoded hdr = {
        .format_version = 0,
        .n_partitions = 1,
        .partition_offs = part_arr,
        .var_data_off = part_arr,
        .var_data_used = V4_TOTAL,
        .var_data_allocated = V4_TOTAL,
        .allocated_size = sizeof(dense_buf),
    };
    memcpy(dense_buf, &hdr, DECODED_HDR);
    
    put_u32(data, part_arr, part_start);
    
    SSKPartition part = {
        .partition_id = 0,
        .segment_count = 1,
        .segments_off = seg_arr - part_start,
        .var_data_off = seg_arr - part_start,
        .var_data_used = V4_TOTAL - seg_arr,
        .cardinality = 64,
    };
    put_partition(data, part_start, &part);
    
    put_u32(data, seg_arr, seg_start - part_start);
    
    SSKSegment seg = {
        .segment_type = SEG_RLE,
        .segment_len = 64,      /* 64 bits all set */
        .chunk_count = 0,       /* RLE has no chunks */
        .cardinality = 64,
    };
    put_segment(data, seg_start, &seg);
    
    dense_word = (SSKDecoded *)dense_buf;
}

/* ============================================================================
 * VECTOR 5: RAW {1,3,5,...,63} (k=32)
 * Same layout as single_42, but RAW token type
 * ============================================================================
 */

static uint8_t raw_32_buf[DECODED_HDR + V2_TOTAL];
static SSKDecoded *raw_32;

static void init_raw_32(void)
{
    memcpy(raw_32_buf, single_42_buf, sizeof(raw_32_buf));
    
    uint8_t *data = raw_32_buf + DECODED_HDR;
    
    SSKPartition *part = (SSKPartition *)(data + 4);
    part->cardinality = 32;
    
    SSKSegment *seg = (SSKSegment *)(data + 40);
    seg->cardinality = 32;
    
    SSKChunk *chunk = (SSKChunk *)(data + 84);
    chunk->chunk_type = TOK_RAW;
    chunk->cardinality = 32;
    
    /* Bitmap: 0xAAAA... = bits 1,3,5,...,63 */
    put_u64(data, 108, 0xAAAAAAAAAAAAAAAAULL);
    
    raw_32 = (SSKDecoded *)raw_32_buf;
}

/* ============================================================================
 * VECTOR 6: Multi-segment (MIX-RLE-MIX)
 *
 * Word 0: {5, 10}      -> MIX, ENUM k=2
 * Words 1-2: all 128   -> RLE, membership=1
 * Word 3: {8}          -> MIX, ENUM k=1 (global bit 200)
 *
 * Layout:
 *   0:   part_off[0] = 4
 *   4:   SSKPartition
 *  36:   seg_off[0..2]  (3 * 4 = 12 bytes)
 *  48:   SSKSegment 0 (MIX)
 *  88:   chunk_off[0]
 *  92:   SSKChunk 0
 * 116:   bitmap 0
 * 124:   SSKSegment 1 (RLE)
 * 164:   SSKSegment 2 (MIX)
 * 204:   chunk_off[0]
 * 208:   SSKChunk 2
 * 232:   bitmap 2
 * ============================================================================
 */

#define V6_TOTAL (4 + PARTITION_SZ + 12 + \
                  (SEGMENT_SZ + 4 + CHUNK_SZ + 8) + \
                  SEGMENT_SZ + \
                  (SEGMENT_SZ + 4 + CHUNK_SZ + 8))
static uint8_t multi_buf[DECODED_HDR + V6_TOTAL];
static SSKDecoded *multi_seg;

static void init_multi_seg(void)
{
    const size_t part_arr    = 0;
    const size_t part_start  = 4;
    const size_t seg_arr     = part_start + PARTITION_SZ;       /* 36 */
    const size_t seg0_start  = seg_arr + 12;                    /* 48 */
    const size_t chunk0_arr  = seg0_start + SEGMENT_SZ;         /* 88 */
    const size_t chunk0_start= chunk0_arr + 4;                  /* 92 */
    const size_t bitmap0_off = chunk0_start + CHUNK_SZ;         /* 116 */
    const size_t seg1_start  = bitmap0_off + 8;                 /* 124 */
    const size_t seg2_start  = seg1_start + SEGMENT_SZ;         /* 164 */
    const size_t chunk2_arr  = seg2_start + SEGMENT_SZ;         /* 204 */
    const size_t chunk2_start= chunk2_arr + 4;                  /* 208 */
    const size_t bitmap2_off = chunk2_start + CHUNK_SZ;         /* 232 */
    
    uint8_t *data = multi_buf + DECODED_HDR;
    
    SSKDecoded hdr = {
        .format_version = 0,
        .n_partitions = 1,
        .partition_offs = part_arr,
        .var_data_off = part_arr,
        .var_data_used = V6_TOTAL,
        .var_data_allocated = V6_TOTAL,
        .allocated_size = sizeof(multi_buf),
    };
    memcpy(multi_buf, &hdr, DECODED_HDR);
    
    put_u32(data, part_arr, part_start);
    
    SSKPartition part = {
        .partition_id = 0,
        .segment_count = 3,
        .segments_off = seg_arr - part_start,
        .var_data_off = seg_arr - part_start,
        .var_data_used = V6_TOTAL - seg_arr,
        .cardinality = 2 + 128 + 1,
    };
    put_partition(data, part_start, &part);
    
    /* Segment offset array */
    put_u32(data, seg_arr + 0, seg0_start - part_start);
    put_u32(data, seg_arr + 4, seg1_start - part_start);
    put_u32(data, seg_arr + 8, seg2_start - part_start);
    
    /* Segment 0: MIX, word 0, {5, 10} */
    SSKSegment seg0 = {
        .segment_type = SEG_MIX,
        .segment_len = 1,
        .chunk_count = 1,
        .chunks_off = chunk0_arr - seg0_start,
        .var_data_off = chunk0_arr - seg0_start,
        .var_data_used = bitmap0_off + 8 - chunk0_arr,
        .cardinality = 2,
    };
    put_segment(data, seg0_start, &seg0);
    put_u32(data, chunk0_arr, chunk0_start - seg0_start);
    SSKChunk chunk0 = {
        .chunk_type = TOK_ENUM,
        .token_count = 1,
        .tokens_off = bitmap0_off - seg0_start,
        .cardinality = 2,
    };
    put_chunk(data, chunk0_start, &chunk0);
    put_u64(data, bitmap0_off, (1ULL << 5) | (1ULL << 10));
    
    /* Segment 1: RLE, 128 bits all 1s */
    SSKSegment seg1 = {
        .segment_type = SEG_RLE,
        .segment_len = 128,
        .chunk_count = 0,
        .cardinality = 128,
    };
    put_segment(data, seg1_start, &seg1);
    
    /* Segment 2: MIX, word 3, {8} (global bit 200) */
    SSKSegment seg2 = {
        .segment_type = SEG_MIX,
        .segment_len = 1,
        .chunk_count = 1,
        .chunks_off = chunk2_arr - seg2_start,
        .var_data_off = chunk2_arr - seg2_start,
        .var_data_used = bitmap2_off + 8 - chunk2_arr,
        .cardinality = 1,
    };
    put_segment(data, seg2_start, &seg2);
    put_u32(data, chunk2_arr, chunk2_start - seg2_start);
    SSKChunk chunk2 = {
        .chunk_type = TOK_ENUM,
        .token_count = 1,
        .tokens_off = bitmap2_off - seg2_start,
        .cardinality = 1,
    };
    put_chunk(data, chunk2_start, &chunk2);
    put_u64(data, bitmap2_off, 1ULL << 8);
    
    multi_seg = (SSKDecoded *)multi_buf;
}

/* ============================================================================
 * PRINT DECODED STRUCTURE
 * ============================================================================
 */

static void
print_decoded(const SSKDecoded *d, const char *name)
{
    printf("\n=== %s ===\n", name);
    printf("format_version=%u, n_partitions=%u\n", 
           d->format_version, d->n_partitions);
    
    if (d->n_partitions == 0) {
        printf("  (empty set)\n");
        return;
    }
    
    const uint32_t *part_offs = (const uint32_t *)&d->data[d->partition_offs];
    for (uint32_t p = 0; p < d->n_partitions; p++) {
        const SSKPartition *part = (const SSKPartition *)&d->data[part_offs[p]];
        printf("\n  Partition %u: id=%u, cardinality=%lu\n",
               p, part->partition_id, (unsigned long)part->cardinality);
        
        const uint32_t *seg_offs = (const uint32_t *)((const uint8_t *)part + part->segments_off);
        for (uint32_t s = 0; s < part->segment_count; s++) {
            const SSKSegment *seg = (const SSKSegment *)((const uint8_t *)part + seg_offs[s]);
            const char *stype = (seg->segment_type == SEG_RLE) ? "RLE" : "MIX";
            
            printf("    Segment %u: %s, %u bits, cardinality=%lu\n",
                   s, stype, seg->segment_len, (unsigned long)seg->cardinality);
            
            if (seg->segment_type == SEG_RLE) {
                printf("      (all 1s)\n");
                continue;
            }
            
            const uint32_t *chunk_offs = (const uint32_t *)((const uint8_t *)seg + seg->chunks_off);
            for (uint32_t c = 0; c < seg->chunk_count; c++) {
                const SSKChunk *chunk = (const SSKChunk *)((const uint8_t *)seg + chunk_offs[c]);
                const char *ctype = 
                    (chunk->chunk_type == TOK_ENUM) ? "ENUM" :
                    (chunk->chunk_type == TOK_RAW) ? "RAW" : "RAW_RUN";
                
                const uint64_t *bitmap = (const uint64_t *)((const uint8_t *)seg + chunk->tokens_off);
                printf("      Chunk %u: %s k=%lu, bits=", 
                       c, ctype, (unsigned long)chunk->cardinality);
                print_bitmap_as_ids(*bitmap);
                printf("\n");
            }
        }
    }
}

/* ============================================================================
 * TEST HARNESS
 * ============================================================================
 */

typedef struct {
    const char *name;
    SSKDecoded **decoded;
} TestVector;

static TestVector vectors[] = {
    { "Empty Set",                      &empty_set },
    { "Single {42}",                    &single_42 },
    { "Sparse {10,20,30}",              &sparse_3 },
    { "Dense (all 64 bits)",            &dense_word },
    { "RAW {1,3,5,...,63} (k=32)",      &raw_32 },
    { "Multi-seg: MIX-RLE-MIX",         &multi_seg },
};

static void init_all_vectors(void)
{
    init_empty_set();
    init_single_42();
    init_sparse_3();
    init_dense_word();
    init_raw_32();
    init_multi_seg();
}

void run_codec_validation_tests(void)
{
    printf("\n========================================\n");
    printf("SSK Codec Validation Test Vectors\n");
    printf("========================================\n");
    
    init_all_vectors();
    
    size_t n = sizeof(vectors) / sizeof(vectors[0]);
    for (size_t i = 0; i < n; i++) {
        print_decoded(*vectors[i].decoded, vectors[i].name);
    }
    
    printf("\n========================================\n");
    printf("Vectors: %zu\n", n);
    printf("========================================\n");
}
