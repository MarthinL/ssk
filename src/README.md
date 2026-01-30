# SSK Source Code Organization

This directory contains the SSK implementation, organized by concern.

## Directory Structure

| Directory | Concern | IDEF0 Reference | Purpose |
|-----------|---------|-----------------|---------|
| pgext/ | Integration | IMP/A0:The SSK Extension (PostgreSQL UDT/AGG interface) | PostgreSQL type registration, I/O functions, operators, aggregates |
| codec/ | Persistence | IMP/A1:Value Decoder, IMP/A3:Value Encoder | Encode/decode between bytes and abstract bit vector |
| keystore/ | Caching | Support for AbV caching | Transaction-scoped abstract bit vector cache |

## Concern Mapping

### Integration Layer (IMP/A0)

**Purpose**: Adapt SSK functionality to PostgreSQL's type system and aggregate framework.

**Responsibilities**:
- Type registration (ssk_udt.c)
- Input/output functions (ssk_in, ssk_out)
- Comparison operators (ssk_eq, ssk_lt, etc.)
- Set operations SQL interface (ssk_union, ssk_intersect, etc.)
- Aggregate functions (ssk_sfunc, ssk_finalfunc)
- PostgreSQL ↔ SSK codec wrappers (ssk_abv.c)

**Files**: pgext/ssk_udt.c, pgext/ssk_agg.c, pgext/ssk_abv.c

### Persistence Layer (IMP/A1, IMP/A3)

**Purpose**: Canonical encoding/decoding preserving bijection.

**Responsibilities**:
- IMP/A1: Decode SSK bytes → Abstract bit Vector (AbV)
- IMP/A3: Encode AbV → SSK bytes
- Format 0 hierarchical structure (Partitions → Segments → Chunks → Tokens)
- CDU codec for data units
- Maintain canonicity rules

**Files**: codec/ssk_codec.c, codec/ssk_encode.c, codec/ssk_decode.c

### Function Processor (IMP/A2)

**Status**: Primarily in pgext/ (UDT/aggregate dispatch), with generic operations in codec/.

**Purpose**: Set operations on in-memory AbV representation.

**Responsibilities**:
- Union, intersection, complement, except
- Membership testing, cardinality
- Fragmentation for efficient processing
- Normalization to canonical form

**Future**: May be extracted to dedicated operations/ directory.

## Understanding Source Organization

SSK separates concerns to enable:
1. **Independent evolution**: Change encoding format without touching PostgreSQL integration
2. **Clear responsibility**: Each file knows its concern boundaries
3. **Testing isolation**: Test persistence without PostgreSQL, test operations without encoding details

When working on SSK:
- **Know your concern**: Identify which concern you're modifying (IMP/A0, IMP/A1, IMP/A2, IMP/A3)
- **Respect boundaries**: Don't mix persistence logic with PostgreSQL integration
- **Consult the model**: See ARCHITECTURE.md for formal concern relationships

## Build Process

SSK uses PostgreSQL's PGXS build system:

```
make USE_PGXS=1
sudo make install USE_PGXS=1
```

See root Makefile for compilation units and dependencies.
