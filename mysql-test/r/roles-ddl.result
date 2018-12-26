CREATE ROLE r1;
CREATE USER u1@localhost IDENTIFIED BY 'foo';
SHOW GRANTS FOR u1@localhost;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
CREATE DATABASE db2;
GRANT CREATE ON *.* TO r1;
GRANT r1 TO u1@localhost;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT CREATE ON *.* TO `u1`@`localhost`
GRANT `r1`@`%` TO `u1`@`localhost`
SET ROLE r1;
CREATE DATABASE db1;
DROP DATABASE db1;
ERROR 42000: Access denied for user 'u1'@'localhost' to database 'db1'
REVOKE CREATE ON *.* FROM r1;
GRANT CREATE ON db1.* TO r1;
SELECT CURRENT_USER(), CURRENT_ROLE();
CURRENT_USER()	CURRENT_ROLE()
u1@localhost	`r1`@`%`
CREATE TABLE db2.test (c1 int);
ERROR 42000: CREATE command denied to user 'u1'@'localhost' for table 'test'
CREATE DATABASE db3;
ERROR 42000: Access denied for user 'u1'@'localhost' to database 'db3'
GRANT CREATE, DROP, INSERT ON db2.* TO r1;
CREATE TABLE db2.t1 (c1 INT);
DROP TABLE db2.t1;
CREATE TABLE db2.t1 (c1 INT);
INSERT INTO db2.t1 VALUES (1),(2),(3);
SELECT * FROM db2.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
SELECT c1 FROM db2.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
UPDATE db2.t1 SET c1=1;
ERROR 42000: UPDATE command denied to user 'u1'@'localhost' for table 't1'
ALTER TABLE db1.t1 ADD COLUMN (c2 INT);
ERROR 42000: ALTER command denied to user 'u1'@'localhost' for table 't1'
DROP DATABASE db1;
ERROR 42000: Access denied for user 'u1'@'localhost' to database 'db1'
++ Clean up
DROP ROLE r1;
DROP USER u1@localhost;
SHOW STATUS LIKE '%Acl_cache%';
Variable_name	Value
Acl_cache_items_count	1
DROP DATABASE db1;
DROP DATABASE db2;
