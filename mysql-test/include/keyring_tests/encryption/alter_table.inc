# This test does four operations, while changing the encryption type:
#
# - Moving table using file-per-table to another file-per-table.
# - Moving table using file-per-table to general tablespace.
# - Moving table using general tablespace to file-per-table.
# - Moving table using general tablespace to another general tablespace.
#
# Error expected by corresponding ALTER TABLE:
# $expected_error1 = ER_*
# $expected_error2 = ER_*
# $expected_error3 = ER_*
# $expected_error4 = ER_*
#
# Parameter to set before including this file.
#
# $privilege_check = true/false - Setting for 'table_encryption_privilege_check'
# $database_encryption = 'n' or 'y' - Default encryption type of database.
# $table_encryption = 'n' or 'y' - Encryption type of Table used by source table.
# $alter_tablespace_name = 'ts1' or predefined names.
# $alter_encryption = 'n' or 'y' - Encryption type of Tablespace used in ALTER TABLE.
# $has_grant = true/false - If User owns TABLE_ENCRYPTION_ADMIN
#

--let caseno=`SELECT $caseno+1`
--echo # [ALTER TABLE] Case $caseno )
--echo `````````````````````````````````````````````````````````

eval CREATE DATABASE db1 DEFAULT ENCRYPTION=$database_encryption;
eval CREATE TABLESPACE tsA ADD DATAFILE 'ts_a.ibd' ENCRYPTION=$table_encryption;
if ($alter_tablespace_name == 'ts1')
{
eval CREATE TABLESPACE $alter_tablespace_name ADD DATAFILE 'ts1.ibd' ENCRYPTION=$alter_encryption;
}
eval CREATE TABLE db1.t1 (f1 int) ENCRYPTION=$table_encryption;
eval CREATE TABLE db1.t2 (f1 int) ENCRYPTION=$table_encryption;
eval CREATE TABLE db1.t3 (f1 int) TABLESPACE=tsA ENCRYPTION=$table_encryption;
eval CREATE TABLE db1.t4 (f1 int) TABLESPACE=tsA ENCRYPTION=$table_encryption;

if ($has_grant == 'true')
{
connection default;
--echo # Grant TABLE_ENCRYPTION_ADMIN if requested.
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
connection con1;
}

eval SET GLOBAL table_encryption_privilege_check=$privilege_check;
--echo # Run ALTER TABLE and check for errors/warnings
--error $expected_error1
eval ALTER TABLE db1.t1 TABLESPACE=innodb_file_per_table ENCRYPTION=$alter_encryption;
--error $expected_error2
eval ALTER TABLE db1.t2 TABLESPACE=$alter_tablespace_name ENCRYPTION=$alter_encryption;
--error $expected_error3
eval ALTER TABLE db1.t3 TABLESPACE=innodb_file_per_table ENCRYPTION=$alter_encryption;
--error $expected_error4
eval ALTER TABLE db1.t4 TABLESPACE=$alter_tablespace_name ENCRYPTION=$alter_encryption;
eval SET GLOBAL table_encryption_privilege_check=false;

--echo # Verify the ENCRYPTION clause value.
SHOW CREATE TABLE db1.t1;
SHOW CREATE TABLE db1.t2;
SHOW CREATE TABLE db1.t3;
SHOW CREATE TABLE db1.t4;
--sorted_result
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
  WHERE TABLE_SCHEMA='db1';

--echo # Cleanup
if ($has_grant == 'true')
{
connection default;
--echo # REVOKE TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
connection con1;
}
DROP DATABASE db1;
DROP TABLESPACE tsA;
if ($alter_tablespace_name == 'ts1')
{
eval DROP TABLESPACE $alter_tablespace_name;
}
--let expected_error1=0
--let expected_error2=0
--let expected_error3=0
--let expected_error4=0
