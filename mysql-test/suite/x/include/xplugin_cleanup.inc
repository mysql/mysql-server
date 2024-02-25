## Script xplugin_cleanup.inc
#  This scripts cleans up all resources created on disk,
#  user accounts created by the test. Additioanlly it
#  ensures that all X Plugin sessions were closed.
#
## Usage
#
# --source ../include/xplugin_cleanup.inc

--source ../include/xplugin_wait_for_connection_closure.inc

--remove_files_wildcard $MYSQL_TMP_DIR *.xpl

disable_result_log;
disable_query_log;
drop schema if exists xtest;
enable_query_log;
enable_result_log;

--source include/xplugin_drop_user.inc
