
CREATE TABLE t1
(
id INT NOT NULL auto_increment,
PRIMARY KEY (id),
name varchar(30)
) ENGINE = INNODB;

SELECT @@global.autocommit;
SELECT @@autocommit;
INSERT into t1(name) values('Record_1');
INSERT into t1(name) values('Record_2');
SELECT * from t1;
ROLLBACK;
SELECT * from t1;

set @@global.autocommit = 1-@@global.autocommit;
set @@autocommit = 1-@@autocommit;
SELECT @@global.autocommit;
SELECT @@autocommit;
INSERT into t1(name) values('Record_1');
INSERT into t1(name) values('Record_2');
SELECT * from t1;
ROLLBACK;
SELECT * from t1;

DROP TABLE t1;
set @@global.autocommit = 1-@@global.autocommit;
