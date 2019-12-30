# ==== Purpose ====
#
# This test will check if the optimized delete (delete_all_rows) was
# used in a set of configurations regardless binary_format as the
# binary log is disabled.
#
# Set set of configurations vary:
# - Table engine: InnoDB
# - Session binlog_format: ROW, MIXED and STATEMENT
#
# ==== Related Bugs and Worklogs ====
#
# WL#8313: Set ROW based binary log format by default
#
# The testcase requires binary logging disabled.
--source include/not_log_bin.inc

CALL mtr.add_suppression('You need to use --log-bin to make --binlog-format work.');

--let $storage_engine=InnoDB
SET binlog_format= 'ROW';
--echo #
--echo # InnoDB, binlog_format= 'ROW'
--echo #
--source include/delete_all_rows.inc
--echo

SET binlog_format= 'MIXED';
--echo #
--echo # InnoDB, binlog_format= 'MIXED'
--echo #
--source include/delete_all_rows.inc
--echo

SET binlog_format= 'STATEMENT';
--echo #
--echo # InnoDB, binlog_format= 'STATEMENT'
--echo #
--source include/delete_all_rows.inc
