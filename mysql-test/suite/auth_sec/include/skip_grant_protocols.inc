## Starts the server from the new datadir created at $MYSQL_TMP_DIR/datadir1,
## and creates a new user with non-default plugin type.
## Caller must cleanup the newly created datadir

--echo
--echo # Set different paths for --datadir
let $MYSQLD_DATADIR1 = $MYSQL_TMP_DIR/datadir1/data;

--echo
--echo # Create custom datadir path
--mkdir $MYSQL_TMP_DIR/datadir1

let BOOTSTRAP_SQL=$MYSQL_TMP_DIR/boot.sql;
let $start_page_size=`SELECT variable_value FROM performance_schema.global_status WHERE LOWER(variable_name) =  'innodb_page_size'`;
#Create bootstrap file
--write_file $BOOTSTRAP_SQL
CREATE DATABASE test;
EOF

let NEW_CMD = $MYSQLD --no-defaults --initialize-insecure --datadir=$MYSQLD_DATADIR1 --init-file=$BOOTSTRAP_SQL</dev/null>>$MYSQLTEST_VARDIR/tmp/bootstrap.log 2>&1 --innodb-page-size=$start_page_size;

--echo
--echo # Run the bootstrap command to create a new datadir
--exec $NEW_CMD

--echo
--echo # Start the DB server with the new datadir
--replace_result $MYSQLD_DATADIR1 MYSQLD_DATADIR1
--let $restart_parameters="restart: --datadir=$MYSQLD_DATADIR1"
--source include/restart_mysqld.inc

SELECT current_user;
SELECT user, plugin FROM mysql.user;

--echo
--echo # Set the password for the root user
SET PASSWORD = 'passwd';

--echo
--echo # Create a new user
CREATE USER 'new_user' IDENTIFIED WITH 'caching_sha2_password' BY 'passwd';
--echo

--remove_file $BOOTSTRAP_SQL
