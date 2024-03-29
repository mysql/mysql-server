# Parameter to set before including this file.

# $privilege_check = true/false
# $global_database_encryption_default = true/false
# $tablespace_encryption = 'n' or 'y'
# $tablespace_encryption_to = 'n' or 'y'
# $database_encryption = 'n' or 'y'
# $table_encryption_clause = 'n' or 'y'
# $alter_tablespace_error = ER_*

--let caseno=`SELECT $caseno+1`
--echo # [ALTER TABLESPACE] Case $caseno )
--echo `````````````````````````````````````````````````````````

if ($has_grant == 'true')
{
connection default;
--echo # Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
connection con1;
}

--echo # Create required schema to run ALTER TABLESPACE.
eval CREATE TABLESPACE ts1 ADD DATAFILE 'df_u.ibd' ENCRYPTION=$tablespace_encryption;
eval CREATE DATABASE db1 DEFAULT ENCRYPTION=$database_encryption;
if ($table1_encryption_clause == 'no')
{
eval CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1;
}
if ($table1_encryption_clause != 'no')
{
eval CREATE TABLE db1.t1 (f1 int) TABLESPACE=ts1 ENCRYPTION=$tablespace_encryption;
}
if ($table2_encryption_clause == 'no')
{
eval CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1;
}
if ($table2_encryption_clause != 'no')
{
eval CREATE TABLE db1.t2 (f1 int) TABLESPACE=ts1 ENCRYPTION=$tablespace_encryption;
}
SHOW CREATE TABLE db1.t1;
SHOW CREATE TABLE db1.t2;

# We assume default_table_encryption same desired tablespace encryption
# type, because our motive here is allow alter tablespace to succeed and
# observe what happens to ENCRYPTION clause of tables. The test
# encryption.tablespaces covers such test cases.
if ($tablespace_encryption_to == "'n'")
{
eval SET SESSION default_table_encryption=false;
}
if ($tablespace_encryption_to == "'y'")
{
eval SET SESSION default_table_encryption=true;
}

--echo # Run ALTER TABLESPACE
eval SET GLOBAL table_encryption_privilege_check=$privilege_check;
--error $alter_tablespace_error
eval ALTER TABLESPACE ts1 ENCRYPTION=$tablespace_encryption_to;
SHOW WARNINGS;
SHOW CREATE TABLE db1.t1;
SHOW CREATE TABLE db1.t2;
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;

--echo # clean up
if ($has_grant == 'true')
{
connection default;
--echo # Revoke TABLE_ENCRYPTION_ADMIN from user
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
connection con1;
}
DROP DATABASE db1;
DROP TABLESPACE ts1;
--echo #
