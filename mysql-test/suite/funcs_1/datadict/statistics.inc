# suite/funcs_1/datadict/statistics.inc
#
# Auxiliary script to be sourced by
#    is_statistics_is
#    is_statistics_mysql
#    is_statistics_<engine>
#
# Purpose:
#    Check the content of information_schema.statistics about tables within the
#    database '$database'.
#
# Usage:
#    The variable $database has to be set before sourcing this script.
#    Example:
#       let $database = db_data;
#
# Author:
# 2008-01-23 mleich WL#4203 Reorganize and fix the data dictionary tests of
#                           testsuite funcs_1
#                   Create this script based on older scripts and new code.
#

--source suite/funcs_1/datadict/datadict.pre

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

let $my_select = SELECT * FROM information_schema.statistics
$my_where
ORDER BY table_schema, table_name, index_name, seq_in_index, column_name;
--replace_column 10 #CARD#
eval $my_select;

--echo # Establish connection testuser1 (user=testuser1)
--replace_result $MASTER_MYPORT MYSQL_PORT $MASTER_MYSOCK MYSQL_SOCK
connect (testuser1,localhost,testuser1,,db_datadict);
--replace_column 10 #CARD#
eval $my_select;

--echo # Switch to connection default and close connection testuser1
connection default;
disconnect testuser1;
DROP USER testuser1@localhost;
DROP DATABASE db_datadict;

