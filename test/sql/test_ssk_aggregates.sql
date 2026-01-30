CREATE EXTENSION IF NOT EXISTS ssk;

-- Test aggregate functions (deferred to Phase 1c)
-- SELECT ssk_agg(id) AS agg_result FROM (VALUES (1), (2), (3)) AS t(id);
SELECT 'ssk_agg deferred to Phase 1c' AS status;