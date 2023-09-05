# This include file checks that the function statement_digest() and
# performance_schema produce the same digests.
#
# - $compare_digests_fn is the function called.
#
# - $compare_digests_query - is what the function is called with.
#
# - $compare_digests_pfs_query is executed and its digest picked up from
#   performance_schema.
#
# - $compare_digests_pfs_column is the column that is compared with the result
#   of the function.
#
# Only single quotes may be used in the queries.

--disable_query_log
SELECT enabled INTO @original_setting
FROM performance_schema.setup_consumers
WHERE NAME = 'statements_digest';

UPDATE performance_schema.setup_consumers
SET enabled = 'NO'
WHERE NAME = 'statements_digest';

# We inactivate statement digests in performance_schema before calling
# statement_digest(), as the function should work anyway.
eval SET @the_digest =
  $compare_digests_fn( "$compare_digests_query" );
--enable_query_log

--disable_query_log
UPDATE performance_schema.setup_consumers
SET enabled = 'YES'
WHERE NAME = 'statements_digest';

eval SET @ps_protocol = $PS_PROTOCOL;
--enable_query_log

--eval $compare_digests_pfs_query

# We do the conversion to 'utf8mb3' (in fact UTF-8 mb3) here for the few cases
# when the SQL text is UTF-8 mb4. For all other cases, this conversion
# doesn't change anything as long as we use UTF-8.
#
# We're doing a little trick if the test runs under --ps-protocol. In this
# case the digests won't match up, obviously, since the queries aren't parsed
# normally and hence aren't in performance_schema. In this case we just answer
# 'YES'. Even if we don't get any useful result, it is still necessary to run
# the tests to check for assertions.
eval
SELECT digest_text, IF( @the_digest = $compare_digests_pfs_column, 'YES', 'NO' )
AS digest_is_correct
FROM performance_schema.events_statements_history
WHERE sql_text = convert( "$compare_digests_pfs_query" USING utf8mb3 )
UNION
SELECT statement_digest_text( "$compare_digests_pfs_query" ), 'YES'
FROM (SELECT 1) a1 JOIN (SELECT @ps_protocol) a2;

--disable_query_log
UPDATE performance_schema.setup_consumers
SET enabled = @original_setting
WHERE NAME = 'statements_digest';
--enable_query_log

--let $compare_digests_fn=
--let $compare_digests_query=
--let $compare_digests_pfs_query=
--let $compare_digests_pfs_column=
