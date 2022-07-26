include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
**** Test case for BUG#16487 ****
**** Master ****
CREATE TABLE test.t4 (a int);
CREATE TABLE test.t1 (a int);
UPDATE test.t4 NATURAL JOIN test.t1 SET t1.a=5;
**** Slave ****
SELECT * FROM t4;
a
DROP TABLE t1;
DROP TABLE t4;
**** Test case for BUG#25482 ****
**** Adding GRANTS on master ****
create table test.t1(a int);
create table test.t4(a int);
CREATE USER mysqltest1@localhost;
GRANT SELECT ON test.t1 TO mysqltest1@localhost;
CREATE USER mysqltest2@localhost;
GRANT INSERT ON test.t4 TO mysqltest2@localhost;
GRANT select, update, insert, references on t1
to mysqltest2@localhost;
CREATE USER mysqltest3@localhost;
GRANT SELECT ON test.* TO mysqltest3@localhost;
GRANT INSERT ON test.t4 TO mysqltest3@localhost;
GRANT select(a), update(a), insert(a), references(a) on t4
to mysqltest3@localhost;
create database mysqltest2;
create table mysqltest2.t2 (id int);
CREATE USER mysqltest4@localhost IDENTIFIED BY 'pass';
GRANT SELECT ON mysqltest2.t2 TO mysqltest4@localhost;
insert ignore into mysql.user (user, host) values ("mysqltest5", "somehost");
Warnings:
Warning	1364	Field 'ssl_cipher' doesn't have a default value
Warning	1364	Field 'x509_issuer' doesn't have a default value
Warning	1364	Field 'x509_subject' doesn't have a default value
CREATE USER mysqltest6@localhost;
GRANT SELECT ON *.* TO mysqltest6@localhost;
GRANT INSERT ON *.* TO mysqltest6@localhost;
GRANT INSERT ON test.* TO mysqltest6@localhost;
GRANT INSERT ON test.t1 TO mysqltest6@localhost;
show grants for mysqltest1@localhost;
Grants for mysqltest1@localhost
GRANT SELECT ON `test`.`t1` TO `mysqltest1`@`localhost`
GRANT USAGE ON *.* TO `mysqltest1`@`localhost`
show grants for mysqltest2@localhost;
Grants for mysqltest2@localhost
GRANT INSERT ON `test`.`t4` TO `mysqltest2`@`localhost`
GRANT SELECT, INSERT, UPDATE, REFERENCES ON `test`.`t1` TO `mysqltest2`@`localhost`
GRANT USAGE ON *.* TO `mysqltest2`@`localhost`
show grants for mysqltest3@localhost;
Grants for mysqltest3@localhost
GRANT SELECT (`a`), INSERT, INSERT (`a`), UPDATE (`a`), REFERENCES (`a`) ON `test`.`t4` TO `mysqltest3`@`localhost`
GRANT SELECT ON `test`.* TO `mysqltest3`@`localhost`
GRANT USAGE ON *.* TO `mysqltest3`@`localhost`
show grants for mysqltest4@localhost;
Grants for mysqltest4@localhost
GRANT SELECT ON `mysqltest2`.`t2` TO `mysqltest4`@`localhost`
GRANT USAGE ON *.* TO `mysqltest4`@`localhost`
show grants for mysqltest6@localhost;
Grants for mysqltest6@localhost
GRANT INSERT ON `test`.* TO `mysqltest6`@`localhost`
GRANT INSERT ON `test`.`t1` TO `mysqltest6`@`localhost`
GRANT SELECT, INSERT ON *.* TO `mysqltest6`@`localhost`
flush privileges;
show grants for mysqltest5@somehost;
Grants for mysqltest5@somehost
GRANT USAGE ON *.* TO `mysqltest5`@`somehost`
**** Checking grants on slave ****
show grants for mysqltest2@localhost;
Grants for mysqltest2@localhost
GRANT USAGE ON *.* TO `mysqltest2`@`localhost`
GRANT INSERT ON `test`.`t4` TO `mysqltest2`@`localhost`
show grants for mysqltest3@localhost;
Grants for mysqltest3@localhost
GRANT USAGE ON *.* TO `mysqltest3`@`localhost`
GRANT SELECT ON `test`.* TO `mysqltest3`@`localhost`
GRANT SELECT (`a`), INSERT, INSERT (`a`), UPDATE (`a`), REFERENCES (`a`) ON `test`.`t4` TO `mysqltest3`@`localhost`
show grants for mysqltest4@localhost;
Grants for mysqltest4@localhost
GRANT USAGE ON *.* TO `mysqltest4`@`localhost`
GRANT SELECT ON `mysqltest2`.`t2` TO `mysqltest4`@`localhost`
show grants for mysqltest5@somehost;
Grants for mysqltest5@somehost
GRANT USAGE ON *.* TO `mysqltest5`@`somehost`
show grants for mysqltest6@localhost;
Grants for mysqltest6@localhost
GRANT SELECT, INSERT ON *.* TO `mysqltest6`@`localhost`
GRANT INSERT ON `test`.* TO `mysqltest6`@`localhost`
show grants for mysqltest1@localhost;
Grants for mysqltest1@localhost
GRANT USAGE ON *.* TO `mysqltest1`@`localhost`
**** Revoking grants on master ****
REVOKE SELECT ON test.t1 FROM mysqltest1@localhost;
REVOKE SELECT ON mysqltest2.t2 FROM mysqltest4@localhost;
REVOKE select(a) on t4
from mysqltest3@localhost;
show grants for mysqltest1@localhost;
Grants for mysqltest1@localhost
GRANT USAGE ON *.* TO `mysqltest1`@`localhost`
show grants for mysqltest3@localhost;
Grants for mysqltest3@localhost
GRANT USAGE ON *.* TO `mysqltest3`@`localhost`
GRANT SELECT ON `test`.* TO `mysqltest3`@`localhost`
GRANT INSERT, INSERT (`a`), UPDATE (`a`), REFERENCES (`a`) ON `test`.`t4` TO `mysqltest3`@`localhost`
show grants for mysqltest4@localhost;
Grants for mysqltest4@localhost
GRANT USAGE ON *.* TO `mysqltest4`@`localhost`
**** Checking grants on slave ****
show grants for mysqltest1@localhost;
Grants for mysqltest1@localhost
GRANT USAGE ON *.* TO `mysqltest1`@`localhost`
show grants for mysqltest3@localhost;
Grants for mysqltest3@localhost
GRANT USAGE ON *.* TO `mysqltest3`@`localhost`
GRANT SELECT ON `test`.* TO `mysqltest3`@`localhost`
GRANT INSERT, INSERT (`a`), UPDATE (`a`), REFERENCES (`a`) ON `test`.`t4` TO `mysqltest3`@`localhost`
show grants for mysqltest4@localhost;
Grants for mysqltest4@localhost
GRANT USAGE ON *.* TO `mysqltest4`@`localhost`
set global replica_exec_mode='IDEMPOTENT';
call mtr.add_suppression("Slave SQL.*Could not execute Delete_rows event on table mysql.* Error_code: 1032");
drop table t1, mysqltest2.t2;
drop table t4;
drop database mysqltest2;
delete from mysql.user where user like "mysqltest%";
delete from mysql.db where user like "mysqltest%";
delete from mysql.columns_priv where user like "mysqltest%";
delete from mysql.tables_priv where user like "mysqltest%";
delete from mysql.tables_priv where user like "mysqltest%";
DROP TABLE IF EXISTS t5;
CREATE TABLE t5 (
word varchar(50) collate utf8_unicode_ci NOT NULL default ''
) DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;
Warnings:
Warning	3778	'utf8mb3_unicode_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	3778	'utf8mb3_unicode_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SET @@session.character_set_client=33,@@session.collation_connection=192;
Warnings:
Warning	1287	'utf8mb3' is deprecated and will be removed in a future release. Please use utf8mb4 instead
Warning	3778	'utf8mb3_unicode_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
CREATE TEMPORARY TABLE tmptbl504451f4258$1 (id INT NOT NULL) ENGINE=MEMORY;
INSERT INTO t5 (word)  VALUES ('TEST’');
SELECT HEX(word) FROM t5;
HEX(word)
54455354E28099
set @@global.replica_exec_mode= default;
SELECT HEX(word) FROM t5;
HEX(word)
54455354E28099
SELECT * FROM tmptbl504451f4258$1;
ERROR 42S02: Table 'test.tmptbl504451f4258$1' doesn't exist
DROP TABLE t5;
include/rpl_end.inc
