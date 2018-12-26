-- source include/have_case_insensitive_file_system.inc


#
# Bug#41049 does syntax "grant" case insensitive?
#
create database db1;
CREATE USER user_1@localhost, USER_1@localhost;
GRANT CREATE ON db1.* to user_1@localhost;
GRANT SELECT ON db1.* to USER_1@localhost;

connect (con1,localhost,user_1,,db1);
CREATE TABLE t1(f1 int);
--error 1142
SELECT * FROM t1;
connect (con2,localhost,USER_1,,db1);
SELECT * FROM t1;
--error 1142
CREATE TABLE t2(f1 int);

connection default;
disconnect con1;
disconnect con2;

REVOKE ALL PRIVILEGES, GRANT OPTION FROM user_1@localhost;
REVOKE ALL PRIVILEGES, GRANT OPTION FROM USER_1@localhost;
DROP USER user_1@localhost;
DROP USER USER_1@localhost;
DROP DATABASE db1;
use test;
