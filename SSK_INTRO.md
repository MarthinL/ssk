<!-- Copyright (c) 2020-2025 Marthin Laubscher. All rights reserved. See LICENSE for details. -->

# An introduction to SSK values  

The concept of SSKs, SubSet Keys, was born in anguish over the slippery nature of M:N in SQL, lacking a stable
reference point for analysis.

With vast pools of data, I needed a way to:
- know if there are potentially meaningful patterns,
- extract meaning from detected patterns, and
- present those to users in summarised form
- all without the luxury of additional domain knowledge to remap the M:N data onto "real" entities.

Eventually, the frustration crystallised into the quest to give subsets an identity of their own, so I can test
which commonly occurring subsets were present, and what is common among those who share them.

## What’s it really about?  

Suppose you had a set of numbers, such as the primary keys of a database table, where the domain or range of
the keys are limited to 1, 2, or 3 (assuming 0 and null mean the same here). 

```
┌────┬──────────┬──────────────────────┬─────┐
│ ID │ name     │ email                │ ... │
├────┼──────────┼──────────────────────┼─────┤
│ 1  │ Alice    │ alice@example.com    │ ... │
│ 2  │ Bob      │ bob@example.com      │ ... │
│ 3  │ Charlie  │ charlie@example.com  │ ... │
└────┴──────────┴──────────────────────┴─────┘
```
**Diagram 1:** A trivial PERSON table. Primary keys 1, 2 and 3.

If we now selected from that table, the result set would invariably (ignoring order) one of 8 (2^3)
possibilities {}, {1}, {2}, {3}, {1, 2}, {1, 3}, {2, 3}, or {1, 2, 3}.

```
┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐
│ ID │...│ │ ID │...│ │ ID │...│ │ ID │...│ │ ID │...│ │ ID │...│ │ ID │...│ │ ID │...│
├────┼───┤ ├────┼───┤ ├────┼───┤ ├────┼───┤ ├────┼───┤ ├────┼───┤ ├────┼───┤ ├────┼───┤
└────┴───┘ │ 1  │...│ │ 2  │...│ │ 3  │...│ │ 1  │...│ │ 1  │...│ │ 2  │...│ │ 1  │...│
           └────┴───┘ └────┴───┘ └────┴───┘ │ 2  │...│ │ 3  │...│ │ 3  │...│ │ 2  │...│
                                            └────┴───┘ └────┴───┘ └────┴───┘ │ 3  │...│
                                                                             └────┴───┘
```
**Diagram 2:** SELECT id, ... FROM PERSON WHERE ...; yields one of 8 possible results.

If we assign each possible ID that may be present in the result its own bit in a bit string, we can encode or
remember exactly what combination of rows a select statement returned, or should return, in a 3-bit value.  

```
┌────────────────────────────────────┐
│                            ID      │
│                     ┌────>  1      │
│                     │┌───>  2      │
│                     ││┌──>  3      │
│                     │││            │
│  Subset       Bit-->210   Value    │
│  {}        <--->  0b000  =   0     │
│  {1}       <--->  0b100  =   4     │
│  {2}       <--->  0b010  =   2     │
│  {3}       <--->  0b001  =   1     │
│  {1, 3}    <--->  0b110  =   6     │
│  {2, 3}    <--->  0b011  =   3     │
│  {1, 3}    <--->  0b101  =   5     │
│  {1, 2, 3} <--->  0b111  =   7     │
└────────────────────────────────────┘
```
**Diagram 3:** Mapping each subset to unique scalar value - perfect bijection.

That 3-bit value would become the identity of that specific subset, a scalar value fully representing a subset.

### The Core Concept

### **That's the entire SubSet Key concept - a value capturing subset membership so predictably it qualifies as unique ID for each possible subset.**

With the "what and why" so clear, attention shifts to how, which of course presents the minor complication that we
to scale up from the above trivial example, to the actual bigint we're targeting,  

```
                         trivial example ╶──────>            bigint target domain
                        ┌───────────────┐        ┌─────────────────────────────────────────┐
                    ID: │  2-bit        │╶──────>│  64-bit                                 │
   Abstract bit vector: │ 2²-1 = 3 bits │╶──────>│ 2⁶⁴-1 = 18,446,744,073,709,551,615 bits │
   Distinct SSK values: │       2³ = 8  │╶──────>│        2¹⁸ ⁴⁴⁶ ⁷⁴⁴ ⁰⁷³ ⁷⁰⁹ ⁵⁵¹ ⁶¹⁵ = ?  │
    Order of magnitude: │            1  │╶──────>│                        5.5 quintillion  │
                        └───────────────┘        └─────────────────────────────────────────┘
```

$2^{2^{64}}$ (5.5 quintillion digits) possible subsets?! 

The complication isn't minor at all, it's likely to be impossible.

> Impossibility Theory: Often times, what is broadly accepted as impossible, has a solution, which will surface if 
you get your team (or yourself) vigorously defending why it's impossible, until someone says "unless...". In that 
moment, know you found an opening, often to another seemingly impossible problem, but a different one. Wash, rinse, 
and repeat, until the next unless isn't impossible any more, then work back up the chain and get it done.

Donald Knuth had already documented and proven the bijection (not in the same terms though, he defined sets, bit
vectors, and said they can represent each other) but came to a similar conclusion that while it works for trivial
sets, it's impractical for large and sparse sets. But Knuth himself gives us such an "unless..." moment - not
viable, unless they are compressed. Compression becomes the new challenge - still imposing, perhaps impossible, 
but a different challenge to tackle nevertheless.

And so the process starts. I wish it was a true account (being true would have simplified my life, a lot), but the
following is grossly inaccurate compared to the multitude of false-starts, dead-ends, and should-have-knowns I
faced along the way. The tale as I tell it here, is the sanitised version enabled by hindsight, while in reality
almost all the theory and prior art involved was identified a long time after I had already done a proof of concept
and developed a way to overcome the overwhelming size issue, using the same style reasoning, that's authentic, just
with a very different, larger and more convoluted set of "unless..." insights.

SSK's objective and concept were and remain stable - a value capturing subset membership so predictably that it
 qualifies as unique ID for each possible subset. Though even such succinct definition stems from hindsight.

Functionally, Knuth's "unless they are compressed" broke down into two separate but related attributes of the
 required 'compression'. I needed performant:

- virtually unbounded compression - $2^{64}$ --> $min(|{subset}|, |\complement {subset}|)$,
- without losing the bijective properties of the bit vector <--> set representation.

Above, I use the term compressed, because Knuth used it, but since Knuth wrote, 'compression' itself has
become contentious territory. With the parallel need to preserve bijection, SSK neither uses nor claims
unbounded compression.

SSK uses domain-specific compaction, exploiting known structure, raw binary for patches of localised randomness,
and combinadics for the semi-dense areas that's neither empty nor chaotic.  

Well after I had taken to doing SSK as I figured it could work, when I took to documenting it as a project on its
own, I found a string of prior art and academic references supporting and validating what I've done and how. 
At the end of that chain, I discovered a summary of the work of Andrey Kolmogorov circa '68. 
Admittedly, even if you shoved it right in my face, shouting "Here, this is the solution", I would not have seen any 
connection between what I was trying to do and what Kolmogorov wrote. Only in hindsight, once it was all figured
out, was I in a position to recognise that it actually confirms and validates exactly the solution I found. 

A (very) loosely paraphrased summary of what Kolmogorov had to say about compression was, interpreted by many
others along the way, was this:
- all compression is about understanding the structure of the data (detected or induced), and storing the structure instead of the data, 
- unbounded compression is enabled by either sparsity or known structure, having both is miraculous, and
- dense, chaotic data defeats any compression, but where the environment (like the database) induces structure, domain-wide chaos in unnatural and attempts to force it will in itself induce structure.

The original Kolmogorov papers were about using "programs in some language" that generates the data, which is
exactly what's happening in SSK, if you map it as such. While not expressed in any general purpose language as he
suggested, SSK uses an extremely tight domain specific language, yet every SSK in its encoded form only
occasionally embeds raw data. Only when the stretch of data is too small to make a fuss about or too random for the
cost to understand its structure. The rest, every single bit of it, is actually a program that generates the data,
the decoder is a compiler that translates the program to executable form in memory, the functions are plug-in
modules for when the program runs, and the encoder is a code generator taking an empty or modified program and
compacts it into code again. The format description provides the environment variables and compiler directives used
by the compiler, code generator and the run-time of the program in order to achieve the second objective - ensuring
that every virtual dataset has one and only one possible programmatic representation. 

It will take some unpacking to apply each of those abstracted concepts in algorithmic code, but that's SSK in a
nutshell, validated by long established principles in mathematics and computer science, in aid of finally getting
SQL to deliver on the Relational Data Model child of both disciplines. 


  
## Putting it to work  
  
> Note: The procedural components mentioned in this description are not specific to the encoding or decoding process, but to how the abstract bit vector is broken down into smaller, well defined parts. It’s described as if operating on a materialised bit string, which is not an option.  How that maps into the in-memory decoded representation of the abstract bit vector and the encoded for storage version of the same thing, is treated as a separate concern.  
  
As explained above SSK is trivially simple as an abstract concept, with all of its complications arising from the equally simple fact that in no known (computational) universe can we even contemplate loading the abstract bit vector into memory in its expanded form to work on it. We’re faced with two basic sources of complexity - canonical representation as a minimal scalar value and the algorithms to implement operations on the SSK in its in-memory format which has to exploit the sparse nature of the actual values as well in order to even fit into memory.  With some commonalities between them we’ll cover first, we can unpack the essence of each type of complication and how SSK tackles them.  
  
### Overall Approach: Divide and Ignore  
  
The most striking feature of the actual values we seek to represent also provides the most significant opportunity to do so efficiently. However you arrange it, a few thousand to even a few hundred million bits being on out of 2^2^64 bit positions unavoidably leaves a massive amount of dominant bits. Thus:  
  
> **Rare and Dominant Bits** in the SSK context are symbolic names for what is usually 1 and 0 respectively, so that a single bit can define which is which and we can specify and implement the rules and code in terms of their symbolic names instead of using 0 and 1 with conditional code and verbiage obfuscating simple principles.    

**The first rule of dominants is that we don't talk about dominants.** Those vast expanses of dominants in the abstract bit vector are accounted for but never directly addressed. We accurately define where the rare bits are positioned in the abstract bit vector, in such way that we can guarantee the same encoding for the same bits being set, but dominants are inferred, not captured in the data.  
  
To make silently omitting references to all-dominant stretches meaningful and predictable enough to ensure efficiency and canonicity, requires rigorously defined and applied rules, which SSK handles in two stages.    
  
#### **Stage 1: Partitioning.**  
  
Partitioning breaks up the gigantic abstract bit vector into blocks with fixed, absolute boundaries, with negligible computational impact at the cost of possible inefficient encoding.  
  
Simple and effective (not to mention aligned with how SSK’s parent project partitions its data) every bigint considered by SSK is first of all split on the 32-bit boundary into a partition number (most significant 32 bits), and an offset relative to that (least significant 32 bits). Nothing is emitted about empty partitions, and each non-empty partition is encoded independently. The empty subset, is nothing more than a format number, 1 bit of global dominant bit value, and a CDU (Canonical Data Unit) encoded partition count of 0, meaning no non-empty partitions.  If n_partitions > 0, it is followed by that many partitions, each starting with a partition number encoded as an offset from the previous where the first partition present is assumed to have a previous of 0 so the offset = the smallest non-empty partition number, a single bit stating the dominant bit in the partition, followed by the next level of encoding, i.e. segments.    
  
#### **Stage 2: Segmentation**  
  
Segmentation uses unambiguous rules to break up partitions into smaller, variable sized parts, to gain encoding efficiency at the cost of some computational complexity.  
  
In principle segmentation is also quite simple. Long runs of dominant bits separate areas in the abstract bit vector that contains rare bits, and since we don't talk about zeros, segmentation ends being just identifying the portions of the partitions's abstract bit vector where there are rare bits to talk about.  

```
┌────────────────────────────────────────────────────────────────────────┐
│ Illustration 4: Segmentation - Islands of Rare Bits                    │ 
├────────────────────────────────────────────────────────────────────────┤
│                                                                        │
│  Partition's abstract bit vector (0 = dominant, 1 = rare):             │
│                                                                        │
│  [000000001110010000000000001100111000000000010000000...]              │
│         └─Seg 1──┘           └─Seg 2──┘        └S3┘                    │
│                                                                        │
│  Encoded output contains only the segments (shaded islands):           │
│                                                                        │
│   ████████                    ████████           ███                   │
│   Segment 1    (gap: omitted) Segment 2  (gap)  Seg 3                  │
│   offset=7     not encoded    offset=25  omit   offset=39              │
│   len=8                       len=8             len=2                  │
│                                                                        │
│  Vast stretches of dominant bits are never stored—only their           │
│  absence is implied by the offsets between segments.                   │
└────────────────────────────────────────────────────────────────────────┘
```
  
The complication comes from the need to stay canonical, which as far as segmentation is concerned, translates to every logical bit string having one and only one possible segmentation. We've already seen that a segment represents an island of rare bits in the sea of dominant bits, which means starting and ending with a rare bit. That becomes the first rule, where the complete segmentation rule set is:

##### Core Canonical Requirements
1. Segments MUST start and end with rare bits.  
2. The first segment MUST start at the first rare bit in the partition.  
3. Each time a we find a rare bit followed by DOMINANT_RUN_THRESHOLD adjacent dominant bits, the segment MUST be terminated at the last rare bit before the dominant run, and a next segment started at the first rare bit found after that, if any.  
4. The first rule of zero applies, also to the dominant bits following the last rare bit in the partition.  

##### Practical optimisations
5. Segments should not exceed MAX_SEGMENT_LEN_HINT bits, but can only be split between adjacent rare bits referred to a split opportunities. When a segment exceeds the maximum, it MUST split at the first opportunity before the maximum point if such an opportunity exists, otherwise it MUST be split at the first opportunity after the maximum point if such an opportunity exists, else it MUST be output as a single segment which the code must be able to handle even if it means more memory and/or more computations.  
6. A successful segment split resulting from MAX_SEGMENT_LEN_HINT will unavoidably yield a zero-length dominant run between every two segments split that way, which though it appears to violate the segmentation principle requiring DOMINANT_RUN_THRESHOLD spaces between segments, is allowed under the rules just like the first segment may be closer to the start of the partition than the threshold. All it really means is that the DOMINANT_RUN_THRESHOLD should never be assumed to have any significance other than being a value indicating at what point the new segment overhead is warranted by skipping over the space. I.e. forget talk, we don’t even think about zeros.  
7. Once a split has been effected, whatever remains due for segmentation, is processed the same way.   
  
> Calculated direct access is preferred, indexed near-direct access is a close second, but when it becomes unavoidable to spin through an entire dataset, the best you can do is to ensure that you only do so once. Doing everything deterministically in a single pass becomes the objective. If some re-iteration is unavoidable, the objective becomes to design the single-pass mechanism to selectively induce localised re-iteration to avoid a full second pass. This is the way  
  
#### **Stage 3: Chunks** (Covered in the next section after Segment Classification.)
  
Yes, you didn’t misread, there is a third level of decomposition involved after Partitioning and Segmentation, we may refer to as Chunks. They will be covered under the encoding heading, after Segment Classification.    
  
### Encoding - a balance between space and canon  
  
#### **Segment Classification**  
  
With all the empty partitions and large enough gaps between denser portions of the abstract bit vector shaved out of the data, most of the battle is already won, but the chances of the remaining segments still being impractical to work with remains high. We’ll need to get creative but before too much work goes into that, there are two easily tested simple cases we can deal with.  
  
Segments where the length and the popcount (a term that usually means number of set bits, but which we’ll mangle to mean number of rare bits) are the same constitute rare runs, which can be expressed as the obligatory offset, a type specifier and a length alone.  An all-rare segment must be classified as an RLE segment, and yes, it means that a single rare bit surrounded by enough dominant bits to meet the threshold is encoded as RLE with a length of 1.    
  
All other segments would contain mixtures of rare and dominant bits, in varying proportions, and sizes, for which the encoding would contain a certain amount of meta data overhead.  To prevent that overhead growing bigger than the data it encodes, the format also defines a value determined as the minimum size for chunk encoding to make sense. Anything smaller would be best encoded absolutely raw, i.e. just an offset, type and length followed by that many bits of data.   

```
┌─────────────────────────────────────────────────────────────────┐
│ Illustration 5: Segment Classification Decision Tree            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│                         Segment                                 │
│                            │                                    │
│              ┌─────────────┴─────────────┐                      │
│              │                           │                      │
│         length = popcount?          length ≠ popcount           │
│              │                           │                      │
│              ↓                           │                      │
│            ┌───┐                         │                      │
│            │RLE│              ┌──────────┴──────────┐           │
│            └───┘              │                     │           │
│         All rare bits    size < MIN_CHUNK?   size ≥ MIN_CHUNK   │
│         (offset+len)          │                     │           │
│                               ↓                     ↓           │
│                            ┌────┐              ┌──────┐         │
│                            │RAW │              │ MIX  │         │
│                            └────┘              └──────┘         │
│                         Output raw bits    Combinadic chunks    │
│                         directly           (Stage 3)            │
└─────────────────────────────────────────────────────────────────┘
```

Note: Segment type uses 1 bit (0=RLE, 1=MIX). Within MIX segments, each chunk's token uses 2 bits for its type tag (00=ENUM, 01=RAW, 10=RAW_RUN, 11=Reserved). These are distinct encodings at different levels of the hierarchy.
  
After that, we're into the tough stuff, the MIX segments with rare and dominant bits scattered so randomly that it becomes exceedingly costly to discern patterns, and even harder to keep canonical.
  
#### **Stage 3: Chunks in the MIX**  
  
Within a MIX type segment we come full circle back to rigid blocks, only now we already know the segment does not contain large enough dominant runs to omit. We’ll need to emit something for every chunk, it is just a matter of what, and as always, the rules are simple, deterministic and parameterised in the format specification to minimise wasted space.    
  
The balancing act is between the encoding space savings and the metadata required to achieve the savings, while steering clear of trying to compress noise (chaotic data).    
  
MIX segments are therefore chopped up into fixed width chunks, and encoded either as the popcount and rank of a combinadic number when the popcount is within range of the (precalculated) rank tables, or if the popcount exceeds the precalculated range the chunk is marked as such and output raw.   

```
┌──────────────────────────────────────────────────────────────────┐
│ Illustration 6: Chunk Encoding within MIX Segment                │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  MIX segment (180 bits total):                                   │
│  ┌──────────────┬──────────────┬──────────────┐                  │
│  │  Chunk 0     │  Chunk 1     │  Chunk 2     │                  │
│  │  (64 bits)   │  (64 bits)   │  (52 bits)   │                  │
│  └──────┬───────┴──────┬───────┴──────┬───────┘                  │
│         │              │              │                          │
│         ↓              ↓              ↓                          │
│    Combinadic     Combinadic      Raw bits                       │
│    (popcount=18)  (popcount=31)   (too short                     │
│    (rank=...)     (rank=...)      for tables)                    │
│                                                                  │
│  Each chunk 64 bits wide (except final partial chunk).           │
│  Encoding: combinadic if popcount in range, else raw.            │
└──────────────────────────────────────────────────────────────────┘
```
  
The final chunk of a segment would likely not be full, but the number of bits it is expected to contain can be calculated unambiguously from the segment’s bit size and the chunk size.   
  
And so concludes the rules to divide (and ignore most of) the data by which to identify which of the 2^2^64 possible combinations of bigints an SSK represents.   

```
┌─────────────────────────────────────────────────────────────────────┐
│ Illustration 7: Hierarchical Structure Overview                     │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  SSKDecoded (root)                                                  │
│  │                                                                  │
│  ├─ Partition 0 (IDs: 0x0000_0000_xxxx_xxxx)                        │
│  │  ├─ Segment 0 (RLE: offset=100, len=50)                          │
│  │  ├─ Segment 1 (MIX: offset=200, len=180)                         │
│  │  │  ├─ Chunk 0 (combinadic)                                      │
│  │  │  ├─ Chunk 1 (combinadic)                                      │
│  │  │  └─ Chunk 2 (raw, partial)                                    │
│  │  └─ Segment 2 (RAW: offset=500, len=12)                          │
│  │                                                                  │
│  ├─ Partition 5 (IDs: 0x0000_0005_xxxx_xxxx)                        │
│  │  └─ Segment 0 (RLE: offset=0, len=1)                             │
│  │                                                                  │
│  └─ Partition 42 (IDs: 0x0000_002A_xxxx_xxxx)                       │
│     ├─ Segment 0 (MIX: offset=1000, len=256)                        │
│     │  ├─ Chunk 0 (combinadic)                                      │
│     │  ├─ Chunk 1 (raw)                                             │
│     │  ├─ Chunk 2 (combinadic)                                      │
│     │  └─ Chunk 3 (combinadic)                                      │
│     └─ Segment 1 (RLE: offset=2000, len=8)                          │
│                                                                     │
│  Three-level hierarchy: Partitions → Segments → Chunks              │
│  Empty partitions omitted. Segments encode only non-dominant runs.  │
└─────────────────────────────────────────────────────────────────────┘
```
  
### Calculation - the rolling "real" window
  
If the abstract SSK bit vector could fit into a digital computer register or even in contiguous memory, not a single of the operations we define for the UDT or aggregate would venture beyond the trivial, but it doesn’t, and that complicates matters tremendously.    
  
But all isn’t lost. The computational complexity of implementing these simple operations on data that even in decoded form is as sparse as the data it represents, we can identify a limited set of scenarios, build algorithms for each, and plug in the operation to be performed at the bit level as a function parameter, functional programming style.    
  
> The keen observer would notice that SSKs have no arithmetic operators, despite the striking similarity between SSK values and arbitrary-width binary integers. That’s not to say SSK principles cannot be expanded to created a new approach to such numbers and how they are stored and managed, but arithmetic is well outside the scope of this application of the concepts.  Even the comparison operators defined does not consider the bits of the abstract bit vector to have anything resembling a place value, only a bit number.    

### Fundamental Algorithms

One fundamental algorithm takes one or more integers, and sets or clears their corresponding bits in the abstract bit vector.
The aggregate state transformation function as well as the ssk_add and ssk_remove functions are special cases of this with an input array of length 1, while the ssk_from_array would invoke the same algorithm with multiple values.    
  
Another fundamental algorithm takes two ssk values (in decoded form) and applies a binary operator to modify the one value with the other.   
  
A third would apply a unary operator to a single decoded SSK.  
  
There might be two flavours of the algorithms depending on whether it has to modify one of the values in place or always produce the result as a new value.    
  
An even more fundamental common approach makes it simpler - take the inputs, sort them if they still need to be sorted (encoded and decoded SSKs are always in sorted order), calculate the affected area, find and step through the affected area with a positional cursor tracking the current partition, respective segment and chunk from  
Which data is being copied into the working register, load the registers, apply the function, track impact on neighbouring sections, deal with the impact when a segment boundary is crossed on the output, and move on to the next block.    
  
Through that mechanism, even the complexity induced by the sparsity we are exploiting becomes simple solve-once-solve-everywhere solutions. It’s also the only time we quietly mention zero - to initialise registers correctly.    
  
Here's a quick review of some of the functions along with the set theory and bit twiddling roots they map to at the lowest level.  

### Set Operations Mapping

```
┌────────────────────────────────────────────────────────────────────┐
│ Illustration 8: Set Operations as Bit Operations                   │
├────────────────────────────────────────────────────────────────────┤
│                                                                    │
│  A:       [1 0 1 1 0]   represents {IDs: 0, 2, 3}                  │
│  B:       [0 1 1 0 1]   represents {IDs: 1, 2, 4}                  │
│                                                                    │
│  A ∪ B:   [1 1 1 1 1]   = {0,1,2,3,4}   (A | B)  union             │
│  A ∩ B:   [0 0 1 0 0]   = {2}           (A & B)  intersection      │
│  A ∖ B:   [1 0 0 1 0]   = {0, 3}        (A & ~B) except            │
│  A △ B:   [1 1 0 1 1]   = {0,1,3,4}     (A ^ B)  symmetric diff    │
│  ~A:      [0 1 0 0 1]   = {1, 4}        (~A)     complement        │
│                                                                    │
│  SSK operations map directly to bitwise logic at the lowest level, │
│  but operate on the sparse hierarchical representation.            │
└────────────────────────────────────────────────────────────────────┘
```
  

| SSK Function (A and B are SSK values) | Set Theory (A and B are sets) | C (A and B are binary integers)  | PL/SQL (A and B are int or bigint) | SQL SELECT (A and B are tables, views, or sub-queries) | Semantics |
| ------------------- | ------------ | ------ | -------------- | -------------------- | ---------------------------------------- |
| ssk_union(A,B) | A∪B | A \| B | A OR B | UNION (not UNION ALL) | Elements in A, B, or both. |
| ssk_intersect(A, B) | A∩B | A & B | A AND B | INTERSECT | Elements in both A and B. |
| ssk_complement(A) | Aᶜ or ~A | ~A | NOT A | - | Elements in U but not in A. |
| ssk_except | A∖B | A & ~B | A AND (NOT B)  | MINUS | Elements in A but not in B. |
| - | AΔB | A ^ B | A XOR B | - | Elements in either A or B, but not both. |
  
 
## Made it work? Now make it (even) fast(er).   

### From PL/pgSQL to C: Performance Gains

SSK’s parent project proved the functional concept in PL/pgSQL, but the hoops that had to jump through to get the job done made it clear that to meet requirements for production use will require a vastly more efficient implementation, which pointed unequivocally to a dedicated extension in C.   
  
Roughly the first two orders of magnitude performance upgrade (estimated) stemmed from the paradigm shift between SQL concepts and C concepts. Though PL/pgSQL is classified as a procedural language, hence the PL prefix, it is still heavily constrained to the primitives, types, and declarative nature of SQL, all of which but most significantly the latter, are fundamentally at odds with what SSK needs.  
  
The SSK extension was born as a way to realise this sacrilegious, anti-relational concept in PostgreSQL.   
  
What’s been discussed so far in this introduction adds another order of magnitude performance and even more space efficiency on top of what the move to C had achieved. Still, its downfall, if left unchecked, would be that using the aggregate mechanism (which offers a dedicated memory context for aggregates to avoid reconstructing the state every time) to create new SSKs would rarely be a viable option for most real-world applications where SSK values are stored in a column, changed incrementally in SQL that may even run from different sessions and transactions.The issue being that while the individual operations are relatively simple, often affecting a single bit of logical bit string, decoding an SSK is too expensive to do every time one of those operations is invoked. We need a way to retain the decoded SSK values in memory that lives as long as possible without killing the server.  
  
Though what we need could be roughly equated to memoising the decode function (in reference to Donald Michie’s memo-function concept circa 1968), we need a mechanism combining several specialised attributes for this purpose.  
  
  
### Caching for Speed: CAS, RC, CoW, and Deduplication

We need:  
1. **Content-Addressable Storage (CAS):** Using the *encoded value* as the key, and the pointer to the decoded value’s allocated and structured memory as the CAS entry.  
2. **Reference Counting (RC):** To keep track of how many functions, in whatever transactions, sessions and queries they occur, is using the decoding so we may know when we require immutability and when time-saving mutation is allowed.  
3. **Copy-on-Write (CoW):** When we need to apply a change to a decoding that is in use elsewhere, we start by making a copy of the value to apply the changes to, encode it, and write back the new encoded version to the store.   
4. **Deduplication/Interning:** When writing to the store, we check if such encoded value already exists in the store. If it doesn’t we add it, with a reference count of 1, but if it already exists, we simply increase the existing reference count and discard the memory we changed and encoded from.   
5. **Best Effort Optimisation:** The objective is saving on repeated decoding effort, but since the SSK is not a reference to the value, but the value itself, in encoded form, a value being absent from the store when required merely means the value is getting decoded again, not a crisis.  
6. **Tuneable Prioritisation:** Smart decisions about which entries to keep in memory and which to evict, based on multiple metrics, including MRU, reference count and memory size. A very large SSK could take as much space as many smaller SSKs, in decoded space, so whether the smaller SSKs should be sacrificed to keep the large one is a run-time optimisation driven by tuneable parameters and empirical data, as it has no impact on what really matters - the canonical representation of a subset as a scalar value.  
“Succinctly” a **Content-Addressable Reference-Counted Copy-on-Write Cache with Deduplication Tuneable and Eviction** is needed. Let’s quickly write one, OK?  
    

## Made it work, fast? Now, Now simplify it.

While all of the above, the is critically important to work through all ob the above, in all of the detail, in order to get a handle on the domain, it's edge cases, canonicity, bijection, efficiency and good citizenship, we dare as the next question - do we really need the implementation to be as complicated as the rationale behind it? As expected from the phrasing of the question, the obvious answer is no, we can simplify the code a great deal, now tht we know what to do. We'll dedicate a whole new document to that. See SSK_CHAPTER_2.md.



## **About size, reality and the road ahead**  
    
### Size and Efficiency: Worst-Case Guarantees

We've discussed monster numbers and extreme countermeasures against chaotic data. But here's the remarkable truth: **even in the worst possible case**—where data is so chaotic it defeats every optimisation we've designed—**SSK still produces encodings 8 to 16 times smaller than the equivalent associative table**. That's not marginal; it's transformative. Long before SSK runs out of space, the underlying data itself would hit the database's storage constraints.  

### Real-World Impact: Beyond the Parent Project

In its native use case—the production system that necessitated SSK's invention—SSKs replace associative tables not for space savings (a welcome surprise) but for **analytical power at transactional speeds**.The problem: many-to-many relationships that cannot be modelled as named associative entities. For myriad reasons, the standard approach—remapping M:N as two 1:M relationships through a join table which becomes a proper business entity—simply isn't viable for ad-hoc, unnamed relationships. Yet those relationships contain critical patterns.  
  
Standard database tools are poorly equipped for this:  
- They cannot convey what ad-hoc M:N data **implies**  
- They make exploiting known patterns difficult  
- They cannot detect pattern **presence** without offline analysis by a clever analyst  
  
The requirement? Detect, apply, and present these patterns in real-time during OLTP operations. SSK makes this possible by representing M:N relationships as scalar values—indexable, comparable, and analytically rich.  

### Future Implications: M:N Relationships and 5NF

Discovering SSK's broader applicability—and its unexpected space efficiency—drove the decision to release it into the public domain. What began as a solution to one system's OLTP analytics challenge may have implications for any database dealing with dynamic, unnamed many-to-many relationships at scale, and might even present opportunities to natively support M:N data and render SQL 5NF-friendly for the first time since inception. Such concepts are explored somewhat in the document CONVO.md, with the basic idea being that by storing its children in a scalar field of the parent table using an SSK, the fundamental blocker of 5NF—repeated parent keys in child tables—is eliminated, enabling decomposition depths previously impossible in relational systems.