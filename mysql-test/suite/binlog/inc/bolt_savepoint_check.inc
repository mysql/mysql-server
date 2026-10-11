# Check one case of binlog_bolt_savepoint_matrix after it ended.
#
# Inputs:
#   $sp_case            case name
#   $sp_expect_ids      ids left in t1, comma-separated, or "none"
#   $sp_expect_promoted 1 if the transaction must have been promoted, else 0
#   $sp_expect_missed   expected change of the missed-optimization counter
#   $sp_expect_binlog_rows  rows of t1 the active binary log must insert
#
# Checks: retained rows; promotion and missed counters; the active binary log
# verifies its checksums and contains exactly the expected t1 row events; a
# promoted file has a valid BOLT header; no bolt_ temp file is left over.
--let $sp_ids = `SELECT IFNULL(GROUP_CONCAT(id ORDER BY id), 'none') FROM t1`
--let $assert_text = $sp_case: t1 holds exactly the expected rows
--let $assert_cond = "$sp_ids" = "$sp_expect_ids"
--source include/assert.inc
--let $sp_bad_rows = `SELECT COUNT(*) FROM t1 WHERE data <> REPEAT(CHAR(64 + MOD(id, 26)), LENGTH(data))`
--let $assert_text = $sp_case: every retained row has its own payload
--let $assert_cond = $sp_bad_rows = 0
--source include/assert.inc
--let $sp_bolt_after = query_get_value(SHOW GLOBAL STATUS LIKE 'Binlog_large_transaction_optimization_count', Value, 1)
--let $assert_text = $sp_case: promoted $sp_expect_promoted time(s)
--let $assert_cond = $sp_bolt_after = $sp_bolt_before + $sp_expect_promoted
--source include/assert.inc
--let $sp_missed_after = query_get_value(SHOW GLOBAL STATUS LIKE 'Binlog_large_transaction_optimization_missed_count', Value, 1)
--let $assert_text = $sp_case: missed optimizations +$sp_expect_missed
--let $assert_cond = $sp_missed_after = $sp_missed_before + $sp_expect_missed
--source include/assert.inc
--let $sp_file = query_get_value(SHOW BINARY LOG STATUS, File, 1)
--exec $MYSQL_BINLOG --force-if-open --verify-binlog-checksum $MYSQLD_DATADIR/$sp_file > /dev/null
# The shell writes a one-line include that sets the variable (\044 is '$';
# a literal dollar sign would be expanded by mysqltest).
--let $sp_rows_inc = $MYSQLTEST_VARDIR/tmp/bolt_sp_rows.inc
--exec $MYSQL_BINLOG --force-if-open -v $MYSQLD_DATADIR/$sp_file | grep -c '^### INSERT INTO .test.\..t1.' | xargs printf -- '--let \044sp_binlog_rows = %s\n' > $sp_rows_inc
--source $sp_rows_inc
--remove_file $sp_rows_inc
--let $assert_text = $sp_case: the binary log inserts $sp_expect_binlog_rows row(s) into t1
--let $assert_cond = "$sp_binlog_rows" = "$sp_expect_binlog_rows"
--source include/assert.inc
if ($sp_expect_promoted)
{
  --let $bolt_header_file = $sp_file
  --source ../inc/validate_bolt_header.inc
}
--let $sp_temp_inc = $MYSQLTEST_VARDIR/tmp/bolt_sp_temp.inc
--exec ls "$MYSQLD_DATADIR/#binlog_temp_files" | grep -c '^bolt_' | xargs printf -- '--let \044sp_temp_files = %s\n' > $sp_temp_inc
--source $sp_temp_inc
--remove_file $sp_temp_inc
--let $assert_text = $sp_case: no bolt_ temp file is left over
--let $assert_cond = "$sp_temp_files" = "0"
--source include/assert.inc
--disable_query_log
TRUNCATE t1;
--enable_query_log
