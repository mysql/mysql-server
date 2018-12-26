# Assert empty values in columns of performance_schema.thread table
#
# The columns must be empty when there is no user on
# X connection. This include file is dedicated for
# performance_schema_threads.test
#
# Usage:
# --source ../include/performance_schema_assert_empty_worker_threads.inc
#

--echo #
--echo # Get actual number of worker threads
--echo #

let $wait_condition= SELECT COUNT(*)=@@mysqlx_min_worker_threads FROM performance_schema.threads
                       WHERE name LIKE "thread/mysqlx/worker";
source include/wait_condition_or_abort.inc;

let $thread_entries=`SELECT @@mysqlx_min_worker_threads`;

let $assert_text= X Plugin must have idle worker threads;
let $assert_cond= [SELECT $thread_entries as `Result`, Result, 1] > 0;
--source include/assert.inc

--echo #
--echo # Assert all significant columns in thread table
--echo #
let $assert_text= All workers must have NULL in field `CONNECTION_TYPE`;
let $assert_cond= [SELECT COUNT(*) as `Result` FROM `performance_schema`.`threads`
                     WHERE `name` LIKE "thread/mysqlx/worker" and `CONNECTION_TYPE` is NULL, Result, 1] = $thread_entries;
--source include/assert.inc

let $assert_text= All workers must have NULL in field `PROCESSLIST_USER`;
let $assert_cond= [SELECT COUNT(*) as `Result` FROM `performance_schema`.`threads`
                     WHERE `name` LIKE "thread/mysqlx/worker" and `PROCESSLIST_USER` is NULL, Result, 1] = $thread_entries;
--source include/assert.inc

let $assert_text= All workers must have NULL in field `PROCESSLIST_HOST`;
let $assert_cond= [SELECT COUNT(*) as `Result` FROM `performance_schema`.`threads`
                     WHERE `name` LIKE "thread/mysqlx/worker" and `PROCESSLIST_HOST` is NULL, Result, 1] = $thread_entries;
--source include/assert.inc

let $assert_text= All workers must have NULL in field `PROCESSLIST_DB`;
let $assert_cond= [SELECT COUNT(*) as `Result` FROM `performance_schema`.`threads`
                     WHERE `name` LIKE "thread/mysqlx/worker" and `PROCESSLIST_DB` is NULL, Result, 1] = $thread_entries;
--source include/assert.inc

let $assert_text= All workers must have NULL in field `PROCESSLIST_COMMAND`;
let $assert_cond= [SELECT COUNT(*) as `Result` FROM `performance_schema`.`threads`
                     WHERE `name` LIKE "thread/mysqlx/worker" and `PROCESSLIST_COMMAND` is NULL, Result, 1] = $thread_entries;
--source include/assert.inc

let $assert_text= All workers must have NULL in field `PROCESSLIST_TIME`;
let $assert_cond= [SELECT COUNT(*) as `Result` FROM `performance_schema`.`threads`
                     WHERE `name` LIKE "thread/mysqlx/worker" and `PROCESSLIST_TIME` is NULL, Result, 1] = $thread_entries;
--source include/assert.inc

let $assert_text= All workers must have NULL in field `PROCESSLIST_STATE`;
let $assert_cond= [SELECT COUNT(*) as `Result` FROM `performance_schema`.`threads`
                     WHERE `name` LIKE "thread/mysqlx/worker" and `PROCESSLIST_STATE` is NULL, Result, 1] = $thread_entries;
--source include/assert.inc

let $assert_text= All workers must have NULL in field `PROCESSLIST_INFO`;
let $assert_cond= [SELECT COUNT(*) as `Result` FROM `performance_schema`.`threads`
                     WHERE `name` LIKE "thread/mysqlx/worker" and `PROCESSLIST_INFO` is NULL, Result, 1] = $thread_entries;
--source include/assert.inc

let $assert_text= All workers must have NULL in field `PROCESSLIST_COMMAND`;
let $assert_cond= [SELECT COUNT(*) as `Result` FROM `performance_schema`.`threads`
                     WHERE `name` LIKE "thread/mysqlx/worker" and `PROCESSLIST_COMMAND` is NULL, Result, 1] = $thread_entries;
--source include/assert.inc

let $assert_text= All workers must have NULL in field `PROCESSLIST_ID`;
let $assert_cond= [SELECT COUNT(*) as `Result` FROM `performance_schema`.`threads`
                     WHERE `name` LIKE "thread/mysqlx/worker" and `PROCESSLIST_ID` is NULL, Result, 1] = $thread_entries;
--source include/assert.inc
