# Auxaliary file for printing optional metadata in table_map_log_event
# Usage :
# --let $binlog_file=
# [--let $stop_position]
# [--let $print_primary_key]
# --source common/binlog/print_optional_metadata.inc

--let $output_file= $MYSQLTEST_VARDIR/tmp/mysqlbinlog.output

--let $_stop_position_opt=
if ($stop_position)
{
  --let $_stop_position_opt=--stop-position=$stop_position
}

--exec $MYSQL_BINLOG -F --print-table-metadata $_stop_position_opt $binlog_file > $output_file

--let $grep_pattern= # (?:Columns\(| {8})
--let $grep_file= $output_file
--source include/grep_pattern.inc

if ($print_primary_key)
{
  --let $grep_pattern= # Primary Key
  --source include/grep_pattern.inc
}
--remove_file $output_file
--let $stop_position=
--let $_stop_position_opt=

