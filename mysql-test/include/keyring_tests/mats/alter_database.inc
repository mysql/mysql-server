# Parameter to set before including this file.

# $privilege_check = 'true' or 'false'
# $global_database_encryption_default = 'true' or 'false'
# $database_encryption = 'y' or 'n'
# $alter_encryption = 'y' or 'n'
# $expected_error = ER_*

--let caseno=`SELECT $caseno+1`
--echo # [ALTER DATABASE] Case $caseno )
--echo `````````````````````````````````````````````````````````

eval CREATE DATABASE db1 DEFAULT ENCRYPTION=$database_encryption;
SHOW CREATE DATABASE db1;

if ($has_grant == 'true')
{
--echo # GRANT TABLE_ENCRYPTION_ADMIN
connection default;
GRANT TABLE_ENCRYPTION_ADMIN ON *.* TO u1@localhost;
connection con1;
}

eval SET SESSION default_table_encryption=$global_database_encryption_default;
eval SET GLOBAL table_encryption_privilege_check=$privilege_check;
--error $expected_error
eval ALTER DATABASE db1 DEFAULT ENCRYPTION=$alter_encryption;
SHOW WARNINGS;
SHOW CREATE DATABASE db1;
SELECT SCHEMA_NAME, DEFAULT_ENCRYPTION
  FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME='db1';
DROP DATABASE db1;

if ($has_grant == 'true')
{
--echo # REVOKE TABLE_ENCRYPTION_ADMIN
connection default;
REVOKE TABLE_ENCRYPTION_ADMIN ON *.* FROM u1@localhost;
connection con1;
}


eval SET GLOBAL table_encryption_privilege_check=false;
eval SET SESSION default_table_encryption=false;
