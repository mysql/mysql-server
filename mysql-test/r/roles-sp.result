CREATE ROLE r1;
CREATE USER u1@localhost IDENTIFIED BY 'foo';
GRANT r1 TO u1@localhost;
CREATE DATABASE db1;
CREATE DATABASE db2;
CREATE TABLE db1.t1 (c1 int);
CREATE TABLE db1.t2 (c1 int);
CREATE TABLE db2.t1 (c1 int);
CREATE TABLE db2.t2 (c1 int);
CREATE PROCEDURE db1.sp1()
SQL SECURITY DEFINER
BEGIN
SELECT * FROM db1.t1;
END//
CREATE PROCEDURE db2.sp1()
SQL SECURITY DEFINER
BEGIN
SELECT * FROM db2.t1;
END//
CREATE PROCEDURE test.sp1()
SQL SECURITY DEFINER
BEGIN
SELECT * FROM db1.t1;
END//
CREATE PROCEDURE db1.sp2()
SQL SECURITY DEFINER
BEGIN
SELECT * FROM db1.t2;
END//
CREATE PROCEDURE db1.sp3()
SQL SECURITY INVOKER
BEGIN
SELECT * FROM db1.t2;
END//
++ Test global level privileges
GRANT EXECUTE ON *.* TO r1;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT EXECUTE ON *.* TO `u1`@`localhost`
GRANT `r1`@`%` TO `u1`@`localhost`
SET ROLE r1;
++ Positive test
CALL db1.sp1();
c1
++ Negative test
CALL db1.sp3();
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't2'
++ Test revoke
REVOKE EXECUTE ON *.* FROM r1;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT `r1`@`%` TO `u1`@`localhost`
SET ROLE r1;
CALL db1.sp1();
ERROR 42000: execute command denied to user 'u1'@'localhost' for routine 'db1.sp1'
++ Test schema level privileges
GRANT EXECUTE ON db1.* TO r1;
SHOW GRANTS FOR u1@localhost USING r1;
Grants for u1@localhost
GRANT USAGE ON *.* TO `u1`@`localhost`
GRANT EXECUTE ON `db1`.* TO `u1`@`localhost`
GRANT `r1`@`%` TO `u1`@`localhost`
++ Positive test
CALL db1.sp1();
c1
CALL db1.sp2();
c1
++ Negative test
CALL db2.sp1();
ERROR 42000: execute command denied to user 'u1'@'localhost' for routine 'db2.sp1'
CALL db1.sp3();
ERROR 42000: SELECT command denied to user 'u1'@'localhost' for table 't2'
REVOKE EXECUTE ON db1.* FROM r1;
++ Test routine level privileges
GRANT EXECUTE ON PROCEDURE db1.sp1 TO r1;
++ Positive test
CALL db1.sp1();
c1
++ Negative test
CALL db1.sp2();
ERROR 42000: execute command denied to user 'u1'@'localhost' for routine 'db1.sp2'
CALL db2.sp1();
ERROR 42000: execute command denied to user 'u1'@'localhost' for routine 'db2.sp1'
CALL db1.sp3();
ERROR 42000: execute command denied to user 'u1'@'localhost' for routine 'db1.sp3'
++ Test Security invoker model
GRANT EXECUTE, SELECT ON db1.* TO r1;
++ Positive test
CALL db1.sp3();
c1
############################################################
## Make sure function privileges are aggregated correctly ##
############################################################
CREATE SCHEMA world;
USE world;
CREATE PROCEDURE world.proc_empty() BEGIN END;
CREATE FUNCTION world.func_plusone(i int) RETURNS INT DETERMINISTIC RETURN i+1;
CREATE FUNCTION world.func_plustwo(i int) RETURNS INT DETERMINISTIC RETURN i+2;
CREATE ROLE r_worldrou;
GRANT EXECUTE ON PROCEDURE world.proc_empty TO r_worldrou;
GRANT EXECUTE ON FUNCTION world.func_plusone TO r_worldrou;
CREATE USER u_worldrou@localhost IDENTIFIED BY 'xxx' DEFAULT ROLE r_worldrou;
SHOW GRANTS FOR u_worldrou@localhost USING r_worldrou;
Grants for u_worldrou@localhost
GRANT USAGE ON *.* TO `u_worldrou`@`localhost`
GRANT EXECUTE ON PROCEDURE `world`.`proc_empty` TO `u_worldrou`@`localhost`
GRANT EXECUTE ON FUNCTION `world`.`func_plusone` TO `u_worldrou`@`localhost`
GRANT `r_worldrou`@`%` TO `u_worldrou`@`localhost`
SELECT CURRENT_ROLE();
CURRENT_ROLE()
`r_worldrou`@`%`
CALL world.proc_empty();
SELECT world.func_plusone(1);
world.func_plusone(1)
2
SELECT world.func_plustwo(1);
ERROR 42000: execute command denied to user 'u_worldrou'@'localhost' for routine 'world.func_plustwo'
DROP SCHEMA world;
DROP ROLE r_worldrou;
DROP USER u_worldrou@localhost;
++ Clean up
DROP DATABASE db1;
DROP DATABASE db2;
DROP USER u1@localhost;
DROP ROLE r1;
DROP PROCEDURE test.sp1;
