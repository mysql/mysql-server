CREATE USER test1@localhost;
CREATE USER test2@localhost;
GRANT USAGE ON *.* TO test1@localhost;
GRANT USAGE ON *.* TO test2@localhost;
GRANT CREATE, DROP, INSERT, SELECT ON test.* TO test1@localhost;
GRANT CREATE, DROP, ALTER, UPDATE, INSERT, SELECT ON test.* TO test2@localhost;
connect (test1,localhost,test1,,test,MASTER_MYPORT,MASTER_MYSOCK);
SELECT current_user();
current_user()
test1@localhost
SHOW GRANTS FOR CURRENT_USER;
Grants for test1@localhost
GRANT USAGE ON *.* TO `test1`@`localhost`
GRANT SELECT, INSERT, CREATE, DROP ON `test`.* TO `test1`@`localhost`
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
ERROR 42000: ALTER command denied to user 'test1'@'localhost' for table 'tp'
disconnect test1;
connect (test2,localhost,test2,,test,MASTER_MYPORT,MASTER_MYSOCK);
USE test;
SELECT current_user();
current_user()
test2@localhost
SHOW GRANTS FOR CURRENT_USER;
Grants for test2@localhost
GRANT USAGE ON *.* TO `test2`@`localhost`
GRANT SELECT, INSERT, UPDATE, CREATE, DROP, ALTER ON `test`.* TO `test2`@`localhost`
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
SELECT * FROM t_10;
a	b
2	Two
4	Four
6	Six
8	Eight
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
a	b
1	One
3	Three
5	Five
9	Nine
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
SELECT * FROM t_10;
a	b
1	One
3	Three
5	Five
9	Nine
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
a	b
2	Two
4	Four
6	Six
8	Eight
ALTER TABLE tsp EXCHANGE PARTITION sp00 WITH TABLE tsp_00;
SELECT * FROM tsp_00;
a	b
SELECT * FROM tsp WHERE a BETWEEN 0 AND 10;
a	b
2	Two
4	Four
5	Five
6	Six
8	Eight
ALTER TABLE tsp EXCHANGE PARTITION sp00 WITH TABLE tsp_00;
SELECT * FROM tsp_00;
a	b
5	Five
SELECT * FROM tsp WHERE a BETWEEN 0 AND 10;
a	b
2	Two
4	Four
6	Six
8	Eight
disconnect test2;
connection default;
REVOKE ALTER ON test.* FROM test2@localhost;
connect (test2,localhost,test2,,test,MASTER_MYPORT,MASTER_MYSOCK);
USE test;
SELECT current_user();
current_user()
test2@localhost
SHOW GRANTS FOR CURRENT_USER;
Grants for test2@localhost
GRANT USAGE ON *.* TO `test2`@`localhost`
GRANT SELECT, INSERT, UPDATE, CREATE, DROP ON `test`.* TO `test2`@`localhost`
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
ERROR 42000: ALTER command denied to user 'test2'@'localhost' for table 'tp'
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
a	b
2	Two
4	Four
6	Six
8	Eight
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
ERROR 42000: ALTER command denied to user 'test2'@'localhost' for table 'tp'
ALTER TABLE tsp EXCHANGE PARTITION sp00 WITH TABLE tsp_00;
ERROR 42000: ALTER command denied to user 'test2'@'localhost' for table 'tsp'
ALTER TABLE tsp EXCHANGE PARTITION sp00 WITH TABLE tsp_00;
ERROR 42000: ALTER command denied to user 'test2'@'localhost' for table 'tsp'
connection default;
disconnect test2;
DROP TABLE IF EXISTS t_10;
DROP TABLE IF EXISTS t_100;
DROP TABLE IF EXISTS t_1000;
DROP TABLE IF EXISTS tp;
DROP TABLE IF EXISTS tsp;
DROP TABLE IF EXISTS tsp_00;
DROP TABLE IF EXISTS tsp_01;
DROP TABLE IF EXISTS tsp_02;
DROP TABLE IF EXISTS tsp_03;
DROP TABLE IF EXISTS tsp_04;
DROP TABLE IF EXISTS t_empty;
DROP TABLE IF EXISTS t_null;
DROP USER test1@localhost;
DROP USER test2@localhost;
