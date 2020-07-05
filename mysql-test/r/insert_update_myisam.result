create table t1 (a int not null unique) engine=myisam;
insert into t1 values (1),(2);
insert ignore into t1 select 1 on duplicate key update a=2;
Warnings:
Warning	1062	Duplicate entry '2' for key 't1.a'
select * from t1;
a
1
2
insert ignore into t1 select a from t1 as t2 on duplicate key update a=t1.a+1 ;
Warnings:
Warning	1062	Duplicate entry '2' for key 't1.a'
select * from t1;
a
1
3
insert into t1 select 1 on duplicate key update a=2;
select * from t1;
a
2
3
insert into t1 select a from t1 on duplicate key update a=a+1 ;
ERROR 23000: Column 'a' in field list is ambiguous
insert ignore into t1 select a from t1 on duplicate key update a=t1.a+1 ;
ERROR 23000: Column 't1.a' in field list is ambiguous
drop table t1;
CREATE TABLE t1 (
a BIGINT(20) NOT NULL DEFAULT 0,
PRIMARY KEY  (a)
) ENGINE=MyISAM;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
INSERT INTO t1 ( a ) SELECT 0 ON DUPLICATE KEY UPDATE a = a + VALUES (a) ;
Warnings:
Warning	1287	'VALUES function' is deprecated and will be removed in a future release. Please use an alias (INSERT INTO ... VALUES (...) AS alias) and replace VALUES(col) in the ON DUPLICATE KEY UPDATE clause with alias.col instead
DROP TABLE t1;
SET SQL_MODE='NO_AUTO_VALUE_ON_ZERO';
CREATE TABLE `t1` (
`id` int(11) PRIMARY KEY auto_increment,
`f1` varchar(10) NOT NULL UNIQUE
)engine=myisam;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
INSERT IGNORE INTO t1 (f1) VALUES ("test1")
ON DUPLICATE KEY UPDATE id=LAST_INSERT_ID(id);
INSERT IGNORE INTO t1 (f1) VALUES ("test1")
ON DUPLICATE KEY UPDATE id=LAST_INSERT_ID(id);
SELECT LAST_INSERT_ID();
LAST_INSERT_ID()
1
SELECT * FROM t1;
id	f1
1	test1
INSERT IGNORE INTO t1 (f1) VALUES ("test2")
ON DUPLICATE KEY UPDATE id=LAST_INSERT_ID(id);
SELECT * FROM t1;
id	f1
1	test1
2	test2
INSERT IGNORE INTO t1 (f1) VALUES ("test2")
ON DUPLICATE KEY UPDATE id=LAST_INSERT_ID(id);
SELECT LAST_INSERT_ID();
LAST_INSERT_ID()
2
SELECT * FROM t1;
id	f1
1	test1
2	test2
INSERT IGNORE INTO t1 (f1) VALUES ("test3")
ON DUPLICATE KEY UPDATE id=LAST_INSERT_ID(id);
SELECT LAST_INSERT_ID();
LAST_INSERT_ID()
3
SELECT * FROM t1;
id	f1
1	test1
2	test2
3	test3
DROP TABLE t1;
CREATE TABLE `t1` (
`id` int(11) PRIMARY KEY auto_increment,
`f1` varchar(10) NOT NULL UNIQUE
);
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
INSERT IGNORE INTO t1 (f1) VALUES ("test1")
ON DUPLICATE KEY UPDATE id=LAST_INSERT_ID(id);
SELECT LAST_INSERT_ID();
LAST_INSERT_ID()
1
SELECT * FROM t1;
id	f1
1	test1
INSERT IGNORE INTO t1 (f1) VALUES ("test1"),("test4")
ON DUPLICATE KEY UPDATE id=LAST_INSERT_ID(id);
SELECT LAST_INSERT_ID();
LAST_INSERT_ID()
2
SELECT * FROM t1;
id	f1
1	test1
2	test4
DROP TABLE t1;
