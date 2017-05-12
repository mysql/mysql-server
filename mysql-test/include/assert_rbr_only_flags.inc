# ==== Purpose ====
#
# Assert that the binary log contains a specific sequence of
# transactions according to the rbr_only flag.
#
# ==== Usage ====
#
# --let $binlog_file= FILENAME
# [--let $binlog_position= OFFSET]
# --let $rbr_only_flags= F1[,F2[,F3...]]
# [--let $save_binlog_position= 1]
# [--let $dont_print_rbr_only_flags= 1]
# --source include/assert_rbr_only_flags.inc
#
# Parameters:
#
#   $binlog_file
#     The binary log to look in.
#
#   $binlog_position
#     By default, this script starts reading from the beginning of the
#     binary log. Set this to start reading from some specific offset.
#
#   $rbr_only_flags
#     This is a comma-separated list of rbr_only flags. Each flag
#     consists of 'yes' or 'no'. Note that this is whitespace-sensitive.
#
#   $save_binlog_position
#     If this flag is set, the script will source
#     include/save_binlog_position.inc after it has finished the
#     assert, making the next invocation automatically continue where
#     this invocation ended.  This is convenient if the script is to
#     be called multiple times in a sequence.
#
#   $dont_print_rbr_only_flags
#     By default, $rbr_only_flags is printed to the result log. If
#     this flag is set, that is suppressed.

--let $include_filename= include/assert_rbr_only_flags.inc [$rbr_only_flags]
if ($dont_print_rbr_only_flags)
{
  --let $include_filename= include/assert_rbr_only_flags.inc
}
--source include/begin_include_file.inc


--let $assert_file= `SELECT CONCAT('$MYSQLTEST_VARDIR/tmp/_arof_', UUID())`

# Run mysqlbinlog.
--let $_arof_offset=
if ($binlog_position)
{
  --let $_arof_offset= --start-position=$binlog_position
}
--let $_arof_datadir= `SELECT @@datadir`
--exec $MYSQL_BINLOG --force-if-open $_arof_offset $_arof_datadir/$binlog_file > $assert_file

# Check for match.
--let $assert_select= rbr_only=.*
#--let $assert_match= `SELECT CONCAT('^[^\n]*last_committed=', REPLACE(REPLACE('$logical_timestamps', ';', '.*\n[^\n]*last_committed='), ' ', '\tsequence_number='))`
--let $assert_match= `SELECT CONCAT('^[^\n]*rbr_only=', REPLACE('$rbr_only_flags', ',', '.*\n[^\n]*rbr_only='))`
--let $assert_text= assert_gtid_rbr_only_flags: $rbr_only_flags
--let $extra_debug_info= Expected GTID rbr_only flags: $rbr_only_flags
--source include/assert_grep.inc

--remove_file $assert_file


if ($save_binlog_position)
{
  --source include/save_binlog_position.inc
}


--let $include_filename= include/assert_rbr_only_flags=.inc [$rbr_only_flags]
--source include/end_include_file.inc
