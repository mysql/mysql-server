set @orig_partial_revokes = @@global.partial_revokes;
SET GLOBAL partial_revokes= OFF;
CREATE ROLE r1;
CREATE USER u1@localhost IDENTIFIED BY 'foo';
SHOW GRANTS FOR u1@localhost;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
CREATE DATABASE db1;
CREATE TABLE db1.t1 (c1 INT, c2 INT, c3 INT);
CREATE TABLE db1.t2 (c1 INT, c2 INT, c3 INT);
++ Test global level privileges
GRANT SELECT ON *.* TO r1;
GRANT r1 TO u1@localhost;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT SELECT ON *.* TO `u1`@`localhost`
GRANT `r1`@`%` TO `u1`@`localhost`
SET ROLE r1;
UPDATE db1.t1 SET c1=2;
ERROR 42000: UPDATE command denied to user 'u1'@'localhost' for table 't1'
SELECT * FROM db1.t1;
c1	c2	c3
REVOKE SELECT ON *.* FROM r1;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT `r1`@`%` TO `u1`@`localhost`
SET ROLE r1;
SHOW GRANTS FOR CURRENT_USER() USING r1;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT `r1`@`%` TO `u1`@`localhost`
SELECT * FROM db1.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
++ Test schema level privilege
GRANT SELECT ON db1.* TO r1;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SELECT ON `db1`.* TO `u1`@`localhost`
GRANT `r1`@`%` TO `u1`@`localhost`
SET ROLE r1;
UPDATE db1.t1 SET c1=2;
ERROR 42000: UPDATE command denied to user 'u1'@'localhost' for table 't1'
SELECT * FROM db1.t1;
c1	c2	c3
SET ROLE NONE;
SELECT * FROM db1.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
SET ROLE r1;
SELECT * FROM db1.t1;
c1	c2	c3
REVOKE SELECT ON db1.* FROM r1;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT `r1`@`%` TO `u1`@`localhost`
SET ROLE r1;
SELECT * FROM db1.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
++ Test table level privileges
GRANT SELECT ON db1.t1 TO r1;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SELECT ON `db1`.`t1` TO `u1`@`localhost`
GRANT `r1`@`%` TO `u1`@`localhost`
SET ROLE r1;
UPDATE db1.t1 SET c1=2;
ERROR 42000: UPDATE command denied to user 'u1'@'localhost' for table 't1'
SELECT * FROM db1.t2;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't2'
SELECT * FROM db1.t1;
c1	c2	c3
REVOKE SELECT ON db1.t1 FROM r1;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT `r1`@`%` TO `u1`@`localhost`
SET ROLE r1;
SELECT * FROM db1.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
++ Test column level privileges
GRANT SELECT(c1) ON db1.t1 TO r1;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SELECT (`c1`) ON `db1`.`t1` TO `u1`@`localhost`
GRANT `r1`@`%` TO `u1`@`localhost`
SET ROLE r1;
UPDATE db1.t1 SET c1=2;
ERROR 42000: UPDATE command denied to user 'u1'@'localhost' for table 't1'
SELECT c1 FROM db1.t2;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't2'
SELECT c2 FROM db1.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for column 'c2' in table 't1'
SELECT c1 FROM db1.t1;
c1
REVOKE SELECT(c1) ON db1.t1 FROM r1;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT `r1`@`%` TO `u1`@`localhost`
SET ROLE r1;
SELECT * FROM db1.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
++ Clean up
DROP ROLE r1;
DROP USER u1@localhost;
DROP DATABASE db1;
++++++++++++++++++++++++++++++++++++++++
++ 2 role depths
++++++++++++++++++++++++++++++++++++++++
CREATE ROLE r1;
CREATE ROLE r2;
CREATE USER u1@localhost IDENTIFIED BY 'foo';
SHOW GRANTS FOR u1@localhost;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
CREATE DATABASE db1;
CREATE DATABASE db2;
CREATE TABLE db1.t1 (c1 INT, c2 INT, c3 INT);
CREATE TABLE db1.t2 (c1 INT, c2 INT, c3 INT);
CREATE TABLE db1.t3 (c1 INT, c2 INT, c3 INT);
CREATE TABLE db2.t1 (c1 INT, c2 INT, c3 INT);
CREATE TABLE db2.t2 (c1 INT, c2 INT, c3 INT);
++ Test global level privileges
GRANT SELECT ON *.* TO r2;
GRANT SELECT ON db1.t2 TO r1 WITH GRANT OPTION;
GRANT r2 TO r1;
GRANT r1 TO u1@localhost;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT SELECT ON *.* TO `u1`@`localhost`
GRANT SELECT ON `db1`.`t2` TO `u1`@`localhost` WITH GRANT OPTION
GRANT `r1`@`%` TO `u1`@`localhost`
SET ROLE r1;
UPDATE db1.t1 SET c1=2;
ERROR 42000: UPDATE command denied to user 'u1'@'localhost' for table 't1'
UPDATE db1.t2 SET c1=2;
ERROR 42000: UPDATE command denied to user 'u1'@'localhost' for table 't2'
GRANT SELECT ON *.* TO u1@localhost;
ERROR 28000: Access denied for user 'u1'@'localhost' (using password: YES)
GRANT SELECT ON db1.* TO u1@localhost;
ERROR 42000: Access denied for user 'u1'@'localhost' to database 'db1'
GRANT SELECT ON db1.t1 TO u1@localhost;
ERROR 42000: GRANT command denied to user 'u1'@'localhost' for table 't1'
SELECT * FROM db1.t1;
c1	c2	c3
SELECT * FROM db1.t2;
c1	c2	c3
GRANT SELECT ON db1.t2 TO u1@localhost;
REVOKE SELECT ON db1.t2 FROM u1@localhost;
GRANT SELECT (c1) ON db1.t2 TO u1@localhost;
REVOKE SELECT (c1) ON db1.t2 FROM u1@localhost;
REVOKE SELECT ON *.* FROM r2;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SELECT ON `db1`.`t2` TO `u1`@`localhost` WITH GRANT OPTION
GRANT `r1`@`%` TO `u1`@`localhost`
SET ROLE r1;
SELECT * FROM db1.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
SELECT * FROM db1.t2;
c1	c2	c3
++ Test schema level privilege
GRANT SELECT ON db1.* TO r2;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SELECT ON `db1`.* TO `u1`@`localhost`
GRANT SELECT ON `db1`.`t2` TO `u1`@`localhost` WITH GRANT OPTION
GRANT `r1`@`%` TO `u1`@`localhost`
SET ROLE r1;
UPDATE db1.t1 SET c1=2;
ERROR 42000: UPDATE command denied to user 'u1'@'localhost' for table 't1'
UPDATE db1.t2 SET c1=2;
ERROR 42000: UPDATE command denied to user 'u1'@'localhost' for table 't2'
SELECT * FROM db2.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
SELECT * FROM db2.t2;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't2'
SELECT * FROM db1.t1;
c1	c2	c3
SELECT * FROM db1.t2;
c1	c2	c3
REVOKE SELECT ON db1.* FROM r2;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SELECT ON `db1`.`t2` TO `u1`@`localhost` WITH GRANT OPTION
GRANT `r1`@`%` TO `u1`@`localhost`
SET ROLE r1;
SELECT * FROM db1.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
++ Test table level privileges
GRANT SELECT ON db1.t1 TO r2;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SELECT ON `db1`.`t1` TO `u1`@`localhost`
GRANT SELECT ON `db1`.`t2` TO `u1`@`localhost` WITH GRANT OPTION
GRANT `r1`@`%` TO `u1`@`localhost`
SET ROLE r1;
UPDATE db1.t1 SET c1=2;
ERROR 42000: UPDATE command denied to user 'u1'@'localhost' for table 't1'
SELECT * FROM db1.t3;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't3'
SELECT * FROM db1.t1;
c1	c2	c3
SELECT * FROM db1.t2;
c1	c2	c3
REVOKE SELECT ON db1.t1 FROM r2;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SELECT ON `db1`.`t2` TO `u1`@`localhost` WITH GRANT OPTION
GRANT `r1`@`%` TO `u1`@`localhost`
SET ROLE r1;
SELECT * FROM db1.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
++ Test column level privileges
GRANT SELECT(c1) ON db1.t1 TO r2;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SELECT (`c1`) ON `db1`.`t1` TO `u1`@`localhost`
GRANT SELECT ON `db1`.`t2` TO `u1`@`localhost` WITH GRANT OPTION
GRANT `r1`@`%` TO `u1`@`localhost`
SET ROLE r1;
UPDATE db1.t1 SET c1=2;
ERROR 42000: UPDATE command denied to user 'u1'@'localhost' for table 't1'
SELECT c1 FROM db1.t3;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't3'
SELECT c2 FROM db1.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for column 'c2' in table 't1'
SELECT c1 FROM db1.t1;
c1
SELECT * FROM db1.t2;
c1	c2	c3
REVOKE SELECT(c1) ON db1.t1 FROM r2;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SELECT ON `db1`.`t2` TO `u1`@`localhost` WITH GRANT OPTION
GRANT `r1`@`%` TO `u1`@`localhost`
SET ROLE r1;
SELECT * FROM db1.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
++ Clean up
DROP ROLE r1;
DROP ROLE r2;
DROP USER u1@localhost;
DROP DATABASE db1;
DROP DATABASE db2;
++++++++++++++++++++++++++++++++++++++++
++ 1 role depths, two active roles
++++++++++++++++++++++++++++++++++++++++
CREATE ROLE r1;
CREATE ROLE r2;
CREATE USER u1@localhost IDENTIFIED BY 'foo';
SHOW GRANTS FOR u1@localhost;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
CREATE DATABASE db1;
CREATE DATABASE db2;
CREATE TABLE db1.t1 (c1 INT, c2 INT, c3 INT);
CREATE TABLE db1.t2 (c1 INT, c2 INT, c3 INT);
CREATE TABLE db1.t3 (c1 INT, c2 INT, c3 INT);
CREATE TABLE db2.t1 (c1 INT, c2 INT, c3 INT);
CREATE TABLE db2.t2 (c1 INT, c2 INT, c3 INT);
++ Test global level privileges
GRANT SELECT ON *.* TO r2;
GRANT SELECT ON db1.t2 TO r1;
GRANT r1 TO u1@localhost;
GRANT r2 TO u1@localhost;
SHOW GRANTS FOR u1@localhost USING r1,r2;
Grants for u1@localhost
GRANT SELECT ON *.* TO `u1`@`localhost`
GRANT SELECT ON `db1`.`t2` TO `u1`@`localhost`
GRANT `r1`@`%`,`r2`@`%` TO `u1`@`localhost`
SET ROLE r1,r2;
UPDATE db1.t1 SET c1=2;
ERROR 42000: UPDATE command denied to user 'u1'@'localhost' for table 't1'
UPDATE db1.t2 SET c1=2;
ERROR 42000: UPDATE command denied to user 'u1'@'localhost' for table 't2'
SELECT * FROM db1.t1;
c1	c2	c3
SELECT * FROM db1.t2;
c1	c2	c3
REVOKE SELECT ON *.* FROM r2;
SHOW GRANTS FOR u1@localhost USING r1,r2;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SELECT ON `db1`.`t2` TO `u1`@`localhost`
GRANT `r1`@`%`,`r2`@`%` TO `u1`@`localhost`
SET ROLE r1,r2;
SELECT * FROM db1.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
SELECT * FROM db1.t2;
c1	c2	c3
++ Test schema level privilege
GRANT SELECT ON db1.* TO r2;
SHOW GRANTS FOR u1@localhost USING r1,r2;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SELECT ON `db1`.* TO `u1`@`localhost`
GRANT SELECT ON `db1`.`t2` TO `u1`@`localhost`
GRANT `r1`@`%`,`r2`@`%` TO `u1`@`localhost`
SET ROLE r1,r2;
UPDATE db1.t1 SET c1=2;
ERROR 42000: UPDATE command denied to user 'u1'@'localhost' for table 't1'
UPDATE db1.t2 SET c1=2;
ERROR 42000: UPDATE command denied to user 'u1'@'localhost' for table 't2'
SELECT * FROM db2.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
SELECT * FROM db2.t2;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't2'
SELECT * FROM db1.t1;
c1	c2	c3
SELECT * FROM db1.t2;
c1	c2	c3
REVOKE SELECT ON db1.* FROM r2;
SHOW GRANTS FOR u1@localhost USING r1,r2;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SELECT ON `db1`.`t2` TO `u1`@`localhost`
GRANT `r1`@`%`,`r2`@`%` TO `u1`@`localhost`
SET ROLE r1,r2;
SELECT * FROM db1.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
++ Test table level privileges
GRANT SELECT ON db1.t1 TO r2;
SHOW GRANTS FOR u1@localhost USING r1,r2;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SELECT ON `db1`.`t1` TO `u1`@`localhost`
GRANT SELECT ON `db1`.`t2` TO `u1`@`localhost`
GRANT `r1`@`%`,`r2`@`%` TO `u1`@`localhost`
SET ROLE r1,r2;
UPDATE db1.t1 SET c1=2;
ERROR 42000: UPDATE command denied to user 'u1'@'localhost' for table 't1'
SELECT * FROM db1.t3;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't3'
SELECT * FROM db1.t1;
c1	c2	c3
SELECT * FROM db1.t2;
c1	c2	c3
REVOKE SELECT ON db1.t1 FROM r2;
SHOW GRANTS FOR u1@localhost USING r1,r2;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SELECT ON `db1`.`t2` TO `u1`@`localhost`
GRANT `r1`@`%`,`r2`@`%` TO `u1`@`localhost`
SET ROLE r1,r2;
SELECT * FROM db1.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
++ Test column level privileges
GRANT SELECT(c1) ON db1.t1 TO r2;
SHOW GRANTS FOR u1@localhost USING r1,r2;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SELECT (`c1`) ON `db1`.`t1` TO `u1`@`localhost`
GRANT SELECT ON `db1`.`t2` TO `u1`@`localhost`
GRANT `r1`@`%`,`r2`@`%` TO `u1`@`localhost`
SET ROLE r1,r2;
UPDATE db1.t1 SET c1=2;
ERROR 42000: UPDATE command denied to user 'u1'@'localhost' for table 't1'
SELECT c1 FROM db1.t3;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't3'
SELECT c2 FROM db1.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for column 'c2' in table 't1'
SELECT c1 FROM db1.t1;
c1
SELECT * FROM db1.t2;
c1	c2	c3
REVOKE SELECT(c1) ON db1.t1 FROM r2;
SHOW GRANTS FOR u1@localhost USING r1,r2;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SELECT ON `db1`.`t2` TO `u1`@`localhost`
GRANT `r1`@`%`,`r2`@`%` TO `u1`@`localhost`
SET ROLE r1,r2;
SELECT * FROM db1.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
++ Clean up
DROP ROLE r1;
DROP ROLE r2;
DROP USER u1@localhost;
SHOW STATUS LIKE '%Acl_cache%';
Variable_name	Value
Acl_cache_items_count	1
DROP DATABASE db1;
DROP DATABASE db2;
++++++++++++++++++++++++++++++++++++++++
++ 1 role depths, database patterns
++++++++++++++++++++++++++++++++++++++++
CREATE ROLE r1, r2, r3, r4;
CREATE USER u1@localhost IDENTIFIED BY 'foo';
CREATE USER u2@localhost IDENTIFIED BY 'foo';
SHOW GRANTS FOR u1@localhost;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
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
SET ROLE r1;
SHOW GRANTS;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT SELECT ON `db_`.* TO `u1`@`localhost`
GRANT `r1`@`%`,`r2`@`%`,`r3`@`%`,`r4`@`%` TO `u1`@`localhost`
SELECT * FROM db1.t1;
c1
1
2
3
SELECT * FROM db2.t1;
c2
1
2
3
SELECT * FROM db1aaaa.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
SELECT * FROM dddddb1.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
SET ROLE r2;
SELECT * FROM db1.t1;
c1
1
2
3
SELECT * FROM db2.t1;
c2
1
2
3
SELECT * FROM db1aaaa.t1;
c2
1
2
3
SELECT * FROM dddddb1.t1;
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't1'
GRANT SELECT ON db1.* TO u2@localhost;
ERROR 42000: Access denied for user 'u1'@'localhost' to database 'db1'
SET ROLE r3;
SELECT * FROM db1.t1;
c1
1
2
3
SELECT * FROM db2.t1;
c2
1
2
3
SELECT * FROM db1aaaa.t1;
c2
1
2
3
GRANT SELECT ON db1.* TO u2@localhost;
# We can grant using a pattern which match the mattern we have.
GRANT SELECT ON db_.* TO u2@localhost;
GRANT SELECT ON `%db1`.* TO u2@localhost;
ERROR 42000: Access denied for user 'u1'@'localhost' to database '%db1'
SET ROLE r4;
GRANT SELECT ON `secdb1`.* TO u2@localhost;
GRANT SELECT ON `secdb1`.`t1` TO u2@localhost;
GRANT SELECT ON `secdb%`.* TO u2@localhost;
ERROR 42000: Access denied for user 'u1'@'localhost' to database 'secdb%'
GRANT SELECT ON `secdb_`.* TO u2@localhost;
ERROR 42000: Access denied for user 'u1'@'localhost' to database 'secdb_'
GRANT SELECT ON `secdb_`.`t1` TO u2@localhost;
ERROR 42000: SELECT, GRANT command denied to user 'u1'@'localhost' for table 't1'
GRANT INSERT ON `%db1`.* TO r1;
SET ROLE r1;
INSERT INTO dddddb1.t1 VALUES (1);
INSERT INTO db2.t1 VALUES (1);
ERROR 42000: INSERT command denied to user 'u1'@'localhost' for table 't1'
++ Clean up
DROP ROLE r1,r2,r3,r4;
DROP USER u1@localhost, u2@localhost;
DROP DATABASE db1;
DROP DATABASE db2;
DROP DATABASE db1aaaa;
DROP DATABASE dddddb1;
DROP DATABASE secdb1;
DROP DATABASE secdb2;
SET GLOBAL partial_revokes = @orig_partial_revokes;
#
# Empty hostnames not handled well
#
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
