/*
 * famtest.c - Exercise the new SSKDecoded hierarchy
 *
 * Test constructing a simple SSK in a stack buffer using the new
 * offset-based layout with separated metadata and blocks.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "ssk_decoded.h"

/*
 * We'll build a minimal SSK representing {42}:
 *   - 1 partition (partition 0, covering IDs 0 to 2^32-1)
 *   - 1 MIX segment
 *   - 1 chunk containing bit 42
 *
 * Memory layout in our buffer:
 *
 *   [SSKDecoded header]
 *   [partition_offs[0]]           <- offset to SSKPartition
 *   [SSKPartition header]
 *   [segment_offs[0]]             <- offset to SSKSegment
 *   [SSKSegment header]
 *   [data[0]: metadata word]      <- 1 chunk = 1 word (32 chunks fit in 64 bits)
 *   [blocks[0]: the bitmap]       <- at blocks_off from data[0]
 */

int main(void)
{
    /* Large enough buffer for our test */
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));

    printf("=== Structure sizes ===\n");
    printf("sizeof(SSKDecoded)   = %zu\n", sizeof(SSKDecoded));
    printf("sizeof(SSKPartition) = %zu\n", sizeof(SSKPartition));
    printf("sizeof(SSKSegment)   = %zu\n", sizeof(SSKSegment));
    printf("\n");

    /* 
     * Layout plan:
     *   Offset 0:    SSKDecoded header (40 bytes) + partition_offs[1] (4 bytes) = 44 bytes
     *   Offset 48:   SSKPartition header (32 bytes) + segment_offs[1] (4 bytes) = 36 bytes  
     *   Offset 88:   SSKSegment header (40 bytes)
     *   Offset 128:  data[0] = metadata (8 bytes)
     *   Offset 136:  blocks[0] = bitmap (8 bytes)
     *   Total: 144 bytes
     *
     * Align partition to 8-byte boundary for cleaner access.
     */

    /* --- SSKDecoded at offset 0 --- */
    SSKDecoded *dec = (SSKDecoded *)buffer;
    dec->format_version = 0;
    dec->n_partitions = 1;
    dec->var_data_off = sizeof(uint32_t);  /* After partition_offs[0] */
    dec->var_data_used = 0;  /* We'll compute later */
    dec->var_data_allocated = sizeof(buffer) - sizeof(SSKDecoded) - sizeof(uint32_t);
    dec->total_allocated = sizeof(buffer);
    dec->cardinality = 1;  /* One bit set: {42} */

    /* partition_offs[0] = offset from &partition_offs[0] to SSKPartition */
    size_t partition_start = 48;  /* Aligned offset from buffer start */
    size_t partition_offs_base = (size_t)dec->partition_offs - (size_t)buffer;
    dec->partition_offs[0] = partition_start - partition_offs_base;

    printf("=== SSKDecoded ===\n");
    printf("  partition_offs[0] = %u (partition at buffer offset %zu)\n",
           dec->partition_offs[0], partition_start);

    /* --- SSKPartition at offset 48 --- */
    SSKPartition *part = (SSKPartition *)(buffer + partition_start);
    part->partition_id = 0;
    part->n_segments = 1;
    part->var_data_off = sizeof(uint32_t);  /* After segment_offs[0] */
    part->var_data_used = 0;
    part->var_data_allocated = 256;  /* Arbitrary */
    part->cardinality = 1;

    /* segment_offs[0] = offset from &segment_offs[0] to SSKSegment */
    size_t segment_start = 88;  /* Aligned offset from buffer start */
    size_t segment_offs_base = (size_t)part->segment_offs - (size_t)buffer;
    part->segment_offs[0] = segment_start - segment_offs_base;

    printf("\n=== SSKPartition ===\n");
    printf("  partition_id = %u\n", part->partition_id);
    printf("  segment_offs[0] = %u (segment at buffer offset %zu)\n",
           part->segment_offs[0], segment_start);

    /* --- SSKSegment at offset 88 --- */
    SSKSegment *seg = (SSKSegment *)(buffer + segment_start);
    seg->segment_type = SEG_TYPE_MIX;
    seg->n_chunks = 1;
    seg->blocks_off = 8;       /* 1 metadata word (8 bytes), then blocks */
    seg->blocks_allocated = 1;
    seg->start_bit = 0;
    seg->n_bits = 64;          /* First chunk covers bits 0-63 */
    seg->last_chunk_nbits = 64;
    seg->rare_bit = 1;         /* 1 is the minority (only one set bit) */
    seg->cardinality = 1;

    printf("\n=== SSKSegment ===\n");
    printf("  segment_type = %s\n", seg->segment_type == SEG_TYPE_MIX ? "MIX" : "RLE");
    printf("  n_chunks = %u\n", seg->n_chunks);
    printf("  blocks_off = %u\n", seg->blocks_off);
    printf("  blocks_allocated = %u\n", seg->blocks_allocated);

    /* Set chunk 0 metadata: ENUM type (sparse), clean */
    uint8_t meta = chunk_meta_pack(CHUNK_TYPE_ENUM, CHUNK_FLAG_CLEAN);
    segment_chunk_meta_set(seg, 0, meta);

    /* Set chunk 0 block: bit 42 set */
    uint64_t bitmap = 1ULL << 42;
    segment_chunk_block_set(seg, 0, bitmap);

    printf("\n=== Chunk 0 ===\n");
    printf("  meta = 0x%02x (type=%s, dirty=%s)\n",
           segment_chunk_meta_get(seg, 0),
           segment_chunk_is_raw(seg, 0) ? "RAW" : "ENUM",
           segment_chunk_is_dirty(seg, 0) ? "yes" : "no");
    printf("  block = 0x%016lx\n", segment_chunk_block_get(seg, 0));

    /* Verify by extracting set bits */
    printf("\n=== Verify set bits ===\n");
    uint64_t blk = segment_chunk_block_get(seg, 0);
    printf("  Set bits: {");
    int first = 1;
    for (int bit = 0; bit < 64; bit++) {
        if (blk & (1ULL << bit)) {
            printf("%s%d", first ? "" : ", ", bit);
            first = 0;
        }
    }
    printf("}\n");

    /* Now verify traversal using the accessor functions */
    printf("\n=== Traversal test ===\n");
    SSKPartition *p = decoded_partition(dec, 0);
    printf("  decoded_partition(dec, 0) -> partition_id = %u\n", p->partition_id);
    
    SSKSegment *s = partition_segment(p, 0);
    printf("  partition_segment(part, 0) -> segment_type = %s, n_chunks = %u\n",
           s->segment_type == SEG_TYPE_MIX ? "MIX" : "RLE", s->n_chunks);
    
    uint64_t b = segment_chunk_block_get(s, 0);
    printf("  segment_chunk_block_get(seg, 0) = 0x%016lx\n", b);

    /* Raw memory dump for debugging */
    printf("\n=== Raw memory (first 160 bytes) ===\n");
    for (int i = 0; i < 160; i += 8) {
        printf("  [%3d]: ", i);
        for (int j = 0; j < 8; j++) {
            printf("%02x ", buffer[i + j]);
        }
        printf(" |");
        uint64_t *qword = (uint64_t *)(buffer + i);
        printf(" 0x%016lx", *qword);
        printf("\n");
    }

    printf("\n=== Test PASSED ===\n");
    return 0;
}