# ==== Purpose ====
#
# Sets the transaction as high priority while the START TRANSACTION
# instruction is processed.
#
# It basically sets the thd->trx_priority=1 through a DBUG_EXECUTE_IF
# instruction while START TRANSACTION is executed.
#
# Given that this uses the dbug facility to inject a flag into the
# THD context, tests including this file will require debug builds to
# run.
#
# This include file is used in WL#6835's testing.
#
# ==== Usage ====
#
# --source include/start_transaction_high_priority.inc
#

--source include/have_debug.inc

--let $include_filename= start_transaction_high_prio.inc
--source include/begin_include_file.inc

--disable_query_log
SET GLOBAL DEBUG='+d,dbug_set_high_prio_trx';
--enable_query_log

START TRANSACTION /* HIGH PRIORITY */;

--disable_query_log
SET GLOBAL DEBUG='-d,dbug_set_high_prio_trx';
--enable_query_log

--let $include_filename= start_transaction_high_prio.inc
--source include/end_include_file.inc
