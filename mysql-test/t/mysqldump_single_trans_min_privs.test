#
# BUG 35020512 - Provide an option in mysqldump to disable
# 'FLUSH TABLES WITH READ LOCK' so that RELOAD privilege is not required
#

--echo # Create the test database
CREATE DATABASE bug_test;
USE bug_test;

--echo # Create the test table
CREATE TABLE bug(n INT);

--echo # Enter random data which could be dumped
INSERT INTO bug VALUES(10);
INSERT INTO bug VALUES(20);
INSERT INTO bug VALUES(30);

--echo # Create the test user
CREATE USER test@localhost;

--echo # Grant only SELECT privilege on the test database
GRANT SELECT ON bug_test.* TO test@localhost;
SHOW GRANTS FOR test@localhost;

--echo # Create the dump file
let $mysqldumpfile = $MYSQLTEST_VARDIR/tmp/bug35020512_dump.sql;

--echo # Start the dump tests
--echo # ----- Test 1 -----
--echo # GTID enabled on the server, --set-gtid-purged is set to it's default value (AUTO).
--echo # Should fail because of insufficient privileges.
--echo # Start the dump
--error 2
--exec $MYSQL_DUMP bug_test -utest --single-transaction --no-tablespaces> $mysqldumpfile
--echo # ----- Test 1 succeeded -----

--echo # ----- Test 2 -----
--echo # GTID enabled on the server, --set-gtid-purged is set to OFF
--echo # Should succeed
--exec $MYSQL_DUMP bug_test -utest --single-transaction --set-gtid-purged=off --no-tablespaces > $mysqldumpfile
--echo # ----- Test 2 succeeded -----

--echo # ----- Test 3 -----
--echo # Increase the privileges of test user by granting RELOAD privilege
GRANT RELOAD ON *.* TO test@localhost;
SHOW GRANTS FOR test@localhost;
--echo # Start the dump with --set-gtid-purged=ON
--echo # Should succeed
--exec $MYSQL_DUMP bug_test -utest --single-transaction --set-gtid-purged=ON --no-tablespaces > $mysqldumpfile
--echo # ----- Test 3 succeeded -----

--echo # ----- End of tests -----

# Cleanup
DROP USER test@localhost;
DROP DATABASE bug_test;
RESET BINARY LOGS AND GTIDS;
--remove_file $mysqldumpfile
