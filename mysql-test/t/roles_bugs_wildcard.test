--source include/not_partial_revokes.inc
# Save the initial number of concurrent sessions
--source include/count_sessions.inc

--echo #
--echo # Bug#31013538 ALL PRIVILEGES ON A DATABASE FOR
--echo #              ROLE DOESN'T ALLOW USER TO CREATE
--echo #

CREATE USER u1;
CREATE ROLE r1;
GRANT CREATE, DROP ON db_name.* TO r1;
GRANT r1 TO u1;

--connect(conn_u1, localhost, u1,,,,)
--echo # Grants without role activation
SHOW GRANTS;
# Activate r1
SET ROLE r1;
--echo # Grants after role activation: Must show privileges from r1
SHOW GRANTS;

CREATE DATABASE db_name;
DROP DATABASE db_name;

CREATE DATABASE db1name;
DROP DATABASE db1name;

--connection default
--disconnect conn_u1
DROP ROLE r1;
DROP USER u1;

--echo #
--echo # Bug #35338567: Role with backslash in grant does not allow proper
--echo #                SHOW DATABASES
--echo #

CREATE DATABASE db_name;
CREATE USER testuser;
CREATE ROLE abc_all_ro_role;

GRANT ALL ON test.* to testuser;
GRANT SELECT ON `db\_name`.* to abc_all_ro_role;
GRANT abc_all_ro_role TO testuser;
SET DEFAULT ROLE ALL TO testuser;

connect(con1,localhost,testuser,,test);

SHOW DATABASES;
USE db_name;

--echo # Cleanup
connection default;
disconnect con1;
DROP ROLE abc_all_ro_role;
DROP USER testuser;
DROP DATABASE db_name;

--echo
--echo # End of 8.0 tests
--echo

# Wait till we reached the initial number of concurrent sessions
--source include/wait_until_count_sessions.inc
