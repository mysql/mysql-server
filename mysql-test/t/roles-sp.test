CREATE ROLE r1;
CREATE USER u1@localhost IDENTIFIED BY 'foo';
GRANT r1 TO u1@localhost;
CREATE DATABASE db1;
CREATE DATABASE db2;

CREATE TABLE db1.t1 (c1 int);
CREATE TABLE db1.t2 (c1 int);
CREATE TABLE db2.t1 (c1 int);
CREATE TABLE db2.t2 (c1 int);

DELIMITER //;
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
DELIMITER ;//


--echo ++ Test global level privileges
GRANT EXECUTE ON *.* TO r1;
SHOW GRANTS FOR u1@localhost USING r1;

connect(con1, localhost, u1, foo, test);
SET ROLE r1;
--echo ++ Positive test
CALL db1.sp1();

--echo ++ Negative test
--error ER_TABLEACCESS_DENIED_ERROR
CALL db1.sp3();

--echo ++ Test revoke
connection default;
REVOKE EXECUTE ON *.* FROM r1;
SHOW GRANTS FOR u1@localhost USING r1;
connection con1;
SET ROLE r1;
--error ER_PROCACCESS_DENIED_ERROR
CALL db1.sp1();

--echo ++ Test schema level privileges
connection default;
GRANT EXECUTE ON db1.* TO r1;
SHOW GRANTS FOR u1@localhost USING r1;
connection con1;

--echo ++ Positive test
CALL db1.sp1();
CALL db1.sp2();

--echo ++ Negative test
--error ER_PROCACCESS_DENIED_ERROR
CALL db2.sp1();
--error ER_TABLEACCESS_DENIED_ERROR
CALL db1.sp3();

connection default;
REVOKE EXECUTE ON db1.* FROM r1;

--echo ++ Test routine level privileges
GRANT EXECUTE ON PROCEDURE db1.sp1 TO r1;
connection con1;

--echo ++ Positive test
CALL db1.sp1();

--echo ++ Negative test
--error ER_PROCACCESS_DENIED_ERROR
CALL db1.sp2();
--error ER_PROCACCESS_DENIED_ERROR
CALL db2.sp1();
--error ER_PROCACCESS_DENIED_ERROR
CALL db1.sp3();

--echo ++ Test Security invoker model
connection default;
GRANT EXECUTE, SELECT ON db1.* TO r1;
connection con1;

--echo ++ Positive test
CALL db1.sp3();

connection default;
--echo ############################################################
--echo ## Make sure function privileges are aggregated correctly ##
--echo ############################################################
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

connect(con_world,localhost,u_worldrou,xxx,world);
SELECT CURRENT_ROLE();
CALL world.proc_empty();
SELECT world.func_plusone(1);
--error ER_PROCACCESS_DENIED_ERROR
SELECT world.func_plustwo(1);

connection default;
disconnect con_world;
DROP SCHEMA world;
DROP ROLE r_worldrou;
DROP USER u_worldrou@localhost;


--echo ++ Clean up
connection default;
DROP DATABASE db1;
DROP DATABASE db2;
DROP USER u1@localhost;
DROP ROLE r1;
DROP PROCEDURE test.sp1;
disconnect con1;

