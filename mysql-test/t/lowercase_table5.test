# 
# Get deafult engine value
#
--let $DEFAULT_ENGINE = `SELECT @@global.default_storage_engine`

#
# Test of force of lower-case-table-names=0 on
# a case sensitive file system.
#

#Server variable option 'lower_case_table_names' sets '0' as default value
#in case sensitive filesystem. Using 'lower_case_table_names=0' in case of
#insensitive filsystem is not allowed.
--source include/have_lowercase0.inc
--source include/have_case_sensitive_file_system.inc

--echo #
--echo # BUG#18923685: PROPERLY INITIALIZE DB OPTION HASH TABLE
--echo #

CREATE DATABASE FoO COLLATE ascii_bin;
CREATE DATABASE Foo COLLATE utf8mb3_unicode_ci;

USE FoO;
CREATE TABLE t1(a INT);
--echo # t1 should inherit ascii_bin
--replace_result $DEFAULT_ENGINE ENGINE
SHOW CREATE TABLE t1;
DROP TABLE t1;

USE Foo;
CREATE TABLE t1(a INT); 
--echo # t1 should inherit utf8_unicode_ci
--replace_result $DEFAULT_ENGINE ENGINE
SHOW CREATE TABLE t1;
DROP TABLE t1;

DROP DATABASE FoO;
DROP DATABASE Foo;

USE test;

--echo # End of 5.6 tests
