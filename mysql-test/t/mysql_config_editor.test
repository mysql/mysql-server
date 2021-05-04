
--echo #
--echo # Testing mysql_config_editor utility.
--echo #

# Note : a) mtr sets the 'MYSQL_TEST_LOGIN_FILE' environment
#           variable to "$opt_tmpdir/.mylogin.cnf".
#        b) The --password option in not tested as its
#           value cannot be specified at command line.

--echo ################################################
--echo # Tests for mysql_config_editor's insert command
--echo ################################################
--echo #
--echo # Default login path (client)
--exec $MYSQL_CONFIG_EDITOR set --user=test_user1 --host=localhost
--echo # done..
--echo # 'test-login-path1'
--exec $MYSQL_CONFIG_EDITOR set --login-path=test-login-path1 --user=test_user2 --host=127.0.0.1 
--echo # done..
--echo # 'test-login-path2'
--exec $MYSQL_CONFIG_EDITOR set --login-path=test-login-path2 --user=test_user3 --host=www.mysql.com
--echo # done..
--echo # 'test-login-path3'
--exec $MYSQL_CONFIG_EDITOR set --login-path=test-login-path3 --user=test_user4 --host=127.0.0.1 --socket=/tmp/configtest.sock --port=15000
--echo # done..

--echo
--echo ################################################
--echo # Tests for mysql_config_editor's print command
--echo ################################################
--echo #
--echo # Default path
--exec $MYSQL_CONFIG_EDITOR print 2>&1
--echo
--echo #
--echo # test-login-path1
--exec $MYSQL_CONFIG_EDITOR print --login-path=test-login-path1 2>&1
--echo
--echo #
--echo # test-login-path2
--exec $MYSQL_CONFIG_EDITOR print --login-path=test-login-path2 2>&1
--echo
--echo #
--echo # all the paths
--exec $MYSQL_CONFIG_EDITOR print --all 2>&1
--echo 
--echo #
--echo # Overwrite existing paths, test-login-path2 & default
--exec $MYSQL_CONFIG_EDITOR set --user=test_user4 --login-path=test-login-path2 --skip-warn
--exec $MYSQL_CONFIG_EDITOR set --user=test_user5 --skip-warn
--echo #
--echo # all the paths again
--exec $MYSQL_CONFIG_EDITOR print --all 2>&1

--echo 
--echo ################################################
--echo # Tests for mysql_config_editor's remove command
--echo ################################################
--echo #
--echo # Default path
--exec $MYSQL_CONFIG_EDITOR remove --skip-warn
--echo # done..
--echo # test-login-path1
--exec $MYSQL_CONFIG_EDITOR remove --login-path=test-login-path1
--echo # done..
--echo # test-login-path3
--exec $MYSQL_CONFIG_EDITOR remove --login-path=test-login-path3 --socket --port
--echo # done..

--echo
--echo ########################
--echo # Printing the leftovers
--echo ########################
--echo #
--echo # using all
--exec $MYSQL_CONFIG_EDITOR print --all

--echo
--echo ###############################################
--echo # Tests for mysql_config_editor's reset command
--echo ###############################################
--exec $MYSQL_CONFIG_EDITOR reset
--echo # done..
--echo # Print-all to check if everything got deleted.
--exec $MYSQL_CONFIG_EDITOR print --all

--echo
--echo ##############################################
--echo # Tests for mysql_config_editor's help command
--echo ##############################################
--replace_regex /.*mysql_config_editor.*\n// /.*Output debug log.*\n// /.*This is a non.debug version.*\n//
--exec $MYSQL_CONFIG_EDITOR help 2>&1
--echo # done..

--echo
--echo ######################
--echo # Testing client tools
--echo ######################
--echo #
--echo # Inserting login paths default & test-login-path1
--exec $MYSQL_CONFIG_EDITOR --verbose set --user=test_user1 --host=localhost 2>&1
--exec $MYSQL_CONFIG_EDITOR --verbose set --login-path=test-login-path1 --user=test_user2 --host=127.0.0.1 2>&1
--exec $MYSQL_CONFIG_EDITOR --verbose set --login-path=test-login-path2 --user=test_user3 --host=127.0.0.1 --socket=/tmp/configtest.sock --port=15000 2>&1
--echo # done..
--echo
--echo # Connecting using 'test_user1'
--echo #
--error 1
--exec $MYSQL 2>&1
--echo
--echo # Connecting using 'test_user2'
--echo #
--error 1
--exec $MYSQL --login-path=test-login-path1 2>&1

--echo # Creating user 'test_user1'
CREATE USER test_user1;
--echo # Creating user 'test_user2'
CREATE USER test_user2;
FLUSH PRIVILEGES;
--echo #
--echo # Now trying to connect again..
--echo # Connecting using 'test_user1'
--echo #
--exec $MYSQLADMIN --no-defaults -S $MASTER_MYSOCK -P $MASTER_MYPORT ping 2>&1
--exec $MYSQLADMIN --no-defaults -S $MASTER_MYSOCK -P $MASTER_MYPORT ping 2>&1
--echo
--echo # Connecting using 'test_user2'
--echo #
--exec $MYSQLADMIN --no-defaults --login-path=test-login-path1 --port=$MASTER_MYPORT ping 2>&1
--exec $MYSQLADMIN --no-defaults --login-path=test-login-path1 --port=$MASTER_MYPORT ping 2>&1

--echo #
--echo # Inserting a login path to test group suffix (client_suffix1)
--exec $MYSQL_CONFIG_EDITOR set --user=test_user3 --login-path=client_suffix1
--echo
--echo # Printing all the paths..
--exec $MYSQL_CONFIG_EDITOR print --all 2>&1
--echo
--echo # Now trying to connect using 'test_user3'
--echo # Note : In this case options from login
--echo #        paths 'client' (default) and 
--echo #        client_suffix1 will be read..
--error 1 
--exec $MYSQL --defaults-group-suffix=_suffix1 2>&1

--echo ## Running my_print_defaults ##
--echo #
--echo # (a) With --no-defaults option..
--echo # It should print the options under the default
--echo # login path 'client'.
--exec $MYSQL_MY_PRINT_DEFAULTS --no-defaults client 2>&1
--echo
--echo # (b) With --no-defaults & --login-path
--exec $MYSQL_MY_PRINT_DEFAULTS --no-defaults --login-path=test-login-path1 client 2>&1
--exec $MYSQL_MY_PRINT_DEFAULTS --no-defaults --login-path=test-login-path2 client 2>&1
--echo
--echo # (c) With --no-defaults & --defaults-group-suffix
--exec $MYSQL_MY_PRINT_DEFAULTS --no-defaults --defaults-group-suffix=_suffix1 client 2>&1
--echo
--echo # (d) With --no-defaults, --login-path & --defaults-group-suffix
--exec $MYSQL_MY_PRINT_DEFAULTS --no-defaults --login-path=test-login-path --defaults-group-suffix=1 client 2>&1
#--exec xterm -e gdb --args $MYSQL_MY_PRINT_DEFAULTS --no-defaults --login-path=test-login-path --defaults-group-suffix=1 client 2>&1

# Cleanup
--echo
--echo # Dropping users 'test_user1' & 'test_user2'
DROP USER test_user1, test_user2;
--echo
--echo ###############################
--echo # Resetting the test login file
--echo ###############################
--exec $MYSQL_CONFIG_EDITOR reset
--echo # done..

--echo
--echo ####################################
--echo # Testing with an invalid login file
--echo ####################################
--remove_file $MYSQL_TEST_LOGIN_FILE
--copy_file std_data/.mylogin.cnf $MYSQL_TEST_LOGIN_FILE
--cat_file $MYSQL_TEST_LOGIN_FILE

--echo # The 'invalid' login file should not cause
--echo # any problem.
--echo #
--exec $MYSQLADMIN --no-defaults -S $MASTER_MYSOCK -P $MASTER_MYPORT -uroot ping 2> /dev/null

--echo ###############################
--echo # Dropping the test login file
--echo ###############################
--remove_file $MYSQL_TEST_LOGIN_FILE
--echo
--echo #################################
--echo # Testing with login file removed
--echo #################################
--echo # Even if login file does not exist, the
--echo # tools should be able to continue
--echo # normally.
--exec $MYSQLADMIN --no-defaults -S $MASTER_MYSOCK -P $MASTER_MYPORT -uroot ping 2>&1

--echo
--echo ######################
--echo # WL#2284 Testing client tools with 32 user name
--echo ######################
--echo #
--echo # Inserting login paths default & test-login-path_user_26
--exec $MYSQL_CONFIG_EDITOR --verbose set --user=user_name_len_25_01234567 --host=localhost 2>&1
--exec $MYSQL_CONFIG_EDITOR --verbose set --login-path=test-login-path_user_26 --user=user_name_len_26_012345678 --host=127.0.0.1 2>&1
--echo # done..
--echo
--echo # Creating users for login
CREATE USER user_name_len_25_01234567@localhost;
CREATE USER user_name_len_26_012345678@localhost;

--echo # Connecting using 'user_name_len_25_01234567'
--echo #
--exec $MYSQLADMIN --no-defaults -S $MASTER_MYSOCK -P $MASTER_MYPORT ping 2>&1
--exec $MYSQLADMIN --no-defaults -S $MASTER_MYSOCK -P $MASTER_MYPORT ping 2>&1
--echo
--echo # Connecting using 'user_name_len_26_012345678'
--echo #
--exec $MYSQLADMIN --no-defaults --login-path=test-login-path_user_26 --port=$MASTER_MYPORT ping 2>&1
--exec $MYSQLADMIN --no-defaults --login-path=test-login-path_user_26 --port=$MASTER_MYPORT ping 2>&1

# Cleanup
--echo
--echo # Dropping users 'user_name_len_25_01234567@localhost' & 'CREATE USER user_name_len_26_012345678@localhost'
DROP USER user_name_len_25_01234567@localhost,user_name_len_26_012345678@localhost;
--echo
--echo ###############################
--echo # Resetting the test login file
--echo ###############################
--exec $MYSQL_CONFIG_EDITOR reset
--echo # done..


--echo #### End of test ####


--echo #
--echo # Bug #24557925: MYSQL_CONFIG_EDITOR CAN MAKE SERVER UNBOOTABLE
--echo #


--exec $MYSQL_CONFIG_EDITOR set --login-path=mysqld --host=test_user5

--echo # Restarting the server. Should work
--source include/restart_mysqld.inc

--echo # Cleanup
--exec $MYSQL_CONFIG_EDITOR remove --login-path=mysqld
--remove_file $MYSQL_TEST_LOGIN_FILE

--echo #
--echo # Bug#29861961: MYSQL_CONFIG_EDITOR CAN NOT DEAL PASSWORD WITH "#"
--echo #

CREATE USER 'test#user1'@'localhost';
--exec $MYSQL_CONFIG_EDITOR set --login-path=test --user=test#user1 --host=127.0.0.1 2>&1

--exec $MYSQLADMIN --no-defaults --login-path=test --port=$MASTER_MYPORT ping 2>&1

--echo # Cleanup
DROP USER 'test#user1'@'localhost';
--exec $MYSQL_CONFIG_EDITOR remove --login-path=test
--remove_file $MYSQL_TEST_LOGIN_FILE

--echo # End of 5.6 tests

--echo #
--echo # Bug #19953349: MYSQL_CONFIG_EDITOR DOES NOT ESCAPE STRINGS
--echo #

CREATE USER 'test1 test1'@'localhost';

--exec $MYSQL_CONFIG_EDITOR set --login-path=test11 --user="test1 test1" --host=127.0.0.1 2>&1
--echo test: must contain space
--exec $MYSQL_CONFIG_EDITOR print --login-path=test11

--echo # test: must not fail with a space in the name
--exec $MYSQLADMIN --no-defaults --login-path=test11 --port=$MASTER_MYPORT ping 2>&1

--echo # cleanup
DROP USER 'test1 test1'@'localhost';
--remove_file $MYSQL_TEST_LOGIN_FILE

--echo # End of 8.0 tests
