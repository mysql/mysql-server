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
++ admin-db1 can manage db2.t1 and admin-db2 can manage db1.t1
GRANT `admin-db2t1` TO `admin-db1`;
GRANT `admin-db1t1` TO `admin-db2`;
++ admin-db1 can promote anyone with the admin-db1t1 rights.
GRANT `admin-db1t1` TO `admin-db1` WITH ADMIN OPTION;
GRANT SELECT, UPDATE, CREATE, DROP, INSERT, DELETE ON db1.* TO `admin-db1`;
GRANT SELECT, UPDATE, CREATE, DROP, INSERT, DELETE ON db2.* TO `admin-db2`;
GRANT SELECT, UPDATE, CREATE, DROP, INSERT, DELETE ON db1.t1 TO `admin-db1t1`;
GRANT SELECT, UPDATE, CREATE, DROP, INSERT, DELETE ON db2.t1 TO `admin-db2t1`;
SET ROLE `admin-db1`;
SHOW GRANTS;
Grants for app-middleware-db1@localhost
GRANT USAGE ON *.* TO `app-middleware-db1`@`localhost`
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP ON `db1`.* TO `app-middleware-db1`@`localhost`
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP ON `db1`.`t1` TO `app-middleware-db1`@`localhost`
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP ON `db2`.`t1` TO `app-middleware-db1`@`localhost`
GRANT `admin-db1`@`%`,`app-updater`@`%` TO `app-middleware-db1`@`localhost`
GRANT `admin-db1t1`@`%` TO `app-middleware-db1`@`localhost` WITH ADMIN OPTION
++ Positive test
INSERT INTO db1.t1 VALUES (1,2,3);
INSERT INTO db1.t2 VALUES (1,2,3);
INSERT INTO db2.t1 VALUES (1,2,3);
SELECT * FROM db1.t1;
c1	c2	c3
1	2	3
SELECT * FROM db1.t2;
c1	c2	c3
1	2	3
SELECT * FROM db2.t1;
c1	c2	c3
1	2	3
GRANT `admin-db1t1` TO `app`@`localhost`;
GRANT r1 TO `app-middleware-db1`@`localhost` WITH ADMIN OPTION;
++ Connected as app-middleware-db1
SET ROLE `admin-db1`;
GRANT `admin-db1t1` TO `app`@`localhost`;
++ r1 and inherited role admin-db1t1 should be WITH ADMIN OPTION
SHOW GRANTS FOR CURRENT_USER() USING `admin-db1`;
Grants for app-middleware-db1@localhost
GRANT USAGE ON *.* TO `app-middleware-db1`@`localhost`
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP ON `db1`.* TO `app-middleware-db1`@`localhost`
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP ON `db1`.`t1` TO `app-middleware-db1`@`localhost`
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP ON `db2`.`t1` TO `app-middleware-db1`@`localhost`
GRANT `admin-db1`@`%`,`app-updater`@`%` TO `app-middleware-db1`@`localhost`
GRANT `admin-db1t1`@`%`,`r1`@`%` TO `app-middleware-db1`@`localhost` WITH ADMIN OPTION
++ Negative test
INSERT INTO db2.t2 VALUES (1,2,3);
ERROR 42000: INSERT command denied to user 'app-middleware-db1'@'localhost' for table 't2'
SELECT * FROM db2.t2;
ERROR 42000: SELECT command denied to user 'app-middleware-db1'@'localhost' for table 't2'
GRANT `admin-db2t1` TO `app`@`localhost`;
ERROR 42000: Access denied; you need (at least one of) the WITH ADMIN, ROLE_ADMIN, SUPER privilege(s) for this operation
++ Connected as root
++ Granting WITH ADMIN OPTION role WITH ADMIN OPTION privileges
++ app@localhost has admin-db1t1 granted.
++ Connected as app@localhost
SHOW GRANTS FOR CURRENT_USER();
Grants for app@localhost
GRANT USAGE ON *.* TO `app`@`localhost`
GRANT `admin-db1t1`@`%` TO `app`@`localhost`
++ Positive test ; setting a granted role.
SET ROLE `admin-db1t1`;
SELECT CURRENT_USER(), CURRENT_ROLE();
CURRENT_USER()	CURRENT_ROLE()
app@localhost	`admin-db1t1`@`%`
++ Negative tests ; Attempt to grant the granted role to 3rd party
++ app@localhost did not inherit the ability to grant WITH ADMIN OPTION
GRANT `admin-db1t1` TO `app-middleware-db2`@`localhost`;
ERROR 42000: Access denied; you need (at least one of) the WITH ADMIN, ROLE_ADMIN, SUPER privilege(s) for this operation
# only count nodes and edges as the sorting order is depending on platform
SELECT ExtractValue(ROLES_GRAPHML(),'count(//node)') as num_nodes;
num_nodes
13
SELECT ExtractValue(ROLES_GRAPHML(),'count(//edge)') as num_edges;
num_edges
8
++ Now grant admin-db1t1 to app@localhost WITH ADMIN OPTION
++ Positive test
++ Connected as app-middleware-db1@localhost
GRANT `admin-db1t1` TO `app`@`localhost` WITH ADMIN OPTION;
++ Connected as app@localhost
++ app@localhost should now be able to grant admin-db1t1 to app-middleware
SET ROLE ALL;
SELECT CURRENT_USER(), CURRENT_ROLE();
CURRENT_USER()	CURRENT_ROLE()
app@localhost	`admin-db1t1`@`%`
GRANT `admin-db1t1` TO `app-middleware-db2`@`localhost`;
++ Revoking roles require WITH ADMIN too
++ Positive tests
REVOKE `admin-db1t1` FROM `app-middleware-db2`@`localhost`;
++ Restorning grant for negative test
GRANT `admin-db1t1` TO `app-middleware-db2`@`localhost`;
++ Connected as app-middleware-db1@localhost
++ Remove WITH ADMIN grants by removing and re-granting role
REVOKE `admin-db1t1` FROM `app`@`localhost`;
GRANT `admin-db1t1` TO `app`@`localhost`;
++ Connected as app@localhost
++ Negative tests
REVOKE `admin-db1t1` FROM `app-middleware-db2`@`localhost`;
ERROR 42000: Access denied; you need (at least one of) the WITH ADMIN, ROLE_ADMIN, SUPER privilege(s) for this operation
++ Connected as app-middleware-db1@localhost
++ Positive test
SELECT CURRENT_USER(), CURRENT_ROLE();
CURRENT_USER()	CURRENT_ROLE()
app-middleware-db1@localhost	`admin-db1`@`%`
SHOW GRANTS;
Grants for app-middleware-db1@localhost
GRANT USAGE ON *.* TO `app-middleware-db1`@`localhost`
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP ON `db1`.* TO `app-middleware-db1`@`localhost`
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP ON `db1`.`t1` TO `app-middleware-db1`@`localhost`
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, DROP ON `db2`.`t1` TO `app-middleware-db1`@`localhost`
GRANT `admin-db1`@`%`,`app-updater`@`%` TO `app-middleware-db1`@`localhost`
GRANT `admin-db1t1`@`%`,`r1`@`%` TO `app-middleware-db1`@`localhost` WITH ADMIN OPTION
++ User stil has WITH ADMIN and can revoke from `app-middleware-db2`@`localhost`
REVOKE `admin-db1t1` FROM `app-middleware-db2`@`localhost`;
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
+++++++++++++++++++++++++++++
++ WITH GRANT OPTION tests ++
+++++++++++++++++++++++++++++
CREATE USER u1@localhost IDENTIFIED BY 'foo';
CREATE USER u2@localhost IDENTIFIED BY 'foo';
CREATE ROLE r1;
CREATE DATABASE db1;
GRANT CREATE ON db1.* TO r1 WITH GRANT OPTION;
GRANT r1 TO u1@localhost;
++ Connected as u1@localhost
SET ROLE ALL;
GRANT CREATE ON db1.* TO u2@localhost;
SET ROLE NONE;
GRANT CREATE ON db1.* TO u2@localhost;
ERROR 42000: Access denied for user 'u1'@'localhost' to database 'db1'
++ Connected as root
DROP USER u1@localhost, u2@localhost;
DROP ROLE r1;
DROP DATABASE db1;
SELECT user,host FROM mysql.user;
user	host
mysql.infoschema	localhost
mysql.session	localhost
mysql.sys	localhost
root	localhost
#############################################
CREATE USER u1@localhost IDENTIFIED BY 'foo';
CREATE USER u2@localhost IDENTIFIED BY 'foo';
CREATE ROLE r1, r2;
use test;
GRANT CREATE ON test.* TO r1;
GRANT DROP ON test.* TO r2;
GRANT r1 TO u1@localhost WITH ADMIN OPTION;
GRANT r2 TO u1@localhost;
++ Connected as u1@localhost
SET ROLE ALL;
SHOW GRANTS;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT CREATE, DROP ON `test`.* TO `u1`@`localhost`
GRANT `r2`@`%` TO `u1`@`localhost`
GRANT `r1`@`%` TO `u1`@`localhost` WITH ADMIN OPTION
GRANT r1 TO u2@localhost WITH ADMIN OPTION;
GRANT r2 TO u2@localhost WITH ADMIN OPTION;
ERROR 42000: Access denied; you need (at least one of) the WITH ADMIN, ROLE_ADMIN, SUPER privilege(s) for this operation
++ Connected as root
#############################################################
++ Dynamic privilege ROLE_ADMIN grants the ability
++ to grant any role to anyone (but not grant any privileges)
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
CREATE USER u3@localhost IDENTIFIED BY 'foo';
CREATE ROLE role_admin, arbitrary_role;
GRANT ROLE_ADMIN ON *.* TO role_admin;
GRANT role_admin TO u3@localhost;
++ Connected as u3@localhost
SET ROLE role_admin;
GRANT arbitrary_role TO u1@localhost;
Granting a role not granted will also work.
GRANT r1 TO u1@localhost;
GRANT r1 TO u3@localhost;
But you can't grant any privileges
GRANT SELECT ON *.* TO r1;
ERROR 28000: Access denied for user 'u3'@'localhost' (using password: YES)
GRANT SELECT ON *.* TO u3@localhost;
ERROR 28000: Access denied for user 'u3'@'localhost' (using password: YES)
++ Connected as root
DROP USER u1@localhost, u2@localhost, u3@localhost;
DROP ROLE r1,r2,role_admin,arbitrary_role;
#############################################################
++ Check that WITH ADMIN properties are persistent
#############################################################
CREATE USER `u1`@`%` IDENTIFIED BY 'foo';
CREATE USER `u2`@`%` IDENTIFIED BY 'foo';
CREATE ROLE r1, r2, r3;
GRANT r2 TO r1;
GRANT r3 TO r2;
GRANT r2 TO u1;
GRANT r3 TO u2;
SET ROLE all;
GRANT r1 TO u2;
ERROR 42000: Access denied; you need (at least one of) the WITH ADMIN, ROLE_ADMIN, SUPER privilege(s) for this operation
GRANT r2 TO u2;
ERROR 42000: Access denied; you need (at least one of) the WITH ADMIN, ROLE_ADMIN, SUPER privilege(s) for this operation
GRANT r3 TO u2;
ERROR 42000: Access denied; you need (at least one of) the WITH ADMIN, ROLE_ADMIN, SUPER privilege(s) for this operation
GRANT r3 TO r2 WITH ADMIN OPTION;
SELECT * FROM mysql.role_edges;
FROM_HOST	FROM_USER	TO_HOST	TO_USER	WITH_ADMIN_OPTION
%	r2	%	r1	N
%	r2	%	u1	N
%	r3	%	r2	Y
%	r3	%	u2	N
SET ROLE all;
SHOW GRANTS;
Grants for u1@%
GRANT USAGE ON *.* TO `u1`@`%`
GRANT `r2`@`%` TO `u1`@`%`
GRANT `r3`@`%` TO `u1`@`%` WITH ADMIN OPTION
## We got WITH ADMIN on r3 through the stickiness of the edge property WITH ADMIN which arrived from r2->u1
GRANT r3 to u2;
## r1 is not granted to u2, and hence we cannot modify it, nor grant it to anyone else.
GRANT r1 TO u2;
ERROR 42000: Access denied; you need (at least one of) the WITH ADMIN, ROLE_ADMIN, SUPER privilege(s) for this operation
GRANT r1 TO u1;
ERROR 42000: Access denied; you need (at least one of) the WITH ADMIN, ROLE_ADMIN, SUPER privilege(s) for this operation
## r2 is indirectly granted to u2 but we don't have WITH ADMIN on this relation.
GRANT r2 TO u2;
ERROR 42000: Access denied; you need (at least one of) the WITH ADMIN, ROLE_ADMIN, SUPER privilege(s) for this operation
## (r3,u2) has been propagated as an effective WITH ADMIN privilege when r1 was activated.
GRANT r3 TO u2;
## Since we didn't grant the (r3,u2) relation to our current user we can't set it as active.
SET ROLE r3;
ERROR HY000: `r3`@`%` is not granted to `u1`@`%`
# but if we grant it explicitly to form the persistent relation(r3,u1) we can use it.
GRANT r3 TO u1 WITH ADMIN OPTION;
SET ROLE r3;
# Let's see if this stick after we flush the caches.
FLUSH PRIVILEGES;
SET ROLE r3;
GRANT r3 TO u2;
GRANT r3 TO u2 WITH ADMIN OPTION;
## The principle is that we don't remove privileges when we issue a GRANT - only elevate.
## As a result (r3,u2) should be WITH ADMIN
SELECT * FROM mysql.role_edges;
FROM_HOST	FROM_USER	TO_HOST	TO_USER	WITH_ADMIN_OPTION
%	r2	%	r1	N
%	r2	%	u1	N
%	r3	%	r2	Y
%	r3	%	u1	Y
%	r3	%	u2	Y
## But if we try to reverse this, then nothing should happen.
GRANT r3 TO u2;
## And (r3,u2) should still be WITH ADMIN.
SELECT * FROM mysql.role_edges;
FROM_HOST	FROM_USER	TO_HOST	TO_USER	WITH_ADMIN_OPTION
%	r2	%	r1	N
%	r2	%	u1	N
%	r3	%	r2	Y
%	r3	%	u1	Y
%	r3	%	u2	Y
DROP ROLE r1,r2,r3;
DROP USER u1,u2;
