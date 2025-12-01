-- SSK extension SQL definitions
-- complies with extension packaging requirements

\echo Use "CREATE EXTENSION ssk" to load this file. \quit

CREATE FUNCTION ssk_version()
RETURNS text
AS 'MODULE_PATHNAME', 'ssk_version'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

COMMENT ON FUNCTION ssk_version() IS 'Returns the SSK extension version';