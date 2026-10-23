# Count the View_change events in a binlog file of the current server.
#
# Parameters:
#   $vc_file   binlog file name (no directory)
# Sets:
#   $vc_count  number of View_change events in $vc_file
--let $_vc_port = `SELECT @@port`
--let $_vc_inc = $MYSQLTEST_VARDIR/tmp/gr_bolt_count_view_changes.inc
--exec $MYSQL --user=root --host=127.0.0.1 --port=$_vc_port -N -e "SHOW BINLOG EVENTS IN '$vc_file'" | grep -c 'View_change' | xargs printf -- '--let \044vc_count = %s\n' > $_vc_inc
--source $_vc_inc
--remove_file $_vc_inc
