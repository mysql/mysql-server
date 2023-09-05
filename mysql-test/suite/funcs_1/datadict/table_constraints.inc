# suite/funcs_1/datadict/table_constraints.inc
#
# Auxiliary script to be sourced by
#    suite/funcs_1/t/is_table_constraints_mysql.test
#    suite/funcs_1/t/is_table_constraints_is.test
#
# The variable
#    $table_schema    database to be inspected
# has to be set before sourcing this script.
#
# Author:
# 2008-01-23 mleich WL#4203 Reorganize and fix the data dictionary tests of
#                           testsuite funcs_1
#                   Create this script based on older scripts and new code.
#

--disable_warnings
DROP DATABASE IF EXISTS db_datadict;
--enable_warnings
CREATE DATABASE db_datadict;

# Create a low privileged user.
# Note: The database db_datadict is just a "home" for the low privileged user
#       and not in the focus of testing.
--error 0,ER_CANNOT_USER
DROP   USER testuser1@localhost;
CREATE USER testuser1@localhost;
GRANT SELECT ON db_datadict.* TO testuser1@localhost;

let $my_select = SELECT * FROM information_schema.table_constraints
WHERE table_schema = '$table_schema' AND table_name != 'ndb_binlog_index'
ORDER BY table_schema,table_name,constraint_name COLLATE utf8mb3_general_ci;
eval $my_select;

--echo # Establish connection testuser1 (user=testuser1)
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (testuser1,localhost,testuser1,,db_datadict);
eval $my_select;

--echo # Switch to connection default and close connection testuser1
connection default;
disconnect testuser1;
DROP USER testuser1@localhost;
DROP DATABASE db_datadict;

