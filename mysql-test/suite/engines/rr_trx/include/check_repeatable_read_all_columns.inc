################################################################################
#
# Verifies that queries in a REPEATABLE READ transaction is indeed repeatable.
# Wrong results are shown as a result set based on one or more specially crafted
# queries. Normally these result sets should be empty.
#
# We want to verify that reads (SELECTs) are indeed repeatable during a 
# REPEATABLE READ transaction.
#
# Generally, queries which should yield the same results at one moment in time 
# should also yield the same results later in the same transaction. In some 
# configurations, however, phantom reads are allowed (may e.g. depend on 
# settings such as falcon_consistent_read).
#
# The check will fail if rows are changed or missing when comparing later
# queries to earlier ones.
# TODO: Phantom reads.
#       Note: There is a separate test looking for `is_uncommitted` = 1.
#
# Assumptions:
#   - we are in a REPEATABLE READ transaction with autocommit OFF.
#   - queries include all columns of table (t1) (we SELECT columns by name)
#
# Requires/using the following variables:
#   $query_count - the number of queries to compare.
#                  Will also be used to deduce the name of the temp table in
#                  which the query results should be stored (see 
#                  record_query_all_columns.inc).
#
################################################################################

# Show results of next queries. Empty results is OK. Non-empty means failure.
--enable_result_log

# The mysqltest language is unfortunaltely not very flexible, but we try our
# best to compare query results this way:
#   - For each query, compare with previous query
#     - this requires that at least 2 queries have been stored
#   - Number of queries should be stored as $query_count
#   - Results should be stored in temp tables with names ending with the query
#     number, and with prefix "tmp". 
#   - E.g. compare "tmp2" with "tmp1", "tmp3" with "tmp2", "tmp4" with "tmp3" etc.
#   - Fail the test once we detect changed or missing or invalid extra rows in
#     latter query.
#   ?????
#   - Problem is that if one of the queries deadlocked or timed out, we may not
#     have enough result sets to compare, so output will vary depending on this.
#     Still we need the output from these checks to see which rows are missing or
#     changed.
#     So, if we don't have enough queries we fake "correct output" to make mysqltest
#     happy.
#
# Unfortunately, we need to utilize SQL and spend client-server roundtrips
# in order to do some computations that the mysqltest language does not handle.
# We try to use mysqltest variables instead where possible, as this should be 
# less expensive in terms of CPU usage and time spenditure.

#
# First, check that we have at least two query results stored.
# We need at least 2 to be able to compare.
# Some results may not have been stored due to locking errors (see record_query_all_columns.inc), so
# we cannot assume that we always have at least 2 query results stored.
# If less than 2 query results are stored, return to calling test/script.
#
if (`SELECT IF($query_count > 1, 1, 0)`)
{
    
    --echo ***************************************************************************
    --echo * Checking REPEATABLE READ by comparing result sets from same transaction
    --echo ***************************************************************************

    --echo *** Query log disabled. See include files used by test for query details.
    --disable_query_log

    let $queryA= 1;
    let $queryB= 2;
    let $more_queries= $query_count;

    # We start out by comparing the first 2 queries, so the while loop should run
    # $query_count - 1 times. (If we have 3 queries, compare 1 with 2, 2 and 3).
    --dec $more_queries

    while ($more_queries)
    {
        # We still have one or more queries that have not been compared to the
        # previous query.
        # Compare queryB ("current query") with queryA ("previous query")

        #--source suite/stress_tx_rr/include/compare_queries_with_pk.inc

        let $tableA= tmp$queryA;
        let $tableB= tmp$queryB;

        --echo *** Comparing query $queryA (A) with query $queryB (B):

        #
        # In the following queries, 'SELECT * ...' could have been used instead of
        # 'SELECT tmp1.pk AS ...' etc., but the latter makes it easier to compare the first
        # result set to the second in test/diff output.



        ###########################
        # Detect extra rows:
        # Allow phantoms in some configurations:
        #   - InnoDB default settings
        #
        ###########################
        # TODO: Execute a query against tmp1 and tmp2 which selects new rows (rows
        #       present in tmp2 that are not present in tmp1) that are of the uncommitted
        #       variety (field `is_uncommitted` = 1).
        # E.g. something like:
        #  SELECT ...
        #   FROM tmp2 LEFT JOIN tmp1
        #   ON tmp1.`pk` = tmp2.`pk`
        #   WHERE tmp1.`int1` IS NULL
        #   OR tmp1.`int1_key` IS NULL
        #   OR tmp1.`int1_unique` IS NULL
        #   OR tmp1.`int2` IS NULL
        #   OR tmp1.`int2_key` IS NULL
        #   OR tmp1.`int2_unique` IS NULL
        #   AND tmp2.`is_uncommitted` = 1;


        --echo ###########################
        --echo # Detect missing rows:
        --echo ###########################


        eval SELECT    $tableA.pk AS 'A.pk',
                      $tableB.pk AS 'B.pk',
                      $tableA.id AS 'A.id',
                      $tableB.id AS 'B.id',
                      $tableA.`int1` AS 'A.int1',
                      $tableB.`int1` AS 'B.int1',
                      $tableA.`int1_key` AS 'A.int1_key',
                      $tableB.`int1_key` AS 'B.int1_key',
                      $tableA.`int1_unique` AS 'A.int1_unique',
                      $tableB.`int1_unique` AS 'B.int1_unique',
                      $tableA.`int2` AS 'A.int2',
                      $tableB.`int2` AS 'B.int2',
                      $tableA.`int2_key` AS 'A.int2_key',
                      $tableB.`int2_key` AS 'B.int2_key',
                      $tableA.`int2_unique` AS 'A.int2_unique',
                      $tableB.`int2_unique` AS 'B.int2_unique',
                      $tableA.`for_update` AS 'A.for_update',
                      $tableB.`for_update` AS 'B.for_update',
                      $tableA.timestamp AS 'A.timestamp',
                      $tableB.timestamp AS 'B.timestamp',
                      $tableA.`connection_id` AS 'A.connection_id',
                      $tableB.`connection_id` AS 'B.connection_id',
                      $tableA.`thread_id` AS 'A.thread_id',
                      $tableB.`thread_id` AS 'B.thread_id',
                      $tableA.`is_uncommitted` AS 'A.is_uncommitted',
                      $tableB.`is_uncommitted` AS 'B.is_uncommitted',
                      $tableA.`is_consistent` AS 'A.is_consistent',
                      $tableB.`is_consistent` AS 'B.is_consistent'
                FROM $tableA LEFT JOIN $tableB
                  ON $tableA.`pk` = $tableB.`pk`
                WHERE $tableB.`pk` IS NULL;

    #
    #               OR $tableB.`int1_key` IS NULL
    #               OR $tableB.`int1_unique` IS NULL
    #               OR $tableB.`int2` IS NULL
    #               OR $tableB.`int2_key` IS NULL
    #               OR $tableB.`int2_unique` IS NULL;

        --echo
        --echo ###########################
        --echo # Detect changed rows:
        --echo ###########################

        eval SELECT  $tableA.pk AS 'A.pk',
                      $tableB.pk AS 'B.pk',
                      $tableA.id AS 'A.id',
                      $tableB.id AS 'B.id',
                      $tableA.`int1` AS 'A.int1',
                      $tableB.`int1` AS 'B.int1',
                      $tableA.`int1_key` AS 'A.int1_key',
                      $tableB.`int1_key` AS 'B.int1_key',
                      $tableA.`int1_unique` AS 'A.int1_unique',
                      $tableB.`int1_unique` AS 'B.int1_unique',
                      $tableA.`int2` AS 'A.int2',
                      $tableB.`int2` AS 'B.int2',
                      $tableA.`int2_key` AS 'A.int2_key',
                      $tableB.`int2_key` AS 'B.int2_key',
                      $tableA.`int2_unique` AS 'A.int2_unique',
                      $tableB.`int2_unique` AS 'B.int2_unique',
                      $tableA.`for_update` AS 'A.for_update',
                      $tableB.`for_update` AS 'B.for_update',
                      $tableA.timestamp AS 'A.timestamp',
                      $tableB.timestamp AS 'B.timestamp',
                      $tableA.`connection_id` AS 'A.connection_id',
                      $tableB.`connection_id` AS 'B.connection_id',
                      $tableA.`thread_id` AS 'A.thread_id',
                      $tableB.`thread_id` AS 'B.thread_id',
                      $tableA.`is_uncommitted` AS 'A.is_uncommitted',
                      $tableB.`is_uncommitted` AS 'B.is_uncommitted',
                      $tableA.`is_consistent` AS 'A.is_consistent',
                      $tableB.`is_consistent` AS 'B.is_consistent'
                FROM $tableB INNER JOIN $tableA
                  ON $tableB.`pk` = $tableA.`pk`
                WHERE $tableB.`int1` <> $tableA.`int1`
                  OR $tableB.`int1_key` <> $tableA.`int1_key`
                  OR $tableB.`int1_unique` <> $tableA.`int1_unique`
                  OR $tableB.`int2` <> $tableA.`int2`
                  OR $tableB.`int2_key` <> $tableA.`int2_key`
                  OR $tableB.`int2_unique` <> $tableA.`int2_unique`;

        --dec $more_queries
        --inc $queryA
        --inc $queryB

    }
    --enable_query_log
}


## Cleanup is skipped because temporary tables and prepared statements will
## be cleaned up automatically by the server when this session ends, and we
## want to have as few client-server roundtrips as possible (thus avoid 
## unnecessary SQL statement executions).
