# Routine to be sourced by
#   plugin/thread_pool/tests/mtr/t/thread_pool_func2a.test
#   plugin/thread_pool/tests/mtr/t/thread_pool_func2b.test
#
# Purpose:
# Check how different values of "pool of threads" variables show up within the
# content of performance_schema.tp_thread_group_state
#
# thread_pool_func2a and thread_pool_func2b differ only within
# - the values of "pool of threads" variables assigned at server startup
#   --> plugin/thread_pool/tests/mtr/t/thread_pool_func2?-master.opt
# - the files with expected results
#   --> plugin/thread_pool/tests/mtr/r/thread_pool_func2?.result
#   where the current values of "pool of threads" variables are printed
#
# Note:
#   - These tests are not intended to show how the content of
#     performance_schema.tp_thread_group_state changes depending on activity
#     within the server.
#   - Please take care that all values of the variables  differ between both
#     tests.
#
# Author:
# 2010-12-17 mleich develop tests for WL#5136 New Thread pool design
#

SET @admin_thread_groups = 1;

--echo # 0. Print the current setting of "pool of threads" variables which
--echo #    were assigned at server startup.
SELECT @@global.thread_pool_size, @@global.thread_pool_algorithm,
       @@global.thread_pool_prio_kickup_timer, @@global.thread_pool_stall_limit;

--echo # 1. We must have thread_pool_size different thread group id's within the table.
SELECT COUNT(DISTINCT TP_GROUP_ID) = @@global.thread_pool_size + @admin_thread_groups AS "Expect 1"
FROM performance_schema.tp_thread_group_state;

--echo # 2. All thread pool groups have the same values for the columns
--echo #    STALL_LIMIT, PRIO_KICKUP_TIMER, ALGORITHM
SELECT COUNT(DISTINCT STALL_LIMIT) = 1
   AND COUNT(DISTINCT PRIO_KICKUP_TIMER) = 1
   AND COUNT(DISTINCT ALGORITHM) = 1
AS "Expect 1"
FROM performance_schema.tp_thread_group_state;

--echo # 3. Check the current values of the columns STALL_LIMIT, PRIO_KICKUP_TIMER, ALGORITHM
--echo #    against the content of the "pool of threads" variables
SELECT STALL_LIMIT = @@GLOBAL.thread_pool_stall_limit
   AND PRIO_KICKUP_TIMER = @@GLOBAL.thread_pool_prio_kickup_timer
   AND ALGORITHM = CASE @@GLOBAL.thread_pool_algorithm
                   WHEN 0 THEN 'Low Concurrency'
                   WHEN 1 THEN 'High Concurrency'
                   ELSE 'Not expected algorithm' END
   AS "Expect 1"
FROM performance_schema.tp_thread_group_state LIMIT 1;

