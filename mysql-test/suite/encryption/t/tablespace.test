--echo #
--echo # WL#12261 Control (enforce and disable) table encryption
--echo #
--source include/have_debug.inc

--echo # Pre-define user u1, which is used in different tests below.
CREATE USER u1@localhost;
GRANT ALL ON db1.* TO u1@localhost;
GRANT CREATE TABLESPACE, PROCESS, SYSTEM_VARIABLES_ADMIN ON *.* TO u1@localhost;
SET GLOBAL debug= '+d,skip_table_encryption_admin_check_for_set';
connect (con1, localhost, u1);

--echo # This test run CREATE/ALTER TABLESPACE in different configurations,
--echo #
--echo # - Setting table_encryption_privilege_check to true/false.
--echo # - Setting default_table_encryption to true/false.
--echo # - With and without ENCRYPTION clause.
--echo # - With and without user holding TABLE_ENCRYPTION_ADMIN privilege.
--echo # - Test INFORMATION_SCHEMA.INNODB_TABLESPACES
--echo # - Check for warnings generated.
--echo #

--let expected_error=0
--let caseno=0

--echo `````````````````````````````````````````````````````````
--echo # CREATE TABLESPACE with explicit ENCRYPTION clause
--echo # table_encryption_privilege_check=false
--let explicit_encryption_clause=false
--let privilege_check=false

--let global_database_encryption_default=false
--source ./create_tablespace.inc
--let global_database_encryption_default=true
--source ./create_tablespace.inc

--echo `````````````````````````````````````````````````````````
--echo # table_encryption_privilege_check=true
--let privilege_check=true

--let global_database_encryption_default=false
--source ./create_tablespace.inc
--let global_database_encryption_default=true
--source ./create_tablespace.inc

--echo `````````````````````````````````````````````````````````
--echo # CREATE TABLESPACE with explicit ENCRYPTION clause
--echo # table_encryption_privilege_check=false
--let explicit_encryption_clause=true
--let privilege_check=false

--let global_database_encryption_default=false
--let tablespace_encryption='y'
--source ./create_tablespace.inc
--let tablespace_encryption='n'
--source ./create_tablespace.inc
--let global_database_encryption_default=true
--let tablespace_encryption='y'
--source ./create_tablespace.inc
--let tablespace_encryption='n'
--source ./create_tablespace.inc

--echo `````````````````````````````````````````````````````````
--echo # table_encryption_privilege_check=true
--let privilege_check=true

--let global_database_encryption_default=false
--let expected_error=ER_CANNOT_SET_TABLESPACE_ENCRYPTION
--let tablespace_encryption='y'
--source ./create_tablespace.inc
--let expected_error=0
--let tablespace_encryption='n'
--source ./create_tablespace.inc
--let global_database_encryption_default=true
--let tablespace_encryption='y'
--source ./create_tablespace.inc
--let expected_error=ER_CANNOT_SET_TABLESPACE_ENCRYPTION
--let tablespace_encryption='n'
--source ./create_tablespace.inc

--echo `````````````````````````````````````````````````````````
--echo # Check for invalid ENCRYPTION clause value.
--let privilege_check=false
--let global_database_encryption_default=false
--let expected_error=ER_INVALID_ENCRYPTION_OPTION
--let tablespace_encryption='k'
--source ./create_tablespace.inc

--echo `````````````````````````````````````````````````````````
--echo # The test cases run ALTER TABLESPACE (with no tables)
--echo # See test tablespace_with_tables.test.
--let caseno=0

--echo `````````````````````````````````````````````````````````
--echo # Unencrypted TABLESPACE to Unencrypted TABLESPACE (Nop)
--echo #
--let global_database_encryption_default=false
--let tablespace_encryption='n'
--let alter_encryption='n'

--let privilege_check=false
--let expected_error=0
--source ./alter_empty_tablespace.inc

--let privilege_check=true
--let expected_error=0
--source ./alter_empty_tablespace.inc

--echo `````````````````````````````````````````````````````````
--echo # Encrypted TABLESPACE to Encrypted TABLESPACE (Nop)
--echo #
--let global_database_encryption_default=true
--let tablespace_encryption='y'
--let alter_encryption='y'

--let privilege_check=false
--let expected_error=0
--source ./alter_empty_tablespace.inc

--let privilege_check=true
--let expected_error=0
--source ./alter_empty_tablespace.inc

--echo `````````````````````````````````````````````````````````
--echo # Unencrypted TABLESPACE to encrypted TABLESPACE
--echo # with database encryption default 'n'
--echo #
--let global_database_encryption_default=false
--let tablespace_encryption='n'
--let alter_encryption='y'

--let privilege_check=false
--let expected_error=0
--source ./alter_empty_tablespace.inc

--let privilege_check=true
--let expected_error=ER_CANNOT_SET_TABLESPACE_ENCRYPTION
--source ./alter_empty_tablespace.inc

--echo `````````````````````````````````````````````````````````
--echo # Unencrypted TABLESPACE to encrypted TABLESPACE
--echo # with database encryption default 'y'
--echo #
--let global_database_encryption_default=true
--let tablespace_encryption='n'
--let alter_encryption='y'

--let privilege_check=false
--let expected_error=0
--source ./alter_empty_tablespace.inc

--let privilege_check=true
--let expected_error=0
--source ./alter_empty_tablespace.inc

--echo `````````````````````````````````````````````````````````
--echo # Encrypted TABLESPACE to unencrypted TABLESPACE
--echo # with database encryption default 'n'
--echo #
--let global_database_encryption_default=false
--let tablespace_encryption='y'
--let alter_encryption='n'

--let privilege_check=false
--let expected_error=0
--source ./alter_empty_tablespace.inc

--let privilege_check=true
--let expected_error=0
--source ./alter_empty_tablespace.inc

--echo `````````````````````````````````````````````````````````
--echo # Encrypted TABLESPACE to unencrypted TABLESPACE
--echo # with database encryption default 'y'
--echo #
--let global_database_encryption_default=true
--let tablespace_encryption='y'
--let alter_encryption='n'

--let privilege_check=false
--let expected_error=0
--source ./alter_empty_tablespace.inc

--let privilege_check=true
--let expected_error=ER_CANNOT_SET_TABLESPACE_ENCRYPTION
--source ./alter_empty_tablespace.inc

--echo `````````````````````````````````````````````````````````
--echo # Check for Invalid encryption option.
--let privilege_check=false
--let global_database_encryption_default=false
--let expected_error=ER_INVALID_ENCRYPTION_OPTION
--let alter_encryption='k'
--source ./alter_empty_tablespace.inc

--echo # Cleanup
disconnect con1;
connection default;
DROP USER u1@localhost;
SET GLOBAL debug= '-d,skip_table_encryption_admin_check_for_set';
