CREATE ROLE r1;
CREATE DATABASE db1;
CREATE DATABASE db2;
CREATE USER u1@localhost IDENTIFIED BY 'foo';
GRANT CREATE ON db1.* TO u1@localhost;
GRANT r1 TO u1@localhost;
SHOW STATUS LIKE '%acl_cache%';
CREATE TABLE db1.t1 (c1 int);
CREATE TABLE db1.t2 (c1 int);
CREATE TABLE db2.t1 (c1 int);
CREATE TABLE db2.t2 (c1 int);

CREATE SQL SECURITY DEFINER VIEW db1.v1 AS SELECT * FROM db1.t1;
CREATE SQL SECURITY DEFINER VIEW db2.v1 AS SELECT * FROM db2.t1;
CREATE SQL SECURITY DEFINER VIEW db1.v2 AS SELECT * FROM db1.t1;
CREATE SQL SECURITY INVOKER VIEW db1.v4 AS SELECT * FROM db2.t2;

--echo ++ Test global level privileges
GRANT SELECT ON *.* TO r1;
SHOW GRANTS FOR u1@localhost USING r1;

connect(con1, localhost, u1, foo, db1);
SET ROLE r1;
--echo ++ Positive test
SELECT * FROM v1;
SELECT * FROM db2.v1;
SELECT * FROM v4;

--echo ++ Test revoke
connection default;
REVOKE SELECT ON *.* FROM r1;
SHOW GRANTS FOR u1@localhost USING r1;
connection con1;
SET ROLE r1;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM v1;

--echo ++ Test schema level privileges
connection default;
GRANT SELECT ON db1.* TO r1;
SHOW GRANTS FOR u1@localhost USING r1;
connection con1;
SET ROLE r1;

--echo ++ Positive test
SELECT * FROM v1;
SELECT * FROM v2;

--echo ++ Negative test
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db2.v1;
--error ER_VIEW_INVALID
SELECT * FROM v4;

connection default;
REVOKE SELECT ON db1.* FROM r1;

--echo ++ Test routine level privileges
GRANT SELECT ON db1.v1 TO r1;
connection con1;
SET ROLE r1;

--echo ++ Positive test
SELECT * FROM v1;

--echo ++ Negative test
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM v2;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db2.v1;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM v4;

--echo ++ Test Security invoker model
connection default;
GRANT SELECT ON db2.* TO r1;
GRANT SELECT ON db1.* TO r1;
GRANT CREATE VIEW ON db1.* TO r1;
connection con1;
SET ROLE r1;

--echo ++ Positive test
SELECT * FROM v1;
SHOW GRANTS FOR CURRENT_USER;
SELECT * FROM v4;

--echo ++ Test SUID = u1@localhost with default roles
CREATE SQL SECURITY DEFINER VIEW db1.v5 AS SELECT * FROM db2.t1;

--echo Negative test; DEFINER VIEWs always use default roles
--error ER_VIEW_INVALID
SELECT * FROM v5;

--echo Positive test; Added default role.
ALTER USER u1@localhost DEFAULT ROLE r1;
SELECT * FROM v5;

connection default;
CREATE USER u2@localhost IDENTIFIED BY 'oof';
GRANT SELECT ON db1.* TO u2@localhost;
SHOW GRANTS FOR u1@localhost USING r1;
connect(con2, localhost, u2, oof, db1);

--echo ++ Positive test
SELECT * FROM db1.v5;
SELECT * FROM db1.v2;

--echo ++ Negative test
--error ER_VIEW_INVALID
SELECT * FROM db1.v4;
connection default;
REVOKE r1 FROM u1@localhost;
connection con2;
--error ER_VIEW_INVALID
SELECT * FROM v5;

--echo ++ Clean up
connection default;
DROP DATABASE db1;
DROP DATABASE db2;
DROP USER u1@localhost;
DROP ROLE r1;
DROP USER u2@localhost;
disconnect con1;
disconnect con2;
SHOW STATUS LIKE '%acl_cache%';


--echo #
--echo # BUG#34341533: "SHOW FIELDS FROM" fails against a view when a view
--echo #               accessing from a view is recreated
--echo #

CREATE USER user_with_role@localhost;
CREATE ROLE test_role;
GRANT ALL on *.* TO test_role;
GRANT test_role TO user_with_role@localhost;
SET DEFAULT ROLE test_role TO user_with_role@localhost;

CREATE USER user_without_role@localhost;
GRANT ALL on *.* TO user_without_role@localhost;

--enable_connect_log
--connect(user_with_role, localhost, user_with_role,,)
USE test;
CREATE TABLE t1 (c1 INT);
CREATE VIEW v1 AS SELECT * FROM t1;
CREATE VIEW v2 AS SELECT * FROM v1;
DROP VIEW v1;
CREATE VIEW v1 AS SELECT * FROM t1;
--echo # Without the patch, the following queries reported ER_VIEW_INVALID error
SHOW FIELDS FROM v2;
DESCRIBE v2;

--connect(user_without_role, localhost, user_without_role,,)
USE test;
CREATE TABLE t2 (c1 INT);
CREATE VIEW v3 AS SELECT * FROM t2;
CREATE VIEW v4 AS SELECT * FROM v3;
DROP VIEW v3;
CREATE VIEW v3 AS SELECT * FROM t2;
SHOW FIELDS FROM v4;
DESCRIBE v4;

# Cleanup
--disconnect user_without_role
--disconnect user_with_role
--connection default
--disable_connect_log

DROP VIEW v1, v2, v3, v4;
DROP TABLE t1, t2;

DROP USER user_without_role@localhost;
DROP USER user_with_role@localhost;
DROP ROLE test_role;


--echo #
--echo # BUG#34467659: "SHOW FIELDS FROM" fails against a view when a view
--echo #               accessing from a view is recreated
--echo #
--echo Note, that the difference between test for bug#34341533 and bug#34467659
--echo is that in granting on *.* vs granting on specific resources.

CREATE USER user_with_role@localhost;
CREATE ROLE test_role;
GRANT ALL on test.t1 TO test_role;
GRANT ALL on test.v1 TO test_role;
GRANT ALL on test.v2 TO test_role;
GRANT test_role TO user_with_role@localhost;
SET DEFAULT ROLE test_role TO user_with_role@localhost;

CREATE USER user_without_role@localhost;
GRANT ALL on test.t2 TO user_without_role@localhost;
GRANT ALL on test.v3 TO user_without_role@localhost;
GRANT ALL on test.v4 TO user_without_role@localhost;

--enable_connect_log
--connect(user_with_role, localhost, user_with_role,,)
USE test;
CREATE TABLE t1 (c1 INT);
CREATE VIEW v1 AS SELECT * FROM t1;
CREATE VIEW v2 AS SELECT * FROM v1;
DROP VIEW v1;
CREATE VIEW v1 AS SELECT * FROM t1;
--echo # Without the patch, the following queries reported ER_VIEW_INVALID error
SHOW FIELDS FROM v2;
DESCRIBE v2;

--connect(user_without_role, localhost, user_without_role,,)
USE test;
CREATE TABLE t2 (c1 INT);
CREATE VIEW v3 AS SELECT * FROM t2;
CREATE VIEW v4 AS SELECT * FROM v3;
DROP VIEW v3;
CREATE VIEW v3 AS SELECT * FROM t2;
--echo # Without the patch, the following queries also reported ER_VIEW_INVALID error
SHOW FIELDS FROM v4;
DESCRIBE v4;

# Cleanup
--disconnect user_without_role
--disconnect user_with_role
--connection default
--disable_connect_log

DROP VIEW v1, v2, v3, v4;
DROP TABLE t1, t2;

DROP USER user_without_role@localhost;
DROP USER user_with_role@localhost;
DROP ROLE test_role;