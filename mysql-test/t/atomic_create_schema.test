call mtr.add_suppression("Attempting backtrace. You can use the following information to find out");
call mtr.add_suppression("Assertion `0' failed");

--echo # Points to verify in this test
--echo # scenario-1. if crash happens just after the schema directory is created, recovery should delete the directory.
--echo # scenario-2. if crash happens happens after we insert ddl log but before the directory is created,
--echo #             we'll have ddl log committed but no directory yet. recovery should go-through in this case.
--echo #

--source include/have_debug.inc

# to make server abort after the schema directory is created
SET DEBUG = "+d,MAKE_SERVER_ABORT_AFTER_SCHEMA_DIR";

--source include/expect_crash.inc
--error 0,2002,2013
CREATE SCHEMA DB1; 
--source include/wait_until_disconnected.inc

# restart
--source include/start_mysqld.inc
--source include/wait_until_connected_again.inc

#deactivate the debug point
SET DEBUG = "-d,MAKE_SERVER_ABORT_AFTER_SCHEMA_DIR";

# verify if the database really got deleted.
CREATE SCHEMA DB1; 
DROP SCHEMA DB1;

SET DEBUG = "+d,MAKE_SERVER_ABORT_BEFORE_SCHEMA_DIR";

--source include/expect_crash.inc
--error 0,2002,2013
CREATE SCHEMA DB1;
--source include/wait_until_disconnected.inc

# restart
--source include/start_mysqld.inc
--source include/wait_until_connected_again.inc

SET DEBUG = "-d,MAKE_SERVER_ABORT_BEFORE_SCHEMA_DIR";
CREATE SCHEMA DB1;
DROP SCHEMA DB1;