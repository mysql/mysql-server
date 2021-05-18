# Parameters to set before including this file.

# $privilege_check = true/false
#
# $database_encryption = 'n' or 'y'
#
# $explicit_encryption_clause = true/false
# $table_encryption = 'n' or 'y'
#
# $use_tablespace = true/false
# $tablespace_name = 'ts1' or predefined names.
#
# $expected_error = ER_*
# $expected_error2 = ER_*

--let caseno=`SELECT $caseno+1`
--echo # [CREATE TABLE] Case $caseno )
--echo `````````````````````````````````````````````````````````

if ($use_tablespace == 'true')
{
  if ($tablespace_name == 'ts1')
  {
    eval CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION=$table_encryption;
  }
}
eval CREATE DATABASE db1 DEFAULT ENCRYPTION=$database_encryption;

eval SET GLOBAL table_encryption_privilege_check=$privilege_check;
if ($use_tablespace == 'true')
{
  if ($explicit_encryption_clause != 'true')
  {
--let create_ddl=CREATE TABLE db1.t1 (f1 int) TABLESPACE=$tablespace_name
  }
  if ($explicit_encryption_clause == 'true')
  {
--let create_ddl=CREATE TABLE db1.t1 (f1 int) TABLESPACE=$tablespace_name ENCRYPTION=$table_encryption
  }
}
if ($use_tablespace != 'true')
{
  if ($explicit_encryption_clause != 'true')
  {
--let create_ddl=CREATE TABLE db1.t1 (f1 int)
  }
  if ($explicit_encryption_clause == 'true')
  {
--let create_ddl=CREATE TABLE db1.t1 (f1 int) ENCRYPTION=$table_encryption
  }
}

--error $expected_error
eval $create_ddl;

if ($expected_error == '0')
{
SHOW CREATE TABLE db1.t1;
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
  WHERE TABLE_NAME='t1';
DROP TABLE db1.t1;
}

if ($privilege_check == 'true')
{
if ($expected_error == 'ER_CANNOT_SET_TABLE_ENCRYPTION')
{
connection default;
--echo # GRANT user the TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
connection con1;

# The create table would still fail if the encryption clause does not
# match the tablespace encryption type.
--error $expected_error2
eval $create_ddl;

if ($expected_error == '0')
{
SHOW CREATE TABLE db1.t1;
SELECT TABLE_NAME, CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
  WHERE TABLE_NAME='t1';
DROP TABLE db1.t1;
}

connection default;
--echo # REVOKE TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
connection con1;
}
}

# Clean up
DROP DATABASE db1;
if ($use_tablespace == 'true')
{
  if ($tablespace_name == 'ts1')
  {
    DROP TABLESPACE ts1;
  }
}
SET GLOBAL table_encryption_privilege_check=false;
