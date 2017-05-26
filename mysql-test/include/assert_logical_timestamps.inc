# ==== Purpose ====
#
# Assert that the binary log contains a specific sequence of logical
# timestamps.
#
# ==== Usage ====
#
# --let $binlog_file= FILENAME
# [--let $binlog_position= OFFSET]
# --let $logical_timestamps= LC1 SN1[;LC2 SN2[;LC3 SN3...]]
# [--let $save_binlog_position= 1]
# [--let $dont_print_logical_timestamps= 1]
# --source include/assert_logical_timestamps.inc
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
#   $logical_timestamps
#     This is a semicolon-separated list of pairs of logical
#     timestamps. Each pair consists of 'last_committed' as an
#     integer, followed by space, followed by 'sequence_number' as an
#     integer.  Note that this is whitespace-sensitive so there should
#     not be any other space characters than those separating
#     last_committed from sequence_number.
#
#   $save_binlog_position
#     If this flag is set, the script will source
#     include/save_binlog_position.inc after it has finished the
#     assert, making the next invocation automatically continue where
#     this invocation ended.  This is convenient if the script is to
#     be called multiple times in a sequence.
#
#   $dont_print_logical_timestamps
#     By default, $logical_timestamps is printed to the result log. If
#     this flag is set, that is suppressed.

--let $include_filename= include/assert_logical_timestamps.inc [$logical_timestamps]
if ($dont_print_logical_timestamps)
{
  --let $include_filename= include/assert_logical_timestamps.inc
}
--source include/begin_include_file.inc


--let $assert_file= `SELECT CONCAT('$MYSQLTEST_VARDIR/tmp/_alt_', UUID())`

# Run mysqlbinlog.
--let $_alt_offset=
if ($binlog_position)
{
  --let $_alt_offset= --start-position=$binlog_position
}
--let $_alt_datadir= `SELECT @@datadir`
--exec $MYSQL_BINLOG --force-if-open $_alt_offset $_alt_datadir/$binlog_file > $assert_file

# Check for match.
--let $assert_select= last_committed=.*sequence_number=.*
--let $assert_match= `SELECT CONCAT('last_committed=', REPLACE(REPLACE('$logical_timestamps', ';', '.*\n[^\n]*last_committed='), ' ', '\tsequence_number='))`
--let $assert_text= assert_logical_timestamps: $logical_timestamps
--let $extra_debug_info= Expected logical timestamps: $logical_timestamps
--source include/assert_grep.inc

--remove_file $assert_file


if ($save_binlog_position)
{
  --source include/save_binlog_position.inc
}


--let $include_filename= include/assert_logical_timestamps.inc [$logical_timestamps]
--source include/end_include_file.inc
