# ==== Purpose ====
#
# Diff the output of a statement on master and slave
#
# ==== Usage =====
#
# let $diff_statement= SELECT * FROM t1 WHERE a < 100;
# source include/diff_master_slave.inc;

--echo source include/diff_master_slave.inc;
disable_query_log;
disable_result_log;

exec $MYSQL test -e "$diff_statement" > $MYSQLTEST_VARDIR/tmp/diff_master.out;
sync_slave_with_master;
exec $MYSQL_SLAVE test -e "$diff_statement" > $MYSQLTEST_VARDIR/tmp/diff_slave.out;

diff_files $MYSQLTEST_VARDIR/tmp/diff_master.out $MYSQLTEST_VARDIR/tmp/diff_slave.out;

enable_result_log;
enable_query_log;
