drop table if exists t1,t1aa,t2aa;
DROP DATABASE IF EXISTS testdb;
CREATE DATABASE testdb;
CREATE TABLE testdb.t1 (
c1 INT,
c3 INT NOT NULL AUTO_INCREMENT PRIMARY KEY);
CREATE VIEW testdb.v1 AS
SELECT * FROM testdb.t1;
CREATE USER 'show_view_tbl'@'localhost', 'select_only_c1'@localhost;
GRANT CREATE VIEW, SHOW VIEW ON testdb.v1 TO 'show_view_tbl'@'localhost';

GRANT SELECT(c1) on testdb.v1 to 'select_only_c1'@localhost;
SHOW GRANTS FOR 'select_only_c1'@'localhost';
Grants for select_only_c1@localhost
GRANT USAGE ON *.* TO `select_only_c1`@`localhost`
GRANT SELECT (`c1`) ON `testdb`.`v1` TO `select_only_c1`@`localhost`

"after fix privs"
SHOW GRANTS FOR 'show_view_tbl'@'localhost';
Grants for show_view_tbl@localhost
GRANT USAGE ON *.* TO `show_view_tbl`@`localhost`
GRANT CREATE VIEW, SHOW VIEW ON `testdb`.`v1` TO `show_view_tbl`@`localhost`

SHOW GRANTS FOR 'select_only_c1'@'localhost';
Grants for select_only_c1@localhost
GRANT USAGE ON *.* TO `select_only_c1`@`localhost`
GRANT SELECT (`c1`) ON `testdb`.`v1` TO `select_only_c1`@`localhost`

DROP USER 'show_view_tbl'@'localhost';
DROP USER 'select_only_c1'@'localhost';
DROP VIEW testdb.v1;
DROP TABLE testdb.t1;
DROP DATABASE testdb;
