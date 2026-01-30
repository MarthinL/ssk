# Project Concerns

## Context Diagram

For 50 years, SQL transformed how the world stores data. Yet in the symbolic world of relational theory, sets are not persisted as data - they live fleeting lives, conjured through mathematical notation.

Mapping abstract theory to storage was profound. When it came to relationships between sets, both formally and informally, "Parent's Children" and "Children of Parent" are equivalent. SQL picked the latter as its storage paradigm.

SSK stems from exploring the former - the other side of that equivalence - as an equally valid alternative. Not a replacement, but a paradigm that could coexist with traditional approaches, even be internalised in SQL, retaining foreign key semantics while implementing storage as the inverse when beneficial.

That exploration yielded SubSet Key: a way to store subsets as scalar values with self-contained identity. The identity is bijection - a concept proven invaluable in its original setting  - and recognised for the opportunities it creates in databases.

This project is dedicated to realising those opportunities for everyone with a stake in data.

*Created by Marthin Laubscheron 2026-01-02 16:29:28, last modified 2026-01-02 16:35:55*

![A0: Subset Identity from Zero to Hero](./diagrams/dgm406.svg)

### Area A0: Subset Identity from Zero to Hero
|  | Role        | Name                                   | Details                               |
|--| :---------  | :------------------------------------- | ------------------------------------- |
|  | Impetus     | Limitations of Database Technology     |                                       |
|  | Impetus     | Structure of Data Imposed by Databases |                                       |
|  |             |                                        |                                       |
|  | Constraint  | Original M:N OLTP Requirement          |                                       |
|  |             |                                        |                                       |
|  | Outcome     | Functional SSK Bijection               |                                       |
|  | Outcome     | SQL Compatibility                      |                                       |
|  | Outcome     | Proven Correctness                     |                                       |
|  |             |                                        |                                       |
|  | Means       | Prior Art in Mathematics, Scients and Technology |                                       |
|  | Means       | Modern Computing and Database Platform |                                       |

---

## Separation of Concerns in Area A0: Subset Identity from Zero to Hero

![A0: Subset Identity from Zero to Hero](./diagrams/dgm455.svg)

### Interaction
| Role        | *#  | Name                                   | Details                               |
| :---------  |:---:| :------------------------------------- | ------------------------------------- |
| Impetus     | I1  | Limitations of Database Technology     | Impetus on A1: Formulation: The Bijection Affair |
| Impetus     | I2  | Structure of Data Imposed by Databases | Impetus on A1: Formulation: The Bijection Affair; Impetus on A4: Expansion: Preserving Semantics at BIGINT Scale |
|             |     |                                        |                                       |
<a id="back-1"></a>
| Constraint  | C1  | Original M:N OLTP Requirement          | Structure, see [1](#note-1)           |
|             |     |                                        |                                       |
<a id="back-2"></a>
| Outcome     | O1  | Functional SSK Bijection               | Structure, see [2](#note-2)           |
| Outcome     | O2  | SQL Compatibility                      | Outcome on A1: Formulation: The Bijection Affair |
| Outcome     | O3  | Proven Correctness                     | Outcome on A2: Exposition: Only Value Matters; Outcome on A3: Implementation: Rubber hits Road; Outcome on A4: Expansion: Preserving Semantics at BIGINT Scale |
|             |     |                                        |                                       |
<a id="back-3"></a>
| Means       | M1  | Prior Art in Mathematics, Scients and Technology | Structure, see [3](#note-3)           |
<a id="back-4"></a>
| Means       | M2  | Modern Computing and Database Platform | Structure, see [4](#note-4)           |


### Notes
<a id="note-1"></a>
**[1]** [C1: Original M:N OLTP Requirement](#back-1) has parts:
- **Scalar Requirement**
  - Constraint on A3: Implementation: Rubber hits Road
  - Constraint on A4: Expansion: Preserving Semantics at BIGINT Scale

- **Target Domain: BIGINT DBID**
  - Constraint on A1: Formulation: The Bijection Affair
  - Constraint on A4: Expansion: Preserving Semantics at BIGINT Scale

- **Example Problems**
  - Constraint on A2: Exposition: Only Value Matters

<a id="note-2"></a>
**[2]** [O1: Functional SSK Bijection](#back-2) has parts:
- **Formal SSK Type Definition**
  - Outcome on A1: Formulation: The Bijection Affair
  - Impetus on A2: Exposition: Only Value Matters
  - Constraint on A3: Implementation: Rubber hits Road
  - Constraint on A4: Expansion: Preserving Semantics at BIGINT Scale

- **User Guide**
  - Outcome on A2: Exposition: Only Value Matters

- **SSK for Trivial Domain**
  - Outcome on A3: Implementation: Rubber hits Road
  - Impetus on A4: Expansion: Preserving Semantics at BIGINT Scale

- **SSK for Target Domain**
  - Outcome on A4: Expansion: Preserving Semantics at BIGINT Scale

<a id="note-3"></a>
**[3]** [M1: Prior Art in Mathematics, Scients and Technology](#back-3) has parts:
- **Bourbaki: bijection**
  - Impetus on A1: Formulation: The Bijection Affair

- **McCaffrey: combinadics**
  - Impetus on A4: Expansion: Preserving Semantics at BIGINT Scale

- **Knuth: set + bit vector**
  - Impetus on A1: Formulation: The Bijection Affair

<a id="note-4"></a>
**[4]** [M2: Modern Computing and Database Platform](#back-4) has parts:
- **PostgreSQL Server**
  - Means on A2: Exposition: Only Value Matters
  - Means on A3: Implementation: Rubber hits Road
  - Means on A4: Expansion: Preserving Semantics at BIGINT Scale

- **PostgresSQL Extensibility**
  - Means on A1: Formulation: The Bijection Affair

- **64-bit Processor Architecture**
  - Means on A1: Formulation: The Bijection Affair

### Constituent Areas

#### A1: Formulation: The Bijection Affair
In The Bijection Affair, we recognise both SQL its profound influence in the database domain, and RDM for outlining a powerful theory despite getting hamstrung by the technology at the time it yielded SQL.

But the past is no place to get trapped. Prior art, and technological advances, helped us leverage new insights into:

- set and bit vectors representing sets, later updated as bijection, from Knuth,

- bijection, a refinement of one-to-one and onto, from Bourbaki,

- data generation language principles, from Kolmogorov,

- combinadics, a second bijection, from McCaffrey, and

- 64-bit processors as norm

to formulate SubSet Key (SSK) as a scalar value which bijects a subset of IDs of a table onto its own unique identifier.

Beyond the immediate value to its donor project, SSK paves the way to completing what RDM promised, without disrupting SQL compatibility.

*Created by Marthin Laubscheron 2026-01-02 16:29:28, last modified 2026-01-02 16:35:55*

|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    | I1 | Limitations of Database Technology     | From A0: Subset Identity from Zero to Hero |
|  |            |    |                                        |                                       |
|  | Impetus    |    | Changes Enabling Opportunities         | Outcome of A5: Vitalisation: Expanding Horizons |
|  |            |    |                                        |                                       |
|  | Impetus    |    | Definition Issues                      | Outcome of A2: Exposition: Only Value Matters |
|  |            |    |                                        |                                       |
|  | Impetus    |    | Knuth: set + bit vector                | Part of M1: Prior Art in Mathematics, Scients and Technology |
|  |            |    |                                        |                                       |
|  | Impetus    |    | Bourbaki: bijection                    | Part of M1: Prior Art in Mathematics, Scients and Technology |
|  |            |    |                                        |                                       |
|  | Impetus    | I2 | Structure of Data Imposed by Databases | From A0: Subset Identity from Zero to Hero |
|  |            |    |                                        |                                       |
|  |            |                                        |                                       |
|  | Constraint |    | Target Domain: BIGINT DBID             | Part of C1: Original M:N OLTP Requirement |
|  |            |    |                                        |                                       |
|  |            |                                        |                                       |
|  | Outcome    | O2 | SQL Compatibility                      | To A0: Subset Identity from Zero to Hero |
|  |            |    |                                        |                                       |
|  | Outcome    |    | Project Ethos                          | Constraint on A5: Vitalisation: Expanding Horizons |
|  |            |    |                                        |                                       |
|  | Outcome    |    | Opportunities                          | Outcome of A2: Exposition: Only Value Matters |
|  |            |    |                                        |                                       |
|  | Outcome    |    | Formal SSK Type Definition             | Part of O1: Functional SSK Bijection  |
|  |            |    |                                        |                                       |
|  |            |                                        |                                       |
|  | Means      |    | 64-bit Processor Architecture          | Part of M2: Modern Computing and Database Platform |
|  |            |    |                                        |                                       |
|  | Means      |    | PostgresSQL Extensibility              | Part of M2: Modern Computing and Database Platform |
|  |            |    |                                        |                                       |

#### A2: Exposition: Only Value Matters
Love doesn't keep score, but Value does.

SSK already provides value to its donor project, but only in a niche capacity - an injustice, given how much more it has to offer the entire database value chain and its stakeholders. None of whom would have encountered anything like it.

Giving a subset a bijective identity that fits in a scalar value is such an outlandish concept that the only way anyone can grasp it is to see it in action.

In the Nothing without Value concern, we capture selected rows into SSKs, store those values, combine them in different ways, and use the results to apply complex filters to tables without reprocessing the data. We treat subsets as scalar data: grouping them, aggregating them, converting to and from other types including SETOF BIGINT tables. We raise questions for the curious to play with.

All without caring about the underlying technology, because the semantics of SSK stay exactly the same whether the domain is 1..64 or 1..2^64. The domain is implementation detail crucial if value is to reach beyond the trivial but the semantics are fixed.

*Created by Marthin Laubscheron 2026-01-02 16:29:28, last modified 2026-01-26 19:45:49*

|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    |    | Formal SSK Type Definition             | Part of O1: Functional SSK Bijection  |
|  |            |    |                                        |                                       |
|  | Impetus    |    | Calculated Results                     | Outcome of A3: Implementation: Rubber hits Road |
|  |            |    |                                        |                                       |
|  |            |                                        |                                       |
|  | Constraint |    | Example Problems                       | Part of C1: Original M:N OLTP Requirement |
|  |            |    |                                        |                                       |
|  |            |                                        |                                       |
|  | Outcome    |    | User Guide                             | Part of O1: Functional SSK Bijection  |
|  |            |    |                                        |                                       |
|  | Outcome    | O3 | Proven Correctness                     | To A0: Subset Identity from Zero to Hero |
|  |            |    |                                        |                                       |
|  | Outcome    |    | Opportunities                          | Outcome of A1: Formulation: The Bijection Affair |
|  |            |    |                                        |                                       |
|  | Outcome    |    | Audited Expected Test Results          | Impetus of A3: Implementation: Rubber hits Road |
|  |            |    |                                        |                                       |
|  | Outcome    |    | Definition Issues                      | Impetus of A1: Formulation: The Bijection Affair |
|  |            |    |                                        |                                       |
|  |            |                                        |                                       |
|  | Means      |    | PostgreSQL Server                      | Part of M2: Modern Computing and Database Platform |
|  |            |    |                                        |                                       |

#### A3: Implementation: Rubber hits Road
Implementation is a technical problem, and any technical problem has a solution. What separates "can be solved" from "is solved" is doing the work. This is the concern where we do it.

For its initial and reference implementation, SSK lives in PostgreSQL user space as a C language extension: a User Defined Type SSK with functions, operators, and a User Defined Aggregate SSK_AGG. Not deeply integrated into the database core, but fully functional.

The trivial domain implementation is trivial, and that matters. Free of scale's distractions, it focuses entirely on SSK semantics - transparent and auditable. Every bit in the vector is visible. Every membership decision is clear. Every operation is verifiable with a programmer's calculator. No magic. No black boxes. No ambiguity.

When we extend to the inevitable non-trivial domain, the trivial implementation stands as a code-level reference. It defines what each function must achieve as it works with the vast abstract bit vector. The semantics remain fixed, solid, regardless of scale.

**On a technical level, this results in the first of two implementation, defined and compiled conditionally using the #ifdef TRIVIAL directive, replacing Format 0 with it's TRIVIAL counterpart Format 1023, which is minimal as well.**

*Created by Marthin Laubscheron 2026-01-02 16:29:28, last modified 2026-01-02 16:35:55*

|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    |    | Audited Expected Test Results          | Outcome of A2: Exposition: Only Value Matters |
|  |            |    |                                        |                                       |
|  |            |                                        |                                       |
|  | Constraint |    | Formal SSK Type Definition             | Part of O1: Functional SSK Bijection  |
|  |            |    |                                        |                                       |
|  | Constraint |    | Scalar Requirement                     | Part of C1: Original M:N OLTP Requirement |
|  |            |    |                                        |                                       |
|  |            |                                        |                                       |
|  | Outcome    | O3 | Proven Correctness                     | To A0: Subset Identity from Zero to Hero |
|  |            |    |                                        |                                       |
|  | Outcome    |    | Opportunities                          | Outcome of A1: Formulation: The Bijection Affair |
|  |            |    |                                        |                                       |
|  | Outcome    |    | Trivial Implementation                 | Constraint on A4: Expansion: Preserving Semantics at BIGINT Scale |
|  |            |    |                                        |                                       |
|  | Outcome    |    | SSK for Trivial Domain                 | Part of O1: Functional SSK Bijection  |
|  |            |    |                                        |                                       |
|  | Outcome    |    | Calculated Results                     | Impetus of A2: Exposition: Only Value Matters |
|  |            |    |                                        |                                       |
|  |            |                                        |                                       |
|  | Means      |    | PostgreSQL Server                      | Part of M2: Modern Computing and Database Platform |
|  |            |    |                                        |                                       |

#### A4: Expansion: Preserving Semantics at BIGINT Scale
The conceptual challenge was foresight: seeing that SSK could work at BIGINT scale, and inductive reasoning that it was possible. Everything else was mechanistic work - engineering for correctness, completeness, and bijection semantics.

Expect metrics to point here: lines of code, complexity, algorithmic density. This *is* the heavy lifting. But don't confuse effort with value. SSK is valuable because of bijection semantics. A4 unlocks that power at BIGINT scale.

We solve the engineering problems: eliminating the vast empty spaces that sparsity creates, finding structure within remaining data, and absorbing the chaotic bits that won't compress. We exploit a fundamental fact: members and slots are one-dimensional, internally ordered, and processable in predictable sequence. By skipping voids and working signal, we produce an external representation that preserves the exact bijective properties of the abstract bit vector it encodes.

The hard riddle solved: practical application of a principle that would otherwise remain theoretical.

**Uses the TRIVIAL implementation as reference point, and prroduces the second implementation, i.e. where TRIVIAL is no longer #defined and the format specification in effect is Format 0**

*Created by Marthin Laubscheron 2026-01-02 16:29:28, last modified 2026-01-02 16:35:55*

|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    |    | Audited Expected Test Results          | Outcome of A2: Exposition: Only Value Matters |
|  |            |    |                                        |                                       |
|  | Impetus    |    | SSK for Trivial Domain                 | Part of O1: Functional SSK Bijection  |
|  |            |    |                                        |                                       |
|  | Impetus    | I2 | Structure of Data Imposed by Databases | From A0: Subset Identity from Zero to Hero |
|  |            |    |                                        |                                       |
|  | Impetus    |    | McCaffrey: combinadics                 | Part of M1: Prior Art in Mathematics, Scients and Technology |
|  |            |    |                                        |                                       |
|  |            |                                        |                                       |
|  | Constraint |    | Trivial Implementation                 | Outcome of A3: Implementation: Rubber hits Road |
|  |            |    |                                        |                                       |
|  | Constraint |    | Formal SSK Type Definition             | Part of O1: Functional SSK Bijection  |
|  |            |    |                                        |                                       |
|  | Constraint |    | Target Domain: BIGINT DBID             | Part of C1: Original M:N OLTP Requirement |
|  |            |    |                                        |                                       |
|  | Constraint |    | Scalar Requirement                     | Part of C1: Original M:N OLTP Requirement |
|  |            |    |                                        |                                       |
|  |            |                                        |                                       |
|  | Outcome    |    | SSK for Target Domain                  | Part of O1: Functional SSK Bijection  |
|  |            |    |                                        |                                       |
|  | Outcome    | O3 | Proven Correctness                     | To A0: Subset Identity from Zero to Hero |
|  |            |    |                                        |                                       |
|  | Outcome    |    | Opportunities                          | Outcome of A1: Formulation: The Bijection Affair |
|  |            |    |                                        |                                       |
|  | Outcome    |    | Calculated Results                     | Impetus of A2: Exposition: Only Value Matters |
|  |            |    |                                        |                                       |
|  |            |                                        |                                       |
|  | Means      |    | PostgreSQL Server                      | Part of M2: Modern Computing and Database Platform |
|  |            |    |                                        |                                       |

#### A5: Vitalisation: Expanding Horizons
One powerful concept realised inspires countless more. This concern captures ideas that cannot command attention until foundations are solid: insights about extended scope, adjacent domains, derivative capabilities. We register them, attribute them, date them. They're safe. Work continues uninterrupted. The long-term viability of SSK as a self-sustaining initiative depends on it.

*Created by Marthin Laubscheron 2026-01-02 16:29:28, last modified 2026-01-02 16:35:55*

|  | Role       | *# | Name                                   | Details                               |
|--| :--------- |:--:| -------------------------------------- | ------------------------------------- |
|  | Impetus    |    | Opportunities                          | Outcome of A1: Formulation: The Bijection Affair |
|  |            |    |                                        |                                       |
|  |            |                                        |                                       |
|  | Constraint |    | Project Ethos                          | Outcome of A1: Formulation: The Bijection Affair |
|  |            |    |                                        |                                       |
|  |            |                                        |                                       |
|  | Outcome    |    | Changes Enabling Opportunities         | Impetus of A1: Formulation: The Bijection Affair |
|  |            |    |                                        |                                       |

---
