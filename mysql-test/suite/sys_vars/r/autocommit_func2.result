CREATE TABLE t1
(
id INT NOT NULL auto_increment,
PRIMARY KEY (id),
name varchar(30)
) ENGINE = INNODB;
SELECT @@global.autocommit;
@@global.autocommit
1
SELECT @@autocommit;
@@autocommit
1
INSERT into t1(name) values('Record_1');
INSERT into t1(name) values('Record_2');
SELECT * from t1;
id	name
1	Record_1
2	Record_2
ROLLBACK;
SELECT * from t1;
id	name
1	Record_1
2	Record_2
set @@global.autocommit = 1-@@global.autocommit;
set @@autocommit = 1-@@autocommit;
SELECT @@global.autocommit;
@@global.autocommit
0
SELECT @@autocommit;
@@autocommit
0
INSERT into t1(name) values('Record_1');
INSERT into t1(name) values('Record_2');
SELECT * from t1;
id	name
1	Record_1
2	Record_2
3	Record_1
4	Record_2
ROLLBACK;
SELECT * from t1;
id	name
1	Record_1
2	Record_2
DROP TABLE t1;
set @@global.autocommit = 1-@@global.autocommit;
