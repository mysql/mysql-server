# Include this at the end of any test using mysql_upgrade

let $MYSQLD_DATADIR= `select @@datadir`;

# The server should have created a mysql_upgrade_history file,
# so the following command will do a clean-up.
--remove_file $MYSQLD_DATADIR/mysql_upgrade_history

#
# See mysql_upgrade_preparation.inc
#
# Restore the original timestamps in:
# - table mysql.user
# - table mysql.tables_priv
# for the mysql.sys user.
#
# Upgrade causes data to be re created for the sys schema,
# which changes the timestamps.
# Restoring the original values for mtr --check-testcases
#

--disable_query_log
--disable_result_log

--eval UPDATE mysql.user SET `password_last_changed` = '$saved_sys_timestamp_1' WHERE `Host` = 'localhost' AND `User` = 'mysql.sys';

--eval UPDATE mysql.tables_priv SET `Timestamp` = '$saved_sys_timestamp_2' WHERE `Host` = 'localhost' AND `Db` = 'sys' AND `User` = 'mysql.sys' AND `Table_name` = 'sys_config';

--enable_result_log
--enable_query_log
