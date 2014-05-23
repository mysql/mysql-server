# ==== Purpose ====
#
# Auxiliary file used by include/mysqlbinlog_start_stop_1.inc
#
# This prints a header, then runs mysqlbinlog once with given parameters.
#
# ==== Usage ====
#
# --let $extra_options= X
# --let $options= Y
# --source include/mysqlbinlog-start-stop-2.inc
#
# Parameters:
#
#   $extra_options
#     Options that will be passed to mysqlbinlog but not printed to the test log
#
#   $options
#     Options that will be printed as a header in the test log.


# Pretty-print $option_text
--let $option_text= `SELECT REPLACE('$options', '"', '')`
--let $eq_pos= `SELECT LOCATE('=', '$option_text')`
while ($eq_pos != 0)
{
  --let $dash_pos= `SELECT LOCATE('--', '$option_text', $eq_pos)`
  --let $left_part= `SELECT SUBSTR('$option_text', 1, $eq_pos - 1)`
  --let $right_part=
  if ($dash_pos != 0) {
    --let $right_part= `SELECT CONCAT(' ', SUBSTR('$option_text', $dash_pos + 2))`
  }
  --let $option_text= $left_part$right_part
  --let $eq_pos= `SELECT LOCATE('=', '$option_text')`
}
--let $option_text= `SELECT REPLACE('$option_text', '--', '')`

--echo
--echo ---- $option_text ----

# Print output
--replace_regex /SET @@SESSION.GTID_NEXT= '.*'/SET @@SESSION.GTID_NEXT= 'GTID'/ /([0-9A-Fa-f\-]{36})\:[0-9]+\-[0-9]+/UUID:#-#/
--exec $MYSQL_BINLOG --short-form $options $extra_options
