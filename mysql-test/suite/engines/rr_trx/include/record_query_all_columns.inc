################################################################################
#
# Stores results from a given query in a temporary table. 
#
# This table can then be used later in the same session for comparing results in
# various stages of a transaction (see check_repeatable_read.inc). 
# 
# The table name will be:  tmp$query_count
# where $query_count is the value of the counter for the number of stored
# queries in this session. Example: "tmp1"
#
# We increment the counter (session scope) for the number of queries so that
# checker scripts may 
#   a) know how many queries to compare
#   b) determine the name of the temp tables storing each query
#
# Assumptions:
#   - we may be in the middle of a transaction with autocommit OFF.
#   - queries include all columns of table (t1). This is because we want to
#     successfully add indexes to columns such as `pk`, `int1_key`, etc.
#
# Requires the following variables to be set:
#   $query - the query to be run, which results will be stored in a temp table.
#
# Modifies the following variables:
#   $query_count - the number of queries processed by this script so far in this
#                  session.
#   $tmptable - helper variable containing the name of the temp table.
#
# The pattern is "CREATE TEMPORARY TABLE tmpx SELECT ...". This allows us to 
# store query results by using SQL without causing implicit commits.
#
################################################################################

# increment the query counter
--inc $query_count

let $tmptable= tmp$query_count;

# Execute the query and store results in a new temp table.
# Creating indexes now because we cannot do that later withut causing implicit commit.
# Therefore we assume that columns of these names exist in the result set produced by the queries.
--echo *** Disabling query log (we may deadlock and not do this after all)
--disable_query_log
# Also disable warnings, because we get 'Unsafe to binlog' warnings for this with 'newer' server versions.
--disable_warnings
--echo *** Creating temp table with results from query '$query' unless we deadlock or time out.
--error 0, ER_LOCK_DEADLOCK, ER_LOCK_WAIT_TIMEOUT
--eval CREATE TEMPORARY TABLE $tmptable (PRIMARY KEY (`pk`), KEY (`int1_key`), KEY (`int2_key`), UNIQUE (`int1_unique`), UNIQUE (`int2_unique`)) $query
--enable_warnings

# We may not have been able to create temp table due to locking constraints.
# In that case, roll back the statement and skip the rest of the test.
--source suite/stress_tx_rr/include/check_for_error_rollback_skip.inc

--echo *** Enabling query log
--enable_query_log
