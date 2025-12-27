# Chunk Encoding

MIX segment tokenization and chunk-level encoding.

## Responsibilities

- Chunk MIX segment interior into fixed-size windows (default 64 bits)
- Determine ENUM vs RAW encoding per chunk based on popcount threshold (k ≤ 18)
- RAW coalescing: group consecutive RAW chunks into RAW_RUN tokens
- Token stream generation

## Files

- chunk_token.c: Token type determination and stream generation
- chunk_raw.c: RAW and RAW_RUN token encoding/decoding
- chunk_enum.c: ENUM token encoding/decoding (uses combinadic)

## Related Concerns

- ../combinadic/: Rank/unrank operations for ENUM chunks
- ../cdu/: CDU encoding for token fields (run_len, combined)
