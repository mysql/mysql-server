# suite/funcs_1/datadict/tables.inc
#
# Auxiliary script to be sourced by
#    is_tables_<engine>.test
#
# The variable $engine_type has to be assigned before sourcing ths script.
#
# Author:
# 2008-06-04 mleich Create this script based on older scripts and new code.
#

# Just have some tables within different databases.
--disable_warnings
DROP DATABASE IF EXISTS test1;
DROP DATABASE IF EXISTS test2;
--enable_warnings
CREATE DATABASE test1 charset latin1;
CREATE DATABASE test2 charset latin1;

--replace_result $engine_type <engine_to_be_used>
eval CREATE TABLE test1.t1  (f1 VARCHAR(20)) ENGINE = $engine_type;
--replace_result $engine_type <engine_to_be_used>
eval CREATE TABLE test1.t2  (f1 VARCHAR(20)) ENGINE = $engine_type;
--replace_result $engine_type <engine_to_be_used>
eval CREATE TABLE test2.t1 (f1 VARCHAR(20)) ENGINE = $engine_type;

--source suite/funcs_1/datadict/tables2.inc
SHOW TABLES FROM test1;
SHOW TABLES FROM test2;

# Create a low privileged user.
# Note: The database db_datadict is just a "home" for the low privileged user
#       and not in the focus of testing.
--error 0,ER_CANNOT_USER
DROP   USER testuser1@localhost;
CREATE USER testuser1@localhost;
GRANT SELECT ON test1.* TO testuser1@localhost;

--echo # Establish connection testuser1 (user=testuser1)
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (testuser1,localhost,testuser1,,test1);
--source suite/funcs_1/datadict/tables2.inc
SHOW TABLES FROM test1;
# The lowprivileged user testuser1 will get here an error.
--disable_abort_on_error
SHOW TABLES FROM test2;
--enable_abort_on_error

--echo # Switch to connection default and close connection testuser1
connection default;
disconnect testuser1;
DROP USER testuser1@localhost;

DROP DATABASE test1;
DROP DATABASE test2;
