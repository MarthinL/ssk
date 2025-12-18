CREATE EXTENSION IF NOT EXISTS ssk;

-- Test aggregate functions
SELECT ssk_agg(id) AS agg_result FROM (VALUES (1), (2), (3)) AS t(id);