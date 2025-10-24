# SSK (SubSet Keys)

## Introduction

To SQL and Codd's RDM, which are like galaxies on opposite ends of the universe, SubSet Keys (SSKs) would live in the subspace by which they're unified.

## The Traditional Relational Database Approach

It is common knowledge that Relational Database Management Systems (RDBMSs), a.k.a. Relational Databases, work on the fundamental principle where any relationship (one-to-one, one-to-many, and even the many-to-many workaround involving an associative table) works on one single principle - records of the "many" side store the key to the "one" side as an attribute. Simple and effective, easy to understand, once you get that pattern embedded into your thinking, that is.

## The Mathematical Foundation: Codd's Relational Data Model

Most also know that Relational Databases were born out of mathematical set theory in the '70s by a man named Edward F. Codd, through what is called the Relational Data Model (RDM).

## The Overlooked Alternative

Yet almost nobody appreciates that the RDM's mathematical basis is entirely ambivalent about whether at the data level you store a parent key for each child or a set of children for each parent. To the maths, and as we will soon discover, in practice, there is no difference between the two. Yes, the one is 50 years ahead in development, but the original implementers could have chosen either mechanism, and then developments on the other would have been 50 years ahead.

The two alternatives may be mathematically each other's equals, and technically the prevailing approach today has the overwhelming advantage, but in terms of the value it is able to deliver to data users, owners and investors, the situation is reversed. 

## SQL's Critical Omission

That's because when the SQL language was being formulated, its formulators made a tiny but important, likely inadvertent, probably well-meaning, omission from its semantics. To represent the notion of a set, as required by the set theory roots, they represented sets as concrete things - tables, queries, views, anything that can be SELECTed from, even composites of those as the result of UNIONs, INTERSECT and MINUS. But that's it. They defined the identity of a row (or tuple as RDM had it), made a whole song and dance about it, but they never bothered to define what a set, or a subset's ID would be, beyond that table or view name and the name given in the SELECT statement as an alias. You could store a tuple's ID in a value or set of values, that was the whole point, but the identity of a subset of rows was considered out of bounds.

## Knuth's Contribution and Its Limitations

Ironically, the key to choosing the alternative representation was already written down and proven by the time the SQL team had to make their choice, but to be fair, it was only published a few short years before the project started and Donald Knuth's seminal work "The Art of Computer Programming" which contained the solution was not yet seen as seminal and a source of anything of practical use. Plus, even though Knuth did spell out the principle, his own analysis of its applicability would have excluded it from consideration in any case. 

For the less-informed, I'm referring of course to the linked concepts of sets as described by Knuth, and the concept of bit vectors as a way to represent sets. The caveat being that though bit-vectors are ideal for representing sets, the register size of a processor renders their applicable domain to no more than the bit-size of the computer. Large and/or sparse sets were seen as placing impractical space demands on the processors of the day. 

## My Discovery: Breaking Through the Perceived Limitations

Luckily for me, I knew nothing about the limitations Knuth placed on bit-vectors representing sets, or even that he made such a connection. So when I was faced with a burning need to deal with many-to-many data in a much, much more effective manner, I wasn't being held back by the presumed impossibilities the old masters attached to the problem I needed solving. I didn't know I was proving anyone wrong when I evolved a solution making full advantage of what I knew about my specific datasets and eventually of all such datasets in general. They say many things are impossible only until you've done them, but by my own experience, it's even better when you have no inkling about the theoretical impossibility you're facing. 

## What Are SubSet Keys?

So I came up with a way to build an indexable variable length byte array which is a canonical representation of any specific subset of a particular table, and called that value a SubSet Key. It is, at a logical level, one gigantic logical bit vector, squashed opportunistically into a very small series of bytes.

## The Problem with Many-to-Many Relationships

Having spent a large portion of my life designing and building innovative databases, I know a thing or two about how (and why) to avoid many-to-many relationships. Among other things, they are an absolute pain to present to users in a sensible manner, but an even bigger pain to try and make analytical sense of. Sufficient domain knowledge had always been key to turning associative tables into real tables that serve a business purpose, and using those to ease all those pains. Yet I found myself faced with a situation where I didn't merely lack the domain knowledge to follow the established pattern, such domain knowledge could not be determined beforehand. 

A real impasse, but I got creative and found a way to get the job done. But it left me somewhat apprehensive about what the cost would be. I assumed there would be a substantial price to pay for the enormous analytical benefits my solution brought with it, so I had to do the sums to determine if I could afford it. 

## The Shocking Performance Results

What I found was unfathomable, left me speechless and reeling, forcing me to go back time after time to check my facts and consult with others to check my math. Turns out, if you take a standard associative table, the space it requires exceeds the space you'd need to store the same information in SSKs in the worst case, by a factor close to 8. That's right. SSKs take up, at worst, around 1/8th of the space FKR (Foreign Key Referencing) takes. I was prepared to live with it if the ratio was 2:8 the other way around, so this was a bonus of epic proportions. 


