--
-- Copyright (c) 2020-2025 Marthin Laubscher
-- All rights reserved. See LICENSE for details.
--

-- SSK Extension SQL Script

-- Create the SSK type
CREATE TYPE ssk;

-- Input/Output functions
CREATE FUNCTION ssk_in(cstring) RETURNS ssk AS 'ssk', 'ssk_in' LANGUAGE C IMMUTABLE STRICT;
CREATE FUNCTION ssk_out(ssk) RETURNS cstring AS 'ssk', 'ssk_out' LANGUAGE C IMMUTABLE STRICT;

-- Type definition
CREATE TYPE ssk (
    INPUT = ssk_in,
    OUTPUT = ssk_out,
    STORAGE = extended,
    ALIGNMENT = int4,
    PASSEDBYVALUE = false,
    INTERNALLENGTH = VARIABLE
);

CREATE FUNCTION ssk_version() RETURNS text AS 'ssk', 'ssk_version' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
COMMENT ON FUNCTION ssk_version() IS 'Returns the SSK extension version';

-- Constructor and manipulation functions
CREATE FUNCTION ssk() RETURNS ssk AS 'ssk', 'ssk_new' LANGUAGE C IMMUTABLE;
CREATE FUNCTION ssk(bigint) RETURNS ssk AS 'ssk', 'ssk_new' LANGUAGE C IMMUTABLE;

CREATE FUNCTION ssk_add(ssk, bigint) RETURNS ssk AS 'ssk', 'ssk_add' LANGUAGE C IMMUTABLE;
CREATE FUNCTION ssk_remove(ssk, bigint) RETURNS ssk AS 'ssk', 'ssk_remove' LANGUAGE C IMMUTABLE;

-- Containment and set operations
CREATE FUNCTION ssk_contains(ssk, bigint) RETURNS boolean AS 'ssk', 'ssk_contains' LANGUAGE C IMMUTABLE;
CREATE FUNCTION ssk_is_contained(bigint, ssk) RETURNS boolean AS 'ssk', 'ssk_is_contained' LANGUAGE C IMMUTABLE;

CREATE FUNCTION ssk_union(ssk, ssk) RETURNS ssk AS 'ssk', 'ssk_union' LANGUAGE C IMMUTABLE;
CREATE FUNCTION ssk_intersect(ssk, ssk) RETURNS ssk AS 'ssk', 'ssk_intersect' LANGUAGE C IMMUTABLE;
CREATE FUNCTION ssk_except(ssk, ssk) RETURNS ssk AS 'ssk', 'ssk_except' LANGUAGE C IMMUTABLE;

-- Cardinality and introspection
CREATE FUNCTION ssk_cardinality(ssk) RETURNS bigint AS 'ssk', 'ssk_cardinality' LANGUAGE C IMMUTABLE;
CREATE FUNCTION ssk_is_empty(ssk) RETURNS boolean AS 'ssk', 'ssk_is_empty' LANGUAGE C IMMUTABLE;
CREATE FUNCTION ssk_unnest(ssk) RETURNS SETOF bigint AS 'ssk', 'ssk_unnest' LANGUAGE C IMMUTABLE;
CREATE FUNCTION ssk_from_array(bigint[]) RETURNS ssk AS 'ssk', 'ssk_from_array' LANGUAGE C IMMUTABLE;
CREATE FUNCTION ssk_to_array(ssk) RETURNS bigint[] AS 'ssk', 'ssk_to_array' LANGUAGE C IMMUTABLE;

-- Comparison and casting
CREATE FUNCTION ssk_cmp(ssk, ssk) RETURNS int AS 'ssk', 'ssk_cmp' LANGUAGE C IMMUTABLE;
CREATE FUNCTION length(ssk) RETURNS bigint AS 'ssk', 'ssk_length' LANGUAGE C IMMUTABLE;

CREATE CAST (bigint[] AS ssk) WITH FUNCTION ssk_from_array(bigint[]) AS IMPLICIT;
CREATE CAST (ssk AS bigint[]) WITH FUNCTION ssk_to_array(ssk) AS IMPLICIT;

-- Aggregate state function
CREATE FUNCTION ssk_agg_sfunc(ssk, bigint) RETURNS ssk AS 'ssk', 'ssk_sfunc' LANGUAGE C;
CREATE FUNCTION ssk_agg_finalfunc(ssk) RETURNS ssk AS 'ssk', 'ssk_finalfunc' LANGUAGE C;

-- Aggregate function
CREATE AGGREGATE ssk_agg(bigint) (
    SFUNC = ssk_agg_sfunc,
    STYPE = ssk,
    FINALFUNC = ssk_agg_finalfunc
);
