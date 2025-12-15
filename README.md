# SSK - SubSet Keys

SSK (SubSet Key) is a novel scalar type for PostgreSQL, designed to invert the cornerstone of the relational database as we know it - the foreign key reference - where children store the parent key as one of their fields. SSK values allow the parent to store a field that nominates its children, in a very special way mathematicians refer to as a bijection, so that each possible subset of children gets a unique identity which encodes the set of children directly in the SSKs value.

**Note:** PostgreSQL serves as the reference implementation for SSK, showcasing its universal applicability in a production database environment. While rooted in PostgreSQL, SSK's concepts extend to other relational databases.

To achieve this, SSK implements a canonical encoding scheme for sparse bit vectors representing sets, designed to efficiently handle subsets of large ID spaces in relational databases. By exploiting unique structural opportunities, SSK overcomes traditional limitations in encoding sparse sets, enabling direct indexing, comparison, and set operations on encoded values without decoding.

## The Problem and Opportunity

In Donald Knuth's seminal work on combinatorial algorithms, the encoding of sparse bit vectors for large sets is dismissed as impractical due to inefficiency. SSK challenges this by targeting a specific, high-value use case: representing subsets of database primary keys (IDs) as compact, canonical scalars. This allows:

- **Direct SQL Operations**: Use encoded SSKs in WHERE clauses, JOINs, and set operations without materializing the underlying bit vectors.
- **Canonical Encoding**: Identical sets always produce identical encodings, enabling equality comparisons and indexing.
- **Scalability**: Handles both tiny subsets (common case) and exceptionally large ones (rare but critical) with elegance.
- **Performance**: Minimizes encode-decode cycles, crucial for PostgreSQL aggregates, CTEs, and user-defined functions.

SSK is not a general-purpose compression algorithm but a specialized codec for set representation in databases, where the encoded form must be indexable and comparable.

## Theoretical Foundation

While SSK uses sparse bit vectors as its encoding mechanism, its purpose is fundamentally different: to provide a unique, canonical identity for any subset of a finite superset. This identity is not a hash (which may have collisions) but a bijection—a one-to-one correspondence—between subsets and scalar values.

Consider a finite superset $S$ with $n$ elements. The powerset $\mathcal{P}(S)$ contains all possible subsets of $S$, totaling $2^n$ subsets. SSK establishes a bijection $f: \mathcal{P}(S) \to \mathbb{N}$ (or a suitable scalar domain), where each subset $T \subseteq S$ maps to a unique SSK scalar that encodes exactly which elements of $S$ are in $T$.

This enables:
- **Exact Membership Encoding**: The SSK scalar contains precise information about subset membership without needing to store or query individual elements.
- **Canonical Representation**: Identical subsets produce identical SSKs, supporting direct equality and indexing.
- **Relational Alignment**: In database terms, for a table $A$ with primary keys $K_A = \{k(x) \mid x \in A\}$, SSK allows representing any subset of $K_A$ as a single scalar, bridging SQL's limitations in set-based operations.

SSK is tailored for database subsets (e.g., IDs in a table), not arbitrary mathematical sets.

### Practical Application in Databases

SSK is designed for tables with auto-generated `BIGINT` primary keys, though applicable to other key types. In SQL, selections define subsets of rows based on attributes via `SELECT` statements. These subsets are dynamic: rows are added, modified, or deleted, requiring re-evaluation of complex queries each time.

Traditionally, subsets lack stable identities:
- Views or materialized views can persist results but require DBA intervention for naming and updates.
- Runtime subset creation is possible but inefficient, with no native support for indexing or manipulating subsets as entities.

SSK transforms this by assigning each possible subset of a table's primary keys a unique, stable scalar identity. For a table with $n$ rows, the powerset includes $2^n$ subsets—from the empty set to the full table. SSK encodes each subset as a canonical scalar, enabling:

- **Stable Subset Storage**: Capture complex selections as indexable SSKs, avoiding repeated query execution.
- **Efficient Operations**: Perform set unions, intersections, and differences directly on SSKs; add/remove IDs without re-querying.
- **Optimized Retrieval**: Read table attributes via primary key lookups instead of re-running intricate `SELECT` statements.
- **Semantic Clarity**: SSKs represent predictable, updatable subsets for data that evolves predictably, complementing SQL's strength in handling unpredictable changes.

By embodying subsets as scalars, SSK empowers databases to treat selections as first-class, manipulable objects, reducing redundant computation and enhancing relational analytics.

## Encoding Scheme

An SSK encodes a sparse bit vector using a hybrid approach:

1. **Raw Bit Segments**: Direct encoding of contiguous bit runs where the pattern is chaotic or dense.
2. **Run-Length Encoded (RLE) Exceptions**: Efficient representation of long runs of zeros or ones relative to a variable base.
4. **Format Version**: Embedded version number ensures forward compatibility and prevents misinterpretation of encoded data.

The encoding is partitioned for 64-bit ID spaces, with segments ordered by partition ID. Empty partitions are omitted.

### Example Structure
- **Header**: Format version and partition metadata.
- **Segments**: Alternating raw bits and RLE blocks, each with CDU encoded parameters.
- **Canonical Property**: The encoding is deterministic, ensuring `encode(set A) == encode(set B)` iff `A == B`.

For detailed encoding specifications and format registry, see `ENCODING.md`.

## PostgreSQL Integration

SSK is designed as a PostgreSQL User-Defined Type (UDT), presenting externally as a variable-length binary string (VARBINARY or BYTEA-like scalar). This allows SSKs to be stored, indexed, and compared directly in SQL.

- **UDT Functions and Operators**: Full implementation of PostgreSQL UDT specifications, including input/output functions, comparison operators, and type-specific operations.
- **User-Defined Aggregates**: Support functions for aggregates where the result is an SSK UDT, enabling efficient construction from sets of IDs.
- **Outer Codec**: Handles serialization/deserialization of SSK binary strings to/from memory structures conducive to interpretation and manipulation. This outer layer decodes the opaque binary into internal representations for normalization and canonical re-encoding, while the inner CDU codec manages the compact encoding of lengths, offsets, and values.

## Building

### Unix-like Systems (Linux, macOS)

SSK uses PostgreSQL's PGXS build system:

```bash
make
sudo make install
```

### Windows

SSK was developed and tested on Unix-like systems where `make` is native and PGXS is fully supported. PostgreSQL extension development on Windows has unique challenges due to toolchain differences (MSVC vs. GCC, header compatibility, etc.).

If your daily work involves building PostgreSQL extensions on Windows, you likely have established toolchains and workflows. Adapt the build process to your environment as needed—whether using MinGW/MSYS2, Cygwin, or custom MSVC setups. The source code is portable C and should compile with appropriate PostgreSQL headers and libraries.

For reference, common approaches include:
- MinGW/MSYS2 for GCC-based builds compatible with PGXS.
- MSVC with custom project files for full Windows integration.

Contributions of Windows-specific build documentation or scripts are welcome!

## Project Goals

SSK is released into the public domain as a standalone PostgreSQL extension, with aspirations for broader adoption:

- **PostgreSQL Extension**: Implements SSK as a UDT with full operator and function support, plus User-Defined Aggregate support functions for aggregate construction.
- **Relational Data Model Alignment**: Bridge SQL's practical limitations with Ted Codd's original relational model, enabling true set-based operations.
- **Community Adoption**: Invite postgres-hackers and the broader database community to embrace SSK as a standard for set representation.

This project represents a critical component of a larger vision for advanced relational analytics, donated early to foster independent development and widespread benefit.

## Testing

SSK uses PostgreSQL's standard testing frameworks: **pg_regress** for SQL tests and **TAP** for complex scenarios.

**Quick start:**
```bash
make USE_PGXS=1
sudo make install USE_PGXS=1
cd test && make installcheck USE_PGXS=1
```

(Requires PostgreSQL superuser privileges to create test databases. See `TESTING.md` for alternatives.)

**For complete testing documentation**, see `TESTING.md` which covers:
- How PostgreSQL's testing frameworks work (pg_regress, TAP, make check vs installcheck)
- What tests exist and their purposes
- How to run specific tests
- How to add new tests
- Debugging test failures
- Requirements for each test type

## Testing

```bash
# Install extension to system PostgreSQL
sudo make install

# Run SQL regression tests
make installcheck

# Run TAP tests (Perl-based, creates temporary PostgreSQL instances)
make prove

# Run all tests
make check
```

See `TESTING.md` for comprehensive testing documentation.

## Contributing

Contributions are welcome! Please adhere to the project's style guide (`STYLE-GUIDE.md`). Technical decisions are documented in `DECISION_LOG.md`.

## License

This project is dedicated to the public domain. No rights reserved.
