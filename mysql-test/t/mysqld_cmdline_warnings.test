################################################################################
#
# Test warnings caused by mysqld's command line parameters
#
################################################################################

--echo #
--echo # WL#11109: Issue deprecation warnings on utf8mb3
--echo #

--let $MESSAGES_DIR =    `select @@lc_messages_dir`
--let $PLUGIN_DIR   = 	 `select @@plugin_dir`
--let $WL11109_DATADIR = $MYSQL_TMP_DIR/wl11109dir
--let $LOG_FILE =        $WL11109_DATADIR/mysqld.log
--let $MYSQLD_ARGS =     --no-defaults --plugin-dir=$PLUGIN_DIR --secure-file-priv="" --lc-messages-dir=$MESSAGES_DIR --datadir=$WL11109_DATADIR --log-error=$LOG_FILE

--let SEARCH_FILE=       $LOG_FILE

--mkdir $WL11109_DATADIR

# 1. Start the server to test 'utf8mb3'
# The server should warn about command line parameters and exit immediately on the uninitialized datadir:
--error 3
--exec $MYSQLD $MYSQLD_ARGS --character_set_server=utf8mb3 --character-set-filesystem=utf8mb3 2>&1

--echo # Warning on --character-set-server=utf8mb3:
--let SEARCH_PATTERN= \[Warning\] \[MY-[0-9]+] \[Server\] --character-set-server: 'utf8mb3' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release\. Please consider using UTF8MB4 in order to be unambiguous\.
--source include/search_pattern.inc

--echo # Warning on --character-set-filesystem=utf8mb3:
--let SEARCH_PATTERN= \[Warning\] \[MY-[0-9]+] \[Server\] --character-set-filesystem: 'utf8mb3' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release\. Please consider using UTF8MB4 in order to be unambiguous\.
--source include/search_pattern.inc


# 2. Start the server to test 'utf8mb3' and 'utf8mb3_general_ci'.
# The server should warn about command line parameters and exit immediately on the uninitialized datadir:
--error 3
--exec $MYSQLD $MYSQLD_ARGS --character_set_server=utf8mb3 --character-set-filesystem=utf8mb3  --collation-server=utf8mb3_general_ci 2>&1

--echo # Warning on --character-set-server=utf8mb3:
--let SEARCH_PATTERN= \[Warning\] \[MY-[0-9]+\] \[Server\] --character-set-server: The character set UTF8MB3 is deprecated and will be removed in a future release\. Please consider using UTF8MB4 instead\.
--source include/search_pattern.inc

--echo # Warning on --character-set-filesystem=utf8mb3:
--let SEARCH_PATTERN= \[Warning\] \[MY-[0-9]+\] \[Server\] --character-set-filesystem: The character set UTF8MB3 is deprecated and will be removed in a future release\. Please consider using UTF8MB4 instead\.
--source include/search_pattern.inc

--echo # Warning on --collation-server=utf8mb3_general_ci:
--let SEARCH_PATTERN= \[Warning\] \[MY-[0-9]+\] \[Server\] --collation-server: 'utf8mb3_general_ci' is a collation of the deprecated character set UTF8MB3\. Please consider using UTF8MB4 with an appropriate collation instead\.
--source include/search_pattern.inc

# Cleanup
--remove_files_wildcard $WL11109_DATADIR *
--rmdir $WL11109_DATADIR
