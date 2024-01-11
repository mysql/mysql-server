#
# RENAME-related tests which require debug sync.
#

--source include/have_debug_sync.inc

--source include/force_myisam_default.inc
--source include/have_myisam.inc

--echo #
--echo # Bug#24571427 MYSQLDUMP & MYSQLPUMP MAY FAIL WHEN
--echo #              DDL STATEMENTS ARE RUNNING
--echo #

--disable_warnings
SET DEBUG_SYNC= 'RESET';
DROP SCHEMA IF EXISTS test_i_s;
--enable_warnings

CREATE SCHEMA test_i_s;
USE test_i_s;
CREATE TABLE t1(a INT) ENGINE=MyISAM;

--connection default
SET DEBUG_SYNC='alter_table_before_rename_result_table SIGNAL blocked WAIT_FOR i_s_select';

--echo # Sending ALTER Command
--send ALTER TABLE t1 modify column a varchar(30);

connect (con_mysqldump,localhost,root,,);

--echo # Wait until ALTER stopped before renaming the temporary file it created.
SET DEBUG_SYNC= 'now WAIT_FOR blocked';

--echo # Verify that #sql... tables are not seen by I_S and SHOW
SELECT COUNT(TABLE_NAME) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='test_i_s' AND TABLE_NAME like '#sql%';
SHOW TABLES FROM test_i_s;

--echo # Check that there exists a hidden table created by ALTER and
--echo # I_S.TABLES had not listed above. The SHOW EXTENDED syntax was
--echo # added as part of Bug#24786075.
--replace_regex /#sql.*$/#sql-xxxxx/
SHOW EXTENDED TABLES FROM test_i_s;

--echo # Make sure mysqldump ignores the #sql... tables.
--echo # mysqldump fails to acquire the lock without the fix.
--let $file = $MYSQLTEST_VARDIR/tmp/bug24571427.sql
--exec $MYSQL_DUMP --no-tablespaces --databases test_i_s --ignore-table=test_i_s.t1 > $file
--remove_file $file

--echo # Allow ALTER to continue.
SET DEBUG_SYNC= 'now SIGNAL i_s_select';

--disconnect con_mysqldump
--source include/wait_until_disconnected.inc

--connection default
--echo # Reaping "ALTER ..."
--reap

--echo # Verify if the ALTERed TABLE is not-hidden
SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='test_i_s';

--echo # Verify if  inplace alter also keeps the table not-hidden.
ALTER TABLE t1 add column (c2 int);
SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='test_i_s';

--connection default
SET DEBUG_SYNC= 'RESET';
SET GLOBAL DEBUG='';
DROP SCHEMA test_i_s;
