# Parameter to set before including this file.

# $privilege_check = true/false
# $database_encryption = 'n' or 'y'
# $table_encryption = 'n' or 'y'
# $alter_tablespace_name = 'ts1' or predefined names.
# $alter_encryption = 'n' or 'y'
#
# $expected_error1 = ER_*
# $expected_error2 = ER_*
# $expected_error3 = ER_*
# $expected_error4 = ER_*

--let caseno=`SELECT $caseno+1`
--echo # [ALTER TABLE ... RENAME ...] Case $caseno )
--echo `````````````````````````````````````````````````````````

eval CREATE DATABASE db1 DEFAULT ENCRYPTION='y';
eval CREATE DATABASE db2 DEFAULT ENCRYPTION='n';

eval CREATE DATABASE sch1 DEFAULT ENCRYPTION='y';
eval CREATE DATABASE sch2 DEFAULT ENCRYPTION='n';

if ($alter_tablespace_name == 'ts1')
{
eval CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION=$table_encryption;
}

eval CREATE TABLE db1.fpt1 (f1 int) ENCRYPTION=$table_encryption;
eval CREATE TABLE db1.fpt2 (f1 int) ENCRYPTION=$table_encryption;
eval CREATE TABLE db1.ts1  (f1 int) TABLESPACE=$alter_tablespace_name ENCRYPTION=$table_encryption;
eval CREATE TABLE db1.ts2  (f1 int) TABLESPACE=$alter_tablespace_name ENCRYPTION=$table_encryption;
eval CREATE TABLE db2.fpt1 (f1 int) ENCRYPTION=$table_encryption;
eval CREATE TABLE db2.fpt2 (f1 int) ENCRYPTION=$table_encryption;
eval CREATE TABLE db2.ts1  (f1 int) TABLESPACE=$alter_tablespace_name ENCRYPTION=$table_encryption;
eval CREATE TABLE db2.ts2  (f1 int) TABLESPACE=$alter_tablespace_name ENCRYPTION=$table_encryption;

eval CREATE TABLE sch1.fpt (f1 int) ENCRYPTION=$table_encryption;
eval CREATE TABLE sch1.ts  (f1 int) TABLESPACE=$alter_tablespace_name ENCRYPTION=$table_encryption;
eval CREATE TABLE sch2.fpt (f1 int) ENCRYPTION=$table_encryption;
eval CREATE TABLE sch2.ts  (f1 int) TABLESPACE=$alter_tablespace_name ENCRYPTION=$table_encryption;

eval SET GLOBAL table_encryption_privilege_check=$privilege_check;

if ($has_grant == 'true')
{
connection default;
--echo # Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
connection con1;
}

--let clause=
if ($algo == 'copy')
{
--let clause=,MODIFY f1 CHAR(30)
}
if ($algo == 'inplace')
{
--let clause=,ADD f2 CHAR(30)
}

--echo # $caseno.1.
--echo # Rename table using file-per-table tablespace from db encryption='y'
--echo # to db with encryption='n' and viceversa
--error $expected_error1
eval ALTER TABLE db1.fpt1 RENAME db2.fpt1_renamed $clause;
--error $expected_error2
eval ALTER TABLE db2.fpt1 RENAME db1.fpt1_renamed $clause;

--echo # $caseno.2.
--echo # Rename table using general tablespace from db encryption='y'
--echo # to db with encryption='n' and viceversa
--error $expected_error3
eval ALTER TABLE db1.ts1 RENAME db2.ts1_rename $clause;
--error $expected_error4
eval ALTER TABLE db2.ts1 RENAME db1.ts1_rename $clause;

--echo # $caseno.3.
--echo # Rename table (using file-per-table/general tablespace)
--echo # within the database never fails.
eval ALTER TABLE sch1.fpt  RENAME sch1.fpt_renamed $clause;
eval ALTER TABLE sch1.ts   RENAME sch1.ts_renamed  $clause;
eval ALTER TABLE sch2.fpt  RENAME sch2.fpt_renamed $clause;
eval ALTER TABLE sch2.ts   RENAME sch2.ts_renamed  $clause;

eval SET GLOBAL table_encryption_privilege_check=false;
SHOW TABLES IN db1;
SHOW TABLES IN db2;
SHOW TABLES IN sch1;
SHOW TABLES IN sch2;

if ($has_grant == 'true')
{
connection default;
--echo # Revoke TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
connection con1;
}

# clean up
DROP DATABASE db1;
DROP DATABASE db2;
DROP DATABASE sch1;
DROP DATABASE sch2;

if ($alter_tablespace_name == 'ts1')
{
eval DROP TABLESPACE $alter_tablespace_name;
}

--let expected_error1=0
--let expected_error2=0
--let expected_error3=0
--let expected_error4=0
