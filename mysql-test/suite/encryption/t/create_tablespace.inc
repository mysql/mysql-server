# Parameter to set before including this file.

# $privilege_check = 'true' or 'false'
# $global_database_encryption_default = 'true' or 'false'
# $explicit_encryption_clause = 'true' or 'false'
# $tablespace_encryption = 'y' or 'n'
# $expected_error = ER_*

--let caseno=`SELECT $caseno+1`
--echo # [CREATE TABLESPACE] Case $caseno )
--echo `````````````````````````````````````````````````````````

eval SET GLOBAL table_encryption_privilege_check=$privilege_check;
eval SET SESSION default_table_encryption=$global_database_encryption_default;

if ($explicit_encryption_clause == 'true')
{
--let create_ddl=CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd' ENCRYPTION=$tablespace_encryption;
}
if ($explicit_encryption_clause != 'true')
{
--let create_ddl=CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd';
}

--error $expected_error
eval $create_ddl
SHOW WARNINGS;
if ($expected_error == '0')
{
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
  WHERE NAME LIKE 'ts1';
DROP TABLESPACE ts1;
}

if ($privilege_check == 'true')
{
--echo # Check that we never fail with TABLE_ENCRYPTION_ADMIN
connection default;
--echo # Grant user with TABLE_ENCRYPTION_ADMIN
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
connection con1;
--error 0
eval $create_ddl
SHOW WARNINGS;
SELECT NAME, ENCRYPTION FROM INFORMATION_SCHEMA.INNODB_TABLESPACES
  WHERE NAME LIKE 'ts1';
DROP TABLESPACE ts1;
connection default;
--echo # Revoke TABLE_ENCRYPTION_ADMIN from user.
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
connection con1;
}

# Clean up
SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
