set @orig_partial_revokes = @@global.partial_revokes;
# Turn Off the partial_revokes to test with wildcard grants
SET GLOBAL partial_revokes= OFF;

CREATE ROLE r1;

CREATE USER u1@localhost IDENTIFIED BY 'foo';
SHOW GRANTS FOR u1@localhost;

CREATE DATABASE db1;

CREATE TABLE db1.t1 (c1 INT, c2 INT, c3 INT);
CREATE TABLE db1.t2 (c1 INT, c2 INT, c3 INT);

--echo ++ Test global level privileges
GRANT SELECT ON *.* TO r1;
GRANT r1 TO u1@localhost;
SHOW GRANTS FOR u1@localhost USING r1;

connect(con1, localhost, u1, foo, test);
SET ROLE r1;

# test negative
--error ER_TABLEACCESS_DENIED_ERROR
UPDATE db1.t1 SET c1=2;

# test positive
SELECT * FROM db1.t1;

# test revoke
connection default;
REVOKE SELECT ON *.* FROM r1;
SHOW GRANTS FOR u1@localhost USING r1;
connection con1;
SET ROLE r1;

SHOW GRANTS FOR CURRENT_USER() USING r1;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db1.t1;

--echo ++ Test schema level privilege
connection default;
GRANT SELECT ON db1.* TO r1;
SHOW GRANTS FOR u1@localhost USING r1;

connection con1;
SET ROLE r1;

# test negative
--error ER_TABLEACCESS_DENIED_ERROR
UPDATE db1.t1 SET c1=2;

# test positive
SELECT * FROM db1.t1;

# side track:
# set roles to none and verify that privileges are updated!
SET ROLE NONE;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db1.t1;
SET ROLE r1;
SELECT * FROM db1.t1;

# test revoke
connection default;
REVOKE SELECT ON db1.* FROM r1;
SHOW GRANTS FOR u1@localhost USING r1;

connection con1;
SET ROLE r1;

--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db1.t1;

--echo ++ Test table level privileges
connection default;
GRANT SELECT ON db1.t1 TO r1;
SHOW GRANTS FOR u1@localhost USING r1;

connection con1;
SET ROLE r1;

# test negative
--error ER_TABLEACCESS_DENIED_ERROR
UPDATE db1.t1 SET c1=2;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db1.t2;

# test positive
SELECT * FROM db1.t1;

# test revoke
connection default;
REVOKE SELECT ON db1.t1 FROM r1;
SHOW GRANTS FOR u1@localhost USING r1;
connection con1;
SET ROLE r1;

--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db1.t1;

--echo ++ Test column level privileges
connection default;
GRANT SELECT(c1) ON db1.t1 TO r1;
SHOW GRANTS FOR u1@localhost USING r1;

connection con1;
SET ROLE r1;

# test negative
--error ER_TABLEACCESS_DENIED_ERROR
UPDATE db1.t1 SET c1=2;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT c1 FROM db1.t2;
--error ER_COLUMNACCESS_DENIED_ERROR
SELECT c2 FROM db1.t1;


# test positive
SELECT c1 FROM db1.t1;

# test revoke
connection default;
REVOKE SELECT(c1) ON db1.t1 FROM r1;
SHOW GRANTS FOR u1@localhost USING r1;
connection con1;
SET ROLE r1;

--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db1.t1;

--echo ++ Clean up
connection default;
DROP ROLE r1;
DROP USER u1@localhost;
DROP DATABASE db1;
disconnect con1;

--echo ++++++++++++++++++++++++++++++++++++++++
--echo ++ 2 role depths
--echo ++++++++++++++++++++++++++++++++++++++++
CREATE ROLE r1;
CREATE ROLE r2;

CREATE USER u1@localhost IDENTIFIED BY 'foo';
SHOW GRANTS FOR u1@localhost;

CREATE DATABASE db1;
CREATE DATABASE db2;

CREATE TABLE db1.t1 (c1 INT, c2 INT, c3 INT);
CREATE TABLE db1.t2 (c1 INT, c2 INT, c3 INT);
CREATE TABLE db1.t3 (c1 INT, c2 INT, c3 INT);

CREATE TABLE db2.t1 (c1 INT, c2 INT, c3 INT);
CREATE TABLE db2.t2 (c1 INT, c2 INT, c3 INT);

--echo ++ Test global level privileges
GRANT SELECT ON *.* TO r2;
GRANT SELECT ON db1.t2 TO r1 WITH GRANT OPTION;
GRANT r2 TO r1;
GRANT r1 TO u1@localhost;
SHOW GRANTS FOR u1@localhost USING r1;

connect(con1, localhost, u1, foo, test);
SET ROLE r1;

# test negative
--error ER_TABLEACCESS_DENIED_ERROR
UPDATE db1.t1 SET c1=2;
--error ER_TABLEACCESS_DENIED_ERROR
UPDATE db1.t2 SET c1=2;
--error ER_ACCESS_DENIED_ERROR
GRANT SELECT ON *.* TO u1@localhost;
--error ER_DBACCESS_DENIED_ERROR
GRANT SELECT ON db1.* TO u1@localhost;
--error ER_TABLEACCESS_DENIED_ERROR
GRANT SELECT ON db1.t1 TO u1@localhost;

# test positive
SELECT * FROM db1.t1;
SELECT * FROM db1.t2;
GRANT SELECT ON db1.t2 TO u1@localhost;
REVOKE SELECT ON db1.t2 FROM u1@localhost;
GRANT SELECT (c1) ON db1.t2 TO u1@localhost;
REVOKE SELECT (c1) ON db1.t2 FROM u1@localhost;

# test revoke
connection default;
REVOKE SELECT ON *.* FROM r2;
SHOW GRANTS FOR u1@localhost USING r1;
connection con1;
SET ROLE r1;

--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db1.t1;
SELECT * FROM db1.t2;

--echo ++ Test schema level privilege
connection default;
GRANT SELECT ON db1.* TO r2;
SHOW GRANTS FOR u1@localhost USING r1;

connection con1;
SET ROLE r1;

# test negative
--error ER_TABLEACCESS_DENIED_ERROR
UPDATE db1.t1 SET c1=2;
--error ER_TABLEACCESS_DENIED_ERROR
UPDATE db1.t2 SET c1=2;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db2.t1;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db2.t2;

# test positive
SELECT * FROM db1.t1;
SELECT * FROM db1.t2;

# test revoke
connection default;
REVOKE SELECT ON db1.* FROM r2;
SHOW GRANTS FOR u1@localhost USING r1;

connection con1;
SET ROLE r1;

--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db1.t1;

--echo ++ Test table level privileges
connection default;
GRANT SELECT ON db1.t1 TO r2;
SHOW GRANTS FOR u1@localhost USING r1;

connection con1;
SET ROLE r1;

# test negative
--error ER_TABLEACCESS_DENIED_ERROR
UPDATE db1.t1 SET c1=2;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db1.t3;


# test positive
SELECT * FROM db1.t1;
SELECT * FROM db1.t2;

# test revoke
connection default;
REVOKE SELECT ON db1.t1 FROM r2;
SHOW GRANTS FOR u1@localhost USING r1;
connection con1;
SET ROLE r1;

--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db1.t1;

--echo ++ Test column level privileges
connection default;
GRANT SELECT(c1) ON db1.t1 TO r2;
SHOW GRANTS FOR u1@localhost USING r1;

connection con1;
SET ROLE r1;

# test negative
--error ER_TABLEACCESS_DENIED_ERROR
UPDATE db1.t1 SET c1=2;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT c1 FROM db1.t3;
--error ER_COLUMNACCESS_DENIED_ERROR
SELECT c2 FROM db1.t1;

# test positive
SELECT c1 FROM db1.t1;
SELECT * FROM db1.t2;

# test revoke
connection default;
REVOKE SELECT(c1) ON db1.t1 FROM r2;
SHOW GRANTS FOR u1@localhost USING r1;
connection con1;
SET ROLE r1;

--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db1.t1;

--echo ++ Clean up
connection default;
DROP ROLE r1;
DROP ROLE r2;
DROP USER u1@localhost;
DROP DATABASE db1;
DROP DATABASE db2;
disconnect con1;

--echo ++++++++++++++++++++++++++++++++++++++++
--echo ++ 1 role depths, two active roles
--echo ++++++++++++++++++++++++++++++++++++++++
CREATE ROLE r1;
CREATE ROLE r2;

CREATE USER u1@localhost IDENTIFIED BY 'foo';
SHOW GRANTS FOR u1@localhost;

CREATE DATABASE db1;
CREATE DATABASE db2;

CREATE TABLE db1.t1 (c1 INT, c2 INT, c3 INT);
CREATE TABLE db1.t2 (c1 INT, c2 INT, c3 INT);
CREATE TABLE db1.t3 (c1 INT, c2 INT, c3 INT);

CREATE TABLE db2.t1 (c1 INT, c2 INT, c3 INT);
CREATE TABLE db2.t2 (c1 INT, c2 INT, c3 INT);

--echo ++ Test global level privileges
GRANT SELECT ON *.* TO r2;
GRANT SELECT ON db1.t2 TO r1;
GRANT r1 TO u1@localhost;
GRANT r2 TO u1@localhost;
SHOW GRANTS FOR u1@localhost USING r1,r2;

connect(con1, localhost, u1, foo, test);
SET ROLE r1,r2;

# test negative
--error ER_TABLEACCESS_DENIED_ERROR
UPDATE db1.t1 SET c1=2;
--error ER_TABLEACCESS_DENIED_ERROR
UPDATE db1.t2 SET c1=2;

# test positive
SELECT * FROM db1.t1;
SELECT * FROM db1.t2;

# test revoke
connection default;
REVOKE SELECT ON *.* FROM r2;
SHOW GRANTS FOR u1@localhost USING r1,r2;
connection con1;
SET ROLE r1,r2;

--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db1.t1;
SELECT * FROM db1.t2;

--echo ++ Test schema level privilege
connection default;
GRANT SELECT ON db1.* TO r2;
SHOW GRANTS FOR u1@localhost USING r1,r2;

connection con1;
SET ROLE r1,r2;

# test negative
--error ER_TABLEACCESS_DENIED_ERROR
UPDATE db1.t1 SET c1=2;
--error ER_TABLEACCESS_DENIED_ERROR
UPDATE db1.t2 SET c1=2;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db2.t1;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db2.t2;

# test positive
SELECT * FROM db1.t1;
SELECT * FROM db1.t2;

# test revoke
connection default;
REVOKE SELECT ON db1.* FROM r2;
SHOW GRANTS FOR u1@localhost USING r1,r2;

connection con1;
SET ROLE r1,r2;

--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db1.t1;

--echo ++ Test table level privileges
connection default;
GRANT SELECT ON db1.t1 TO r2;
SHOW GRANTS FOR u1@localhost USING r1,r2;

connection con1;
SET ROLE r1,r2;

# test negative
--error ER_TABLEACCESS_DENIED_ERROR
UPDATE db1.t1 SET c1=2;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db1.t3;


# test positive
SELECT * FROM db1.t1;
SELECT * FROM db1.t2;

# test revoke
connection default;
REVOKE SELECT ON db1.t1 FROM r2;
SHOW GRANTS FOR u1@localhost USING r1,r2;
connection con1;
SET ROLE r1,r2;

--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db1.t1;

--echo ++ Test column level privileges
connection default;
GRANT SELECT(c1) ON db1.t1 TO r2;
SHOW GRANTS FOR u1@localhost USING r1,r2;

connection con1;
SET ROLE r1,r2;

# test negative
--error ER_TABLEACCESS_DENIED_ERROR
UPDATE db1.t1 SET c1=2;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT c1 FROM db1.t3;
--error ER_COLUMNACCESS_DENIED_ERROR
SELECT c2 FROM db1.t1;

# test positive
SELECT c1 FROM db1.t1;
SELECT * FROM db1.t2;

# test revoke
connection default;
REVOKE SELECT(c1) ON db1.t1 FROM r2;
SHOW GRANTS FOR u1@localhost USING r1,r2;
connection con1;
SET ROLE r1,r2;

--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db1.t1;

--echo ++ Clean up
connection default;
DROP ROLE r1;
DROP ROLE r2;
DROP USER u1@localhost;
SHOW STATUS LIKE '%Acl_cache%';
DROP DATABASE db1;
DROP DATABASE db2;
disconnect con1;

--echo ++++++++++++++++++++++++++++++++++++++++
--echo ++ 1 role depths, database patterns
--echo ++++++++++++++++++++++++++++++++++++++++
CREATE ROLE r1, r2, r3, r4;
CREATE USER u1@localhost IDENTIFIED BY 'foo';
CREATE USER u2@localhost IDENTIFIED BY 'foo';
SHOW GRANTS FOR u1@localhost;

CREATE DATABASE db1;
CREATE DATABASE db2;
CREATE DATABASE db1aaaa;
CREATE DATABASE dddddb1;
CREATE DATABASE secdb1;
CREATE DATABASE secdb2;

CREATE TABLE db1.t1 (c1 INT);
CREATE TABLE db2.t1 (c2 INT);
CREATE TABLE dddddb1.t1 (c2 INT);
CREATE TABLE db1aaaa.t1 (c2 INT);
CREATE TABLE secdb1.t1 (c1 INT);
CREATE TABLE secdb2.t1 (c2 INT);

INSERT INTO db1.t1 VALUES (1),(2),(3);
INSERT INTO db2.t1 VALUES (1),(2),(3);
INSERT INTO dddddb1.t1 VALUES (1),(2),(3);
INSERT INTO db1aaaa.t1 VALUES (1),(2),(3);

GRANT SELECT ON `db_`.* TO r1;
GRANT SELECT ON `db%`.* TO r2;
GRANT SELECT ON `db%`.* TO r3 WITH GRANT OPTION;
GRANT SELECT ON `secdb1`.* TO r4 WITH GRANT OPTION;

GRANT r1,r2,r3,r4 TO u1@localhost;

connect(con1, localhost, u1, foo, test);
SET ROLE r1;
SHOW GRANTS;

# Positive test
SELECT * FROM db1.t1;
SELECT * FROM db2.t1;

# Negative test
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db1aaaa.t1;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM dddddb1.t1;

SET ROLE r2;
# Positive test
SELECT * FROM db1.t1;
SELECT * FROM db2.t1;
SELECT * FROM db1aaaa.t1;

# Negative test
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM dddddb1.t1;
--error ER_DBACCESS_DENIED_ERROR
GRANT SELECT ON db1.* TO u2@localhost;

SET ROLE r3;
# Positive test
SELECT * FROM db1.t1;
SELECT * FROM db2.t1;
SELECT * FROM db1aaaa.t1;
GRANT SELECT ON db1.* TO u2@localhost;
--echo # We can grant using a pattern which match the mattern we have.
GRANT SELECT ON db_.* TO u2@localhost;

# Negative test
--error ER_DBACCESS_DENIED_ERROR
GRANT SELECT ON `%db1`.* TO u2@localhost;

SET ROLE r4;
# Positive test
GRANT SELECT ON `secdb1`.* TO u2@localhost;
GRANT SELECT ON `secdb1`.`t1` TO u2@localhost;

# Negative test
--error ER_DBACCESS_DENIED_ERROR
GRANT SELECT ON `secdb%`.* TO u2@localhost;
--error ER_DBACCESS_DENIED_ERROR
GRANT SELECT ON `secdb_`.* TO u2@localhost;
--error ER_TABLEACCESS_DENIED_ERROR
GRANT SELECT ON `secdb_`.`t1` TO u2@localhost;

connection default;

GRANT INSERT ON `%db1`.* TO r1;

connection con1;
SET ROLE r1;

# Positive test
INSERT INTO dddddb1.t1 VALUES (1);

# Negative test
--error ER_TABLEACCESS_DENIED_ERROR
INSERT INTO db2.t1 VALUES (1);

--echo ++ Clean up
connection default;
DROP ROLE r1,r2,r3,r4;
DROP USER u1@localhost, u2@localhost;
DROP DATABASE db1;
DROP DATABASE db2;
DROP DATABASE db1aaaa;
DROP DATABASE dddddb1;
DROP DATABASE secdb1;
DROP DATABASE secdb2;
disconnect con1;

--disable_warnings
SET GLOBAL partial_revokes = @orig_partial_revokes;
--enable_warnings

--echo #
--echo # Empty hostnames not handled well
--echo #
CREATE USER 'u1'@'' IDENTIFIED BY '123';
GRANT SELECT ON *.* TO 'u1'@'';
CREATE USER 'r1'@'' IDENTIFIED BY '123';
CREATE USER 'r2'@'' IDENTIFIED BY '123';
GRANT ROLE_ADMIN ON *.* TO current_user();
GRANT 'r1'@'' TO 'u1'@'';
GRANT 'r2'@'' TO 'u1'@'';
SET DEFAULT ROLE 'r1'@'', 'r2'@'' TO 'u1'@'';
REVOKE 'r1'@'' FROM 'u1'@'';
REVOKE 'r2'@'' FROM 'u1'@'';
DROP USER 'u1'@'','r1'@'','r2'@'';

