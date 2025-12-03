#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "ssk_decoded.h"

int main(void) {
  uint8_t sskbuf[512]       = {0};
  SSKDecoded    *ssk        = (SSKDecoded *)sskbuf;
  SSKPartition  *partition  = decoded_partition(ssk, 0);
  SSKSegment    *segment    = partition_segment(partition, 0);

  *ssk        = (SSKDecoded){.format_version        = 1,             // ???
                             .rare_bit              = 1,             // ???
                             .var_data_allocated    = 512,           // ---
                             .n_partitions          = 1,             // ???
                             .var_data_off          = 6,             // ---
                             .var_data_used         = 20,            // ---
                             .cardinality           = 12             // ???
                            };                                       // ???
  ssk->partition_offs[0]                            = 0;             // ---
  *partition  = (SSKPartition){.partition_id        = 24,            // ???
                               .rare_bit            = 1,             // ???
                               .n_segments          = 1,             // ???
                               .var_data_off        = 3,             // ---
                               .var_data_used       = 6,             // ---
                               .var_data_allocated  = 4,             // ---
                               .cardinality         = 12};           // ???
  partition->segment_offs[0]                        = 0;             // ---
  *segment    = (SSKSegment){.segment_type          = SEG_TYPE_RLE,  // ???
                             .n_bits                = 12,            // ???
                             .start_bit             = 1234,          // ???
                             .blocks_off            = 0,             // ---
                             .blocks_allocated      = 0,             // ---
                             .cardinality           = 12,            // ???
                             .rare_bit              = 1};            // ???

  for (int i = 0; i < 20; i++) {
    printf("[%2d] = 0x%016llx\n", i, (unsigned long long)((unsigned long long *)ssk)[i]);
  }

  return 0;
}