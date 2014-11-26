# Routine to be called by pfs_upgrade.test
# $out_file and $err_file must be set within pfs_upgrade.test.
#

--source include/count_sessions.inc
--exec $MYSQL_UPGRADE --skip-verbose --force > $out_file 2> $err_file
--source include/wait_until_count_sessions.inc

# Verify that mysql_upgrade complained about the performance_schema
--cat_file $err_file
--error 0,1
--remove_file $out_file
--error 0,1
--remove_file $err_file

--source include/mysql_upgrade_cleanup.inc