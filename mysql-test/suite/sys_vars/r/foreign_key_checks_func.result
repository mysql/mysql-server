'#--------------------FN_DYNVARS_032_01-------------------------#'
SET @@session.foreign_key_checks = 0;
'connect (con1,localhost,root,,,,)'
'connection con1'
SELECT @@session.foreign_key_checks;
@@session.foreign_key_checks
1
SET @@session.foreign_key_checks = 1;
'connect (con2,localhost,root,,,,)'
'connection con2'
SELECT @@session.foreign_key_checks;
@@session.foreign_key_checks
1
'#--------------------FN_DYNVARS_032_02-------------------------#'
'connection con1'
DROP TABLE IF EXISTS t1,t2;
CREATE TABLE t1(a INT PRIMARY KEY)ENGINE = INNODB;
CREATE TABLE t2(a INT PRIMARY KEY,b INT)ENGINE = INNODB;
ALTER TABLE t2
ADD CONSTRAINT fk FOREIGN KEY (b) REFERENCES t1 (a);
'---Check when foreign_key_checks is enabled---'
SET @@session.foreign_key_checks = 1;
INSERT INTO t1 values (1),(2),(3);
INSERT INTO t2 values (10,1);
INSERT INTO t2 values (20,22);
ERROR 23000: Cannot add or update a child row: a foreign key constraint fails (`test`.`t2`, CONSTRAINT `fk` FOREIGN KEY (`b`) REFERENCES `t1` (`a`))
'---Check when foreign_key_checks is disabled---'
TRUNCATE t1;
ERROR 42000: Cannot truncate a table referenced in a foreign key constraint (`test`.`t2`, CONSTRAINT `fk`)
SET @@session.foreign_key_checks = 0;
TRUNCATE t1;
TRUNCATE t2;
INSERT INTO t1 values (1),(2),(3);
INSERT INTO t2 values (10,1);
INSERT INTO t2 values (20,4);
'try enabling foreign_key_checks again';
SET @@session.foreign_key_checks = 1;
UPDATE t2 SET b=4 where a=20;
'Check when foreign_key_checks is enabled and FK constraint is re-created'
SET @@session.foreign_key_checks = 0;
TRUNCATE t2;
TRUNCATE t1;
INSERT INTO t1 values (1),(2),(3);
INSERT INTO t2 values (10,1),(20,4);
ALTER TABLE t2 DROP FOREIGN KEY fk;
SET @@session.foreign_key_checks = 1;
DELETE FROM t2 WHERE b not in (SELECT a from t1);
ALTER TABLE t2
ADD CONSTRAINT fk FOREIGN KEY (b) REFERENCES t1 (a);
INSERT INTO t2 values (20,2);
SELECT * from t2;
a	b
10	1
20	2
DROP TABLE IF EXISTS t2;
DROP TABLE IF EXISTS t1;
