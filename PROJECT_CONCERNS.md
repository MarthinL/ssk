# Project Concerns Model (PRJ)

**IDEF0 Model Report**  
**Date**: December 24, 2025

---

**Repository:** Open Source SSK;  Date:  12/25/2025


# 


# Repository:  Open Source SSK


# Repository Summary

**Used At:** LobeShare
**Creator:** Marthin Laubscher

## Description

This repository deals with the extraction of SSK as defined and used by Synoida into a separate public GitHub Repository aimed at allowing the enormous potential the SubSet Key bijection has in the wider database industry and its countless customers, end-users, their customers, and the plethora of ventures that form and service the entire value chain.

# Model:  Project Concerns


# Model Summary

**Name:** Project Concerns
**Creator:** Marthin Laubscher

## Purpose

It is (hard, and) important to keep in mind that the purpose of this model is to separate concerns, not to design and structure the solution itself.
The intention being to replace Activity names with "OneWord"  or "Multiple Word Phraces (MWP)" to name concerns, broken down to at most one API function per "Activity/Concern" and nothing about the detail below that.
For the Concept names, we stick with Object names from the domain, with state/purpose tagging as needed to keep them unique in as to not breqk the Activity Modelling rules.

# Title:Project Concerns


![Project Concerns](diagrams/dgm24.svg)


## Activities in Diagram"Project Concerns"


| Activity | Creator | Description |
| --- | --- | --- |
| [A0: Subset Identity from Zero to Hero](#titledecomposition-of-ssk-concerns) | Marthin Laubscher | For 50 years, SQL transformed how the world stores data. Yet in the symbolic world of relational theory, sets are not persisted as datathey live fleeting lives, conjured through mathematical notation.<br><br>Mapping abstract theory to storage was profound. When it came to relationships between sets, both formally and informally, "Parent's Children" and "Children of Parent" are equivalent. SQL picked the latter as its storage paradigm.<br><br>SSK stems from exploring the formerthe other side of that equivalenceas an equally valid alternative. Not a replacement, but a paradigm that could coexist with traditional approaches, even be internalised in SQL, retaining foreign key semantics while implementing storage as the inverse when beneficial.<br><br>That exploration yielded SubSet Key: a way to store subsets as scalar values with self-contained identity. The identity is bijectiona concept proven invaluable in its original setting, and recognised for the opportunities it creates in databases.<br><br>This project is dedicated to realising those opportunities for everyone with a stake in data. |


## Concepts in Diagram"Project Concerns"


| Concept | Creator |
| --- | --- |
| Functional SSK Bijection | Marthin Laubscher |
| SQL Compatibility | Marthin Laubscher |
| Original M:N OLTP Requirement | Marthin Laubscher |
| Prior Art in Mathematics, Scients and Technology | Marthin Laubscher |
| Modern Computing and Database Platform | Marthin Laubscher |
| Limitations of Database Technology | Marthin Laubscher |
| Structure of Data Imposed by Databases | Marthin Laubscher |
| Proven Correctness | Marthin Laubscher |


# Title:Decomposition of SSK Concerns


![Decomposition of SSK Concerns](diagrams/dgm333.svg)


# Diagram Summary

**Name:** Decomposition of SSK Concerns
**Creator:** Marthin Laubscher

## Activities in Diagram"[Decomposition of SSK Concerns](#titleproject-concerns)"


| Activity | Creator | Description |
| --- | --- | --- |
| [A1: Formulation: The Bijection Affair](IMPLEMENTATION_CONCERNS.md#titlea0-the-ssk-extension) | Marthin Laubscher | In The Bijection Affair, we recognise both SQL its profound influence in the database domain, and RDM for outlining a powerful theory despite getting hamstrung by the technology at the time it yielded SQL.<br><br>But the past is no place to get trapped. Prior art, and technological advances, helped us leverage new insights into:<br><br>- set and bit vectors representing sets, later updated as bijection, from Knuth,<br><br>- bijection, a refinement of one-to-one and onto, from Bourbaki,<br><br>- data generation language principles, from Kolmogorov,<br><br>- combinadics, a second bijection, from McCaffrey, and<br><br>- 64-bit processors as norm<br><br>to formulate SubSet Key (SSK) as a scalar value which bijects a subset of IDs of a table onto its own unique identifier.<br><br>Beyond the immediate value to its donor project, SSK paves the way to completing what RDM promised, without disrupting SQL compatibility. |
| A2: Exposition: Nothing without Value | Marthin Laubscher | Love doesn't keep score, but Value does.<br><br>SSK already provides value to its donor project, but only in a niche capacityan injustice, given how much more it has to offer the entire database value chain and its stakeholders. None of whom would have encountered anything like it.<br><br>Giving a subset a bijective identity that fits in a scalar value is such an outlandish concept that the only way anyone can grasp it is to see it in action.<br><br>In the Nothing without Value concern, we capture selected rows into SSKs, store those values, combine them in different ways, and use the results to apply complex filters to tables without reprocessing the data. We treat subsets as scalar data: grouping them, aggregating them, converting to and from other types including SETOF BIGINT tables. We raise questions for the curious to play with.<br><br>All without caring about the underlying technology, because the semantics of SSK stay exactly the same whether the domain is 1..64 or 1..2^64. The domain is implementation detailcrucial if value is to reach beyond the trivialbut the semantics are fixed. |
| A3: Implementation: Rubber hits Road | Marthin Laubscher | Implementation is a technical problem, and any technical problem has a solution. What separates "can be solved" from "is solved" is doing the work. This is the concern where we do it.<br><br>For its initial and reference implementation, SSK lives in PostgreSQL user space as a C language extension: a User Defined Type SSK with functions, operators, and a User Defined Aggregate SSK_AGG. Not deeply integrated into the database core, but fully functional.<br><br>The trivial domain implementation is trivial, and that matters. Free of scale's distractions, it focuses entirely on SSK semanticstransparent and auditable. Every bit in the vector is visible. Every membership decision is clear. Every operation is verifiable with a programmer's calculator. No magic. No black boxes. No ambiguity.<br><br>When we extend to the inevitable non-trivial domain, the trivial implementation stands as a code-level reference. It defines what each function must achieve as it works with the vast abstract bit vector. The semantics remain fixed, solid, regardless of scale. |
| A4: Expansion: Preserving Semantics at BIGINT Scale | Marthin Laubscher | The conceptual challenge was foresight: seeing that SSK could work at BIGINT scale, and inductive reasoning that it was possible. Everything else was mechanistic workengineering for correctness, completeness, and bijection semantics<br><br>Expect metrics to point here: lines of code, complexity, algorithmic density. Thisisthe heavy lifting. But don't confuse effort with value. SSK is valuable because of bijection semantics. A4 unlocks that power at BIGINT scale.<br><br>We solve the engineering problems: eliminating the vast empty spaces that sparsity creates, finding structure within remaining data, and absorbing the chaotic bits that won't compress. We exploit a fundamental fact: members and slots are one-dimensional, internally ordered, and processable in predictable sequence. By skipping voids and working signal, we produce an external representation that preserves the exact bijective properties of the abstract bit vector it encodes.<br><br>The hard riddle solved: practical application of a principle that would otherwise remain theoretical. |
| A5: Vitalisation: Expanding Horizons | Marthin Laubscher | One powerful concept realised inspires countless more. This concern captures ideas that cannot command attention until foundations are solid: insights about extended scope, adjacent domains, derivative capabilities. We register them, attribute them, date them. They're safe. Work continues uninterrupted. The long-term viability of SSK as a self-sustaining initiative depends on it. |


## Concepts in Diagram"[Decomposition of SSK Concerns](#titleproject-concerns)"


| Concept | Creator | Description |
| --- | --- | --- |
| Functional SSK Bijection | Marthin Laubscher |  |
| SQL Compatibility | Marthin Laubscher |  |
| Proven Correctness | Marthin Laubscher |  |
| Original M:N OLTP Requirement | Marthin Laubscher |  |
| Prior Art in Mathematics, Scients and Technology | Marthin Laubscher |  |
| Modern Computing and Database Platform | Marthin Laubscher |  |
| Limitations of Database Technology | Marthin Laubscher |  |
| Structure of Data Imposed by Databases | Marthin Laubscher |  |
| Target Domain: BIGINT DBID | Marthin Laubscher | In the original requirement the IDs are all BIGINT values, which would require a bijunction capable of storing 2^64 membership decisions would theoretically be required as the superset is all 2^64 BIGINT values.<br><br>However, database tables have reasonable limits restricting the actual number of rows and therefore primary keys that can appear in a table at any given point. Limiting the effective superset for which we need to indentify subsets to those, makes a big difference to the viability of SSK. We'd still need the full range of identifiers, but the natural limitation on cardinality of the superset caps the cardinality of the subset as well, which guarantees a level of sparsity we can depend on.<br><br>The SSK concept deos not do unbounded compression or other magic, it makes optimal use of implied constraints and the opportunities those create. |
| Scalar Requirement | Marthin Laubscher | Beyond affording each subset an identity, the the original requirement is also to represent that identity at database (field, variable) level as a regular scalar value, i.e. not a set, array or other composite type, at least externally. Internally it can be anything, but it should not require being internalised in order to be compared to other values for ordering or equality. |
| Formal SSK Type Definition | Marthin Laubscher |  |
| Definition Issues | Marthin Laubscher |  |
| Example Problems | Marthin Laubscher |  |
| User Guide | Marthin Laubscher |  |
| Opportunities | Marthin Laubscher |  |
| Changes Enabling Opportunities | Marthin Laubscher |  |
| Project Ethos | Marthin Laubscher |  |
| SSK for Trivial Domain | Marthin Laubscher |  |
| SSK for Target Domain | Marthin Laubscher |  |
| Audited Expected Test Results | Marthin Laubscher |  |
| Calculated Results | Marthin Laubscher |  |
| d64-bit Processor Architecture | Marthin Laubscher |  |
| PostgresSQL Extensibility | Marthin Laubscher |  |
| PostgreSQL Server | Marthin Laubscher |  |
| Knuth: set + bit vector | Marthin Laubscher |  |
| McCaffrey: combinadics | Marthin Laubscher |  |
| Bourbaki: bijection | Marthin Laubscher |  |

Creator:	Marthin Laubscher
Date Created:	12/24/2025  02:25.27
Last Modified:	12/24/2025  02:25.27

## SSK for Target Domain

Creator:	Marthin Laubscher
Date Created:	12/24/2025  02:25.27
Last Modified:	12/24/2025  02:25.27

## Audited Expected Test Results

Creator:	Marthin Laubscher
Date Created:	12/24/2025  02:25.27
Last Modified:	12/24/2025  02:25.27

## Calculated Results

Creator:	Marthin Laubscher
Date Created:	12/24/2025  02:25.27
Last Modified:	12/24/2025  02:25.27

## d64-bit Processor Architecture

Creator:	Marthin Laubscher
Date Created:	12/25/2025  01:28.06
Last Modified:	12/25/2025  01:28.06

## PostgresSQL Extensibility

Creator:	Marthin Laubscher
Date Created:	12/24/2025  02:25.27
Last Modified:	12/24/2025  02:25.27

## PostgreSQL Server

Creator:	Marthin Laubscher
Date Created:	12/24/2025  02:25.27
Last Modified:	12/24/2025  02:25.27

## Knuth: set + bit vector

Creator:	Marthin Laubscher
Date Created:	12/25/2025  01:28.06
Last Modified:	12/25/2025  01:28.06

## McCaffrey: combinadics

Creator:	Marthin Laubscher
Date Created:	12/24/2025  02:25.27
Last Modified:	12/24/2025  02:25.27

## Bourbaki: bijection

Creator:	Marthin Laubscher
Date Created:	12/24/2025  02:25.27
Last Modified:	12/24/2025  02:25.27