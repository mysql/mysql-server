CREATE ROLE r1;

CREATE ROLE `admin-db1`;
CREATE ROLE `admin-db2`;
CREATE ROLE `admin-db1t1`;
CREATE ROLE `admin-db2t1`;
CREATE ROLE `app-updater`;

CREATE USER `app-middleware-db1`@`localhost` IDENTIFIED BY 'foo';
CREATE USER `app-middleware-db2`@`localhost` IDENTIFIED BY 'foo';
CREATE USER `app`@`localhost` IDENTIFIED BY 'foo';

GRANT `admin-db1` TO `app-middleware-db1`@`localhost`;
GRANT `admin-db2` TO `app-middleware-db2`@`localhost`;
GRANT `app-updater` TO `app-middleware-db1`@`localhost`;

CREATE DATABASE db1;
CREATE DATABASE db2;

CREATE TABLE db1.t1 (c1 INT, c2 INT, c3 INT);
CREATE TABLE db1.t2 (c1 INT, c2 INT, c3 INT);
CREATE TABLE db2.t1 (c1 INT, c2 INT, c3 INT);
CREATE TABLE db2.t2 (c1 INT, c2 INT, c3 INT);

--echo ++ admin-db1 can manage db2.t1 and admin-db2 can manage db1.t1
GRANT `admin-db2t1` TO `admin-db1`;
GRANT `admin-db1t1` TO `admin-db2`;
--echo ++ admin-db1 can promote anyone with the admin-db1t1 rights.
GRANT `admin-db1t1` TO `admin-db1` WITH ADMIN OPTION;

GRANT SELECT, UPDATE, CREATE, DROP, INSERT, DELETE ON db1.* TO `admin-db1`;
GRANT SELECT, UPDATE, CREATE, DROP, INSERT, DELETE ON db2.* TO `admin-db2`;
GRANT SELECT, UPDATE, CREATE, DROP, INSERT, DELETE ON db1.t1 TO `admin-db1t1`;
GRANT SELECT, UPDATE, CREATE, DROP, INSERT, DELETE ON db2.t1 TO `admin-db2t1`;

connect(con1, localhost, app-middleware-db1, foo, test);
SET ROLE `admin-db1`;
SHOW GRANTS;

--echo ++ Positive test
INSERT INTO db1.t1 VALUES (1,2,3);
INSERT INTO db1.t2 VALUES (1,2,3);
INSERT INTO db2.t1 VALUES (1,2,3);

SELECT * FROM db1.t1;
SELECT * FROM db1.t2;
SELECT * FROM db2.t1;

GRANT `admin-db1t1` TO `app`@`localhost`;

connection default;
GRANT r1 TO `app-middleware-db1`@`localhost` WITH ADMIN OPTION;

connection con1;
--echo ++ Connected as app-middleware-db1
SET ROLE `admin-db1`;
GRANT `admin-db1t1` TO `app`@`localhost`;
--echo ++ r1 and inherited role admin-db1t1 should be WITH ADMIN OPTION
SHOW GRANTS FOR CURRENT_USER() USING `admin-db1`;

--echo ++ Negative test
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO db2.t2 VALUES (1,2,3);
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db2.t2;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
GRANT `admin-db2t1` TO `app`@`localhost`;

connection default;
--echo ++ Connected as root
--echo ++ Granting WITH ADMIN OPTION role WITH ADMIN OPTION privileges
--echo ++ app@localhost has admin-db1t1 granted.
connect(con2, localhost, app, foo, test);
--echo ++ Connected as app@localhost
SHOW GRANTS FOR CURRENT_USER();
--echo ++ Positive test ; setting a granted role.
SET ROLE `admin-db1t1`;
SELECT CURRENT_USER(), CURRENT_ROLE();

--echo ++ Negative tests ; Attempt to grant the granted role to 3rd party
--echo ++ app@localhost did not inherit the ability to grant WITH ADMIN OPTION
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
GRANT `admin-db1t1` TO `app-middleware-db2`@`localhost`;
connection default;
--echo # only count nodes and edges as the sorting order is depending on platform
SELECT ExtractValue(ROLES_GRAPHML(),'count(//node)') as num_nodes;
SELECT ExtractValue(ROLES_GRAPHML(),'count(//edge)') as num_edges;

--echo ++ Now grant admin-db1t1 to app@localhost WITH ADMIN OPTION
--echo ++ Positive test
connection con1;
--echo ++ Connected as app-middleware-db1@localhost
GRANT `admin-db1t1` TO `app`@`localhost` WITH ADMIN OPTION;
connection con2;
--echo ++ Connected as app@localhost
--echo ++ app@localhost should now be able to grant admin-db1t1 to app-middleware
SET ROLE ALL;
SELECT CURRENT_USER(), CURRENT_ROLE();
GRANT `admin-db1t1` TO `app-middleware-db2`@`localhost`;

--echo ++ Revoking roles require WITH ADMIN too
--echo ++ Positive tests
REVOKE `admin-db1t1` FROM `app-middleware-db2`@`localhost`;
--echo ++ Restorning grant for negative test
GRANT `admin-db1t1` TO `app-middleware-db2`@`localhost`;
connection con1;
--echo ++ Connected as app-middleware-db1@localhost
--echo ++ Remove WITH ADMIN grants by removing and re-granting role
REVOKE `admin-db1t1` FROM `app`@`localhost`;
GRANT `admin-db1t1` TO `app`@`localhost`;
connection con2;
--echo ++ Connected as app@localhost
--echo ++ Negative tests
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
REVOKE `admin-db1t1` FROM `app-middleware-db2`@`localhost`;
connection con1;
--echo ++ Connected as app-middleware-db1@localhost
--echo ++ Positive test
SELECT CURRENT_USER(), CURRENT_ROLE();
SHOW GRANTS;
--echo ++ User stil has WITH ADMIN and can revoke from `app-middleware-db2`@`localhost`
REVOKE `admin-db1t1` FROM `app-middleware-db2`@`localhost`;

connection default;
DROP DATABASE db1;
DROP DATABASE db2;
DROP ROLE r1;
DROP ROLE `admin-db1`;
DROP ROLE `admin-db2`;
DROP ROLE `admin-db1t1`;
DROP ROLE `admin-db2t1`;
DROP ROLE `app-updater`;
DROP USER `app-middleware-db1`@`localhost`;
DROP USER `app-middleware-db2`@`localhost`;
DROP USER `app`@`localhost`;
disconnect con1;
disconnect con2;

--echo +++++++++++++++++++++++++++++
--echo ++ WITH GRANT OPTION tests ++
--echo +++++++++++++++++++++++++++++
CREATE USER u1@localhost IDENTIFIED BY 'foo';
CREATE USER u2@localhost IDENTIFIED BY 'foo';
CREATE ROLE r1;
CREATE DATABASE db1;
GRANT CREATE ON db1.* TO r1 WITH GRANT OPTION;
GRANT r1 TO u1@localhost;
connect(con1, localhost, u1, foo, test);
--echo ++ Connected as u1@localhost
SET ROLE ALL;
GRANT CREATE ON db1.* TO u2@localhost;
SET ROLE NONE;
--error ER_DBACCESS_DENIED_ERROR
GRANT CREATE ON db1.* TO u2@localhost;
connection default;
--echo ++ Connected as root
disconnect con1;

DROP USER u1@localhost, u2@localhost;
DROP ROLE r1;
DROP DATABASE db1;
SELECT user,host FROM mysql.user;

--echo #############################################
CREATE USER u1@localhost IDENTIFIED BY 'foo';
CREATE USER u2@localhost IDENTIFIED BY 'foo';
CREATE ROLE r1, r2;
use test;
GRANT CREATE ON test.* TO r1;
GRANT DROP ON test.* TO r2;
GRANT r1 TO u1@localhost WITH ADMIN OPTION;
GRANT r2 TO u1@localhost;
connect(con1, localhost, u1, foo, test);

connection con1;
--echo ++ Connected as u1@localhost
SET ROLE ALL;
SHOW GRANTS;
GRANT r1 TO u2@localhost WITH ADMIN OPTION;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
GRANT r2 TO u2@localhost WITH ADMIN OPTION;
connection default;
--echo ++ Connected as root
--echo #############################################################
--echo ++ Dynamic privilege ROLE_ADMIN grants the ability
--echo ++ to grant any role to anyone (but not grant any privileges)
--echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
CREATE USER u3@localhost IDENTIFIED BY 'foo';
CREATE ROLE role_admin, arbitrary_role;
GRANT ROLE_ADMIN ON *.* TO role_admin;
GRANT role_admin TO u3@localhost;
connect(con2, localhost, u3, foo, test);
--echo ++ Connected as u3@localhost
SET ROLE role_admin;
GRANT arbitrary_role TO u1@localhost;
--echo Granting a role not granted will also work.
GRANT r1 TO u1@localhost;
GRANT r1 TO u3@localhost;
--echo But you can't grant any privileges
--error ER_ACCESS_DENIED_ERROR
GRANT SELECT ON *.* TO r1;
--error ER_ACCESS_DENIED_ERROR
GRANT SELECT ON *.* TO u3@localhost;
connection default;
--echo ++ Connected as root
DROP USER u1@localhost, u2@localhost, u3@localhost;
DROP ROLE r1,r2,role_admin,arbitrary_role;
disconnect con1;
--echo #############################################################
--echo ++ Check that WITH ADMIN properties are persistent
--echo #############################################################
CREATE USER `u1`@`%` IDENTIFIED BY 'foo';
CREATE USER `u2`@`%` IDENTIFIED BY 'foo';
CREATE ROLE r1, r2, r3;
GRANT r2 TO r1;
GRANT r3 TO r2;
GRANT r2 TO u1;
GRANT r3 TO u2;

# Connect as u1 - Trying to grant r1,r2,r3 to other use u2 fails as expected.
connect(con1, localhost, u1, foo, test);
SET ROLE all;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
GRANT r1 TO u2;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
GRANT r2 TO u2;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
GRANT r3 TO u2;

# Connect as root
--connection default
GRANT r3 TO r2 WITH ADMIN OPTION;
SELECT * FROM mysql.role_edges;

connection con1;
SET ROLE all;
SHOW GRANTS;
--echo ## We got WITH ADMIN on r3 through the stickiness of the edge property WITH ADMIN which arrived from r2->u1
GRANT r3 to u2;
--echo ## r1 is not granted to u2, and hence we cannot modify it, nor grant it to anyone else.
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
GRANT r1 TO u2;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
GRANT r1 TO u1;
--echo ## r2 is indirectly granted to u2 but we don't have WITH ADMIN on this relation.
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
GRANT r2 TO u2;
--echo ## (r3,u2) has been propagated as an effective WITH ADMIN privilege when r1 was activated.
GRANT r3 TO u2;
--echo ## Since we didn't grant the (r3,u2) relation to our current user we can't set it as active.
--error ER_ROLE_NOT_GRANTED
SET ROLE r3;
--echo # but if we grant it explicitly to form the persistent relation(r3,u1) we can use it.
GRANT r3 TO u1 WITH ADMIN OPTION;
SET ROLE r3;

connection default;
--echo # Let's see if this stick after we flush the caches.
FLUSH PRIVILEGES;
disconnect con1;
connect(con1, localhost, u1, foo, test);
SET ROLE r3;
GRANT r3 TO u2;
GRANT r3 TO u2 WITH ADMIN OPTION;

connection default;
--echo ## The principle is that we don't remove privileges when we issue a GRANT - only elevate.
--echo ## As a result (r3,u2) should be WITH ADMIN
SELECT * FROM mysql.role_edges;
connection con1;
--echo ## But if we try to reverse this, then nothing should happen.
GRANT r3 TO u2;
connection default;
--echo ## And (r3,u2) should still be WITH ADMIN.
SELECT * FROM mysql.role_edges;
--disconnect con1

DROP ROLE r1,r2,r3;
DROP USER u1,u2;


