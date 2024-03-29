CREATE ROLE r1;

CREATE USER u1@localhost IDENTIFIED BY 'foo';
SHOW GRANTS FOR u1@localhost;

CREATE DATABASE db2;
GRANT CREATE ON *.* TO r1;
GRANT r1 TO u1@localhost;
SHOW GRANTS FOR u1@localhost USING r1;

connect(con1, localhost, u1, foo, test);
SET ROLE r1;

# test positive
CREATE DATABASE db1;

# test negative
--error ER_DBACCESS_DENIED_ERROR
DROP DATABASE db1;

connection default;
REVOKE CREATE ON *.* FROM r1;
GRANT CREATE ON db1.* TO r1;
connection con1;
SELECT CURRENT_USER(), CURRENT_ROLE();

# negative test
--error ER_TABLEACCESS_DENIED_ERROR
CREATE TABLE db2.test (c1 int);
--error ER_DBACCESS_DENIED_ERROR
CREATE DATABASE db3;

connection default;
GRANT CREATE, DROP, INSERT ON db2.* TO r1;
connection con1;
# positive test
CREATE TABLE db2.t1 (c1 INT);
DROP TABLE db2.t1;
CREATE TABLE db2.t1 (c1 INT);
INSERT INTO db2.t1 VALUES (1),(2),(3);

# negative test
--error ER_TABLEACCESS_DENIED_ERROR
SELECT * FROM db2.t1;
--error ER_TABLEACCESS_DENIED_ERROR
SELECT c1 FROM db2.t1;
--error ER_TABLEACCESS_DENIED_ERROR
UPDATE db2.t1 SET c1=1;
--error ER_TABLEACCESS_DENIED_ERROR
ALTER TABLE db1.t1 ADD COLUMN (c2 INT);
--error ER_DBACCESS_DENIED_ERROR
DROP DATABASE db1;

--echo ++ Clean up
connection default;
DROP ROLE r1;
#DROP ROLE r2;
DROP USER u1@localhost;
SHOW STATUS LIKE '%Acl_cache%';
DROP DATABASE db1;
DROP DATABASE db2;

