###################################################################
# This test file includes scenarios related to renaming of roles  #
###################################################################

--echo +++++++++++++++++++++++++++++++++++++++++++++++
--echo + Renaming users shouldn't crash the server
--echo +++++++++++++++++++++++++++++++++++++++++++++++
CREATE USER u1, r1;
GRANT r1 TO u1;
RENAME USER u1 TO u11;
--error ER_UNKNOWN_AUTHID
ALTER USER u1 DEFAULT ROLE ALL;
--error ER_UNKNOWN_AUTHID
ALTER USER anything DEFAULT ROLE ALL;
ALTER USER u11 DEFAULT ROLE ALL;

--echo ++ Cleanup
DROP USER u11, r1;

--echo +++++++++++++++++++++++++++++++++++++++++++++
--echo + RENAME USER shouldn't break the role graph
--echo +++++++++++++++++++++++++++++++++++++++++++++
CREATE USER u1@localhost IDENTIFIED BY 'foo';
CREATE USER u3@localhost;
CREATE ROLE r1;
CREATE ROLE r2;
GRANT r1 TO u1@localhost;
GRANT r2 TO u1@localhost WITH ADMIN OPTION;
CREATE DATABASE db1;
CREATE TABLE db1.t1 (c1 INT);
GRANT SELECT ON db1.t1 TO r1;
GRANT INSERT ON *.* TO r2;
ALTER USER u1@localhost DEFAULT ROLE r1,r2;
--echo -------------------------------------
--let $user = 0
--source include/show_grants.inc
--echo -------------------------------------
SHOW GRANTS FOR u1@localhost USING r1;
--echo -------------------------------------
--echo # Role should not be allowed to rename.
--error ER_RENAME_ROLE
RENAME USER r1 TO r2;
RENAME USER u1@localhost TO u2@localhost, u3@localhost TO u1@localhost;
SELECT * FROM mysql.default_roles ORDER BY default_role_user;
--echo # Check the current role of the AuthID which is granted default roles.
connect(con1, localhost, u2, foo, test);
SELECT CURRENT_ROLE();
SET ROLE NONE;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db1.t1;
SET ROLE r1;
SELECT * FROM db1.t1;
SELECT CURRENT_USER(), CURRENT_ROLE();
--echo -------------------------------
SHOW GRANTS;
--echo -------------------------------
SHOW GRANTS FOR u2@localhost USING r1;
--echo -------------------------------
connection default;
disconnect con1;

--echo ++ Cleanup
DROP ROLE r1;
DROP ROLE r2;
DROP USER u2@localhost;
DROP USER u1@localhost;
DROP DATABASE db1;

--echo ++++++++++++++++++++++++++++++++++++++++++++++++++++++
--echo + Rename the AuthId after it has been released from
--echo + the role graph post revoking it from the user
--echo ++++++++++++++++++++++++++++++++++++++++++++++++++++++
CREATE USER usr, role_usr;
RENAME USER role_usr to role_usr_test;
GRANT role_usr_test to usr;

--echo # Throw error as role_usr_test is in role graph
--error ER_RENAME_ROLE
RENAME USER role_usr_test to role_usr;

REVOKE role_usr_test from usr;
RENAME USER role_usr_test to role_usr;
DROP USER usr, role_usr;

--echo ++++++++++++++++++++++++++++++++++++++++++++++++++++++
--echo + Rename the AuthId after it has been released from
--echo + the role graph after the user has been dropped.
--echo ++++++++++++++++++++++++++++++++++++++++++++++++++++++

CREATE USER usr, role_usr;
RENAME USER role_usr to role_usr_test;
GRANT role_usr_test to usr;

--echo # Throw error as role_usr_test is in role graph
--error ER_RENAME_ROLE
RENAME USER role_usr_test to role_usr;

--echo ++ Cleanup
DROP USER usr;
RENAME USER role_usr_test to role_usr;
DROP USER role_usr;

--echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++
--echo + Rename the AuthId which is granted some default roles
--echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++
CREATE USER u5;
CREATE ROLE r1,r2,r3;
GRANT ALL ON test.* TO r2;
GRANT r1, r2, r3 TO u5;
ALTER USER u5 DEFAULT ROLE r2,r3;
RENAME USER u5 to u1;
connect(con1,localhost, u1,,);
SELECT current_role();
SET ROLE DEFAULT;
disconnect con1;
connection default;

--echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
--echo + Rename and grant default roles again to the previously granted AuthId
--echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
RENAME USER u1 to u2;
GRANT r3 TO u2;
ALTER USER u2 DEFAULT ROLE r1, r2, r3;
connect(con1,localhost, u2,,);
SELECT CURRENT_ROLE();
SET ROLE DEFAULT;
disconnect con1;
connection default;

--echo ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
--echo + Rename role when it is, in the role graph and, not in the role graph
--echo ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
--error ER_RENAME_ROLE
rename user r2 to r22;
REVOKE r2 FROM u2;
RENAME USER r2 to r22;

--echo ++ Cleanup
DROP ROLE r1,r22, r3;
DROP USER u2;
--error ER_CANNOT_USER
DROP USER u1;

--echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
--echo + Rename authId which was granted inherited roles but the
--echo + authId is not in role graph
--echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
CREATE USER u1;
CREATE ROLE r1,r2,r3;
GRANT r1 TO r2;
GRANT r2 TO r3;
GRANT r3 to u1;
DROP USER u1;
RENAME USER r3 to r33;
--echo ++ Cleanup
--error ER_CANNOT_USER
DROP ROLE r3;
--error ER_CANNOT_USER
DROP USER u1;
DROP ROLE r1, r2, r33;

--echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
--echo + Rename authId which was granted inherited roles but the
--echo + authId is not in role graph
--echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
CREATE USER u1;
CREATE ROLE r1,r2,r3;
GRANT r1 TO r2;
GRANT r2 TO r3;
GRANT r3 to u1;
REVOKE r3 FROM u1;
RENAME USER r3 to r33;
--echo ++ Cleanup
--error ER_CANNOT_USER
DROP ROLE r3;
DROP ROLE r1, r2, r33;
DROP USER u1;
