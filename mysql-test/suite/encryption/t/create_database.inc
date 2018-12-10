# Parameter to set before including this file.

# $privilege_check = 'true' or 'false'
# $global_database_encryption_default = 'true' or 'false'
# $database_encryption = 'y' or 'n'
# $has_grant = 'y' or 'n'
# $expected_error = ER_*
# $explicit_encryption_clause = 'true' or 'false'

--let caseno=`SELECT $caseno+1`
--echo # [CREATE DATABASE] Case $caseno )
--echo `````````````````````````````````````````````````````````

if ($has_grant == 'true')
{
--echo # Granting TABLE_ENCRYPTION_ADMIN for user.
connection default;
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
connection con1;
}

eval SET GLOBAL table_encryption_privilege_check=$privilege_check;
eval SET SESSION default_table_encryption=$global_database_encryption_default;

--error $expected_error
if ($explicit_encryption_clause == 'true')
{
if ($with_default == 'true')
{
eval CREATE DATABASE db1 DEFAULT ENCRYPTION=$database_encryption;
}
if ($with_default == 'false')
{
eval CREATE DATABASE db1 ENCRYPTION=$database_encryption;
}
}
--error $expected_error
if ($explicit_encryption_clause != 'true')
{
eval CREATE DATABASE db1;
}
SHOW WARNINGS;
if ($expected_error == '0')
{
SHOW CREATE DATABASE db1;
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
  FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
DROP DATABASE db1;
}

if ($has_grant == 'true')
{
--echo # Revoke TABLE_ENCRYPTION_ADMIN
connection default;
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
connection con1;
}

SET GLOBAL table_encryption_privilege_check=false;
SET SESSION default_table_encryption=false;
