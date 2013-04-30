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
--let $option_text= `SELECT SUBSTR(REPLACE('$option', '"', ''), 3)`
while (`SELECT LOCATE('=', '$option_text')`)
{
  --let $dash_pos= `SELECT LOCATE('--', '$option_text')`
  --let $option_text= `SELECT CONCAT(SUBSTR('$option_text', 1, LOCATE('=', '$option_text') - 1), IF($dash_pos, CONCAT(' ', SUBSTR('$option_text', $dash_pos + 2)), ''))`
}
--echo
--echo ---- $option_text ----

# Print output
--replace_regex /SET @@SESSION.GTID_NEXT= '.*'/SET @@SESSION.GTID_NEXT= 'GTID'/ /([0-9A-Fa-f\-]{36})\:[0-9]+\-[0-9]+/UUID:#-#/
--exec $MYSQL_BINLOG --short-form $option $extra_options
