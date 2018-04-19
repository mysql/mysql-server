include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
SET SQL_LOG_BIN= 0;
CREATE TABLE t1(`a` INT, `b` DATE DEFAULT NULL,
`c` INT DEFAULT NULL,
PRIMARY KEY(`a`)) ENGINE=MyISAM DEFAULT CHARSET=LATIN1;
CREATE TABLE t2(`a` INT, `b` DATE DEFAULT NULL,
PRIMARY KEY(`a`)) ENGINE=MyISAM DEFAULT CHARSET=LATIN1;
CREATE TABLE t3(`a` INT, `b` DATE DEFAULT NULL,
PRIMARY KEY(`a`)) ENGINE=MyISAM DEFAULT CHARSET=LATIN1;
CREATE TABLE t4(`a` INT, `b` DATE DEFAULT NULL,
`c` INT DEFAULT NULL,
PRIMARY KEY(`a`)) ENGINE=MyISAM DEFAULT CHARSET=LATIN1;
SET SQL_LOG_BIN= 1;
CREATE TABLE t1(`a` INT, `b` DATE DEFAULT NULL,
`c` INT DEFAULT NULL,
PRIMARY KEY(`a`)) ENGINE=MyISAM DEFAULT CHARSET=LATIN1;
CREATE TABLE t2(`a` INT, `b` DATE DEFAULT NULL,
PRIMARY KEY(`a`)) ENGINE=MyISAM DEFAULT CHARSET=LATIN1;
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
CREATE TABLE t3(`a` INT, `b` DATE DEFAULT '0000-00-00',
`c` INT DEFAULT 500, 
PRIMARY KEY(`a`)) ENGINE=MyISAM DEFAULT CHARSET=LATIN1;
CREATE TABLE t4(`a` INT, `b` DATE DEFAULT '0000-00-00',
PRIMARY KEY(`a`)) ENGINE=MyISAM DEFAULT CHARSET=LATIN1;
SET sql_mode = default;
************* EXECUTION WITH INSERTS *************
INSERT INTO t1(a,b,c) VALUES (1, null, 1);
INSERT INTO t1(a,b,c) VALUES (2,'1111-11-11', 2);
INSERT INTO t1(a,b) VALUES (3, null);
INSERT INTO t1(a,c) VALUES (4, 4);
INSERT INTO t1(a) VALUES (5);
INSERT INTO t2(a,b) VALUES (1, null);
INSERT INTO t2(a,b) VALUES (2,'1111-11-11');
INSERT INTO t2(a) VALUES (3);
INSERT INTO t3(a,b) VALUES (1, null);
INSERT INTO t3(a,b) VALUES (2,'1111-11-11');
INSERT INTO t3(a) VALUES (3);
INSERT INTO t4(a,b,c) VALUES (1, null, 1);
INSERT INTO t4(a,b,c) VALUES (2,'1111-11-11', 2);
INSERT INTO t4(a,b) VALUES (3, null);
INSERT INTO t4(a,c) VALUES (4, 4);
INSERT INTO t4(a) VALUES (5);
************* SHOWING THE RESULT SETS WITH INSERTS *************
include/sync_slave_sql_with_master.inc
TABLES t1 and t2 must be equal otherwise an error will be thrown. 
include/diff_tables.inc [master:t1, slave:t1]
include/diff_tables.inc [master:t2, slave:t2]
TABLES t2 and t3 must be different.
SELECT * FROM t3 ORDER BY a;
a	b
1	NULL
2	1111-11-11
3	NULL
SELECT * FROM t3 ORDER BY a;
a	b	c
1	NULL	500
2	1111-11-11	500
3	NULL	500
SELECT * FROM t4 ORDER BY a;
a	b	c
1	NULL	1
2	1111-11-11	2
3	NULL	NULL
4	NULL	4
5	NULL	NULL
SELECT * FROM t4 ORDER BY a;
a	b
1	NULL
2	1111-11-11
3	NULL
4	NULL
5	NULL
************* EXECUTION WITH UPDATES and REPLACES *************
DELETE FROM t1;
INSERT INTO t1(a,b,c) VALUES (1,'1111-11-11', 1);
REPLACE INTO t1(a,b,c) VALUES (2,'1111-11-11', 2);
UPDATE t1 set b= NULL, c= 300 where a= 1;
REPLACE INTO t1(a,b,c) VALUES (2, NULL, 300);
************* SHOWING THE RESULT SETS WITH UPDATES and REPLACES *************
include/sync_slave_sql_with_master.inc
TABLES t1 and t2 must be equal otherwise an error will be thrown. 
include/diff_tables.inc [master:t1, slave:t1]
************* CLEANING *************
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;
include/sync_slave_sql_with_master.inc
SET SQL_LOG_BIN= 0;
CREATE TABLE t1 (`a` INT, `b` BIT DEFAULT NULL, `c` BIT DEFAULT NULL, 
PRIMARY KEY (`a`)) ENGINE= MyISAM;
SET SQL_LOG_BIN= 1;
CREATE TABLE t1 (`a` INT, `b` BIT DEFAULT b'01', `c` BIT DEFAULT NULL,
PRIMARY KEY (`a`)) ENGINE= MyISAM;
************* EXECUTION WITH INSERTS *************
INSERT INTO t1(a,b,c) VALUES (1, null, b'01');
INSERT INTO t1(a,b,c) VALUES (2,b'00', b'01');
INSERT INTO t1(a,b) VALUES (3, null);
INSERT INTO t1(a,c) VALUES (4, b'01');
INSERT INTO t1(a) VALUES (5);
************* SHOWING THE RESULT SETS WITH INSERTS *************
TABLES t1 and t2 must be different.
include/sync_slave_sql_with_master.inc
SELECT a,b+0,c+0 FROM t1 ORDER BY a;
a	b+0	c+0
1	NULL	1
2	0	1
3	NULL	NULL
4	NULL	1
5	NULL	NULL
SELECT a,b+0,c+0 FROM t1 ORDER BY a;
a	b+0	c+0
1	NULL	1
2	0	1
3	NULL	NULL
4	NULL	1
5	NULL	NULL
************* EXECUTION WITH UPDATES and REPLACES *************
DELETE FROM t1;
INSERT INTO t1(a,b,c) VALUES (1,b'00', b'01');
REPLACE INTO t1(a,b,c) VALUES (2,b'00',b'01');
UPDATE t1 set b= NULL, c= b'00' where a= 1;
REPLACE INTO t1(a,b,c) VALUES (2, NULL, b'00');
************* SHOWING THE RESULT SETS WITH UPDATES and REPLACES *************
TABLES t1 and t2 must be equal otherwise an error will be thrown. 
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:t1, slave:t1]
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
################################################################################
#                       NULL ---> NOT NULL (STRICT MODE)
#                    UNCOMMENT THIS AFTER FIXING BUG#43992
################################################################################
################################################################################
#                       NULL ---> NOT NULL (NON-STRICT MODE)
################################################################################
SET SQL_LOG_BIN= 0;
CREATE TABLE t1(`a` INT NOT NULL, `b` INT,
PRIMARY KEY(`a`)) ENGINE=MyISAM DEFAULT CHARSET=LATIN1;
CREATE TABLE t2(`a` INT NOT NULL, `b` INT,
PRIMARY KEY(`a`)) ENGINE=MyISAM DEFAULT CHARSET=LATIN1;
CREATE TABLE t3(`a` INT NOT NULL, `b` INT,
PRIMARY KEY(`a`)) ENGINE=MyISAM DEFAULT CHARSET=LATIN1;
SET SQL_LOG_BIN= 1;
CREATE TABLE t1(`a` INT NOT NULL, `b` INT NOT NULL, 
`c` INT NOT NULL,
PRIMARY KEY(`a`)) ENGINE=MyISAM DEFAULT CHARSET=LATIN1;
CREATE TABLE t2(`a` INT NOT NULL, `b` INT NOT NULL,
`c` INT, 
PRIMARY KEY(`a`)) ENGINE=MyISAM DEFAULT CHARSET=LATIN1;
CREATE TABLE t3(`a` INT NOT NULL, `b` INT NOT NULL,
`c` INT DEFAULT 500, 
PRIMARY KEY(`a`)) ENGINE=MyISAM DEFAULT CHARSET=LATIN1;
************* EXECUTION WITH INSERTS *************
INSERT INTO t1(a) VALUES (1);
INSERT INTO t1(a, b) VALUES (2, NULL);
INSERT INTO t1(a, b) VALUES (3, 1);
INSERT INTO t2(a) VALUES (1);
INSERT INTO t2(a, b) VALUES (2, NULL);
INSERT INTO t2(a, b) VALUES (3, 1);
INSERT INTO t3(a) VALUES (1);
INSERT INTO t3(a, b) VALUES (2, NULL);
INSERT INTO t3(a, b) VALUES (3, 1);
INSERT INTO t3(a, b) VALUES (4, 1);
REPLACE INTO t3(a, b) VALUES (5, null);
REPLACE INTO t3(a, b) VALUES (3, null);
UPDATE t3 SET b = NULL where a = 4;
************* SHOWING THE RESULT SETS *************
include/sync_slave_sql_with_master.inc
SELECT * FROM t1 ORDER BY a;
a	b
1	NULL
2	NULL
3	1
SELECT * FROM t1 ORDER BY a;
a	b	c
1	0	0
2	0	0
3	1	0
SELECT * FROM t2 ORDER BY a;
a	b
1	NULL
2	NULL
3	1
SELECT * FROM t2 ORDER BY a;
a	b	c
1	0	NULL
2	0	NULL
3	1	NULL
SELECT * FROM t3 ORDER BY a;
a	b
1	NULL
2	NULL
3	NULL
4	NULL
5	NULL
SELECT * FROM t3 ORDER BY a;
a	b	c
1	0	500
2	0	500
3	0	500
4	0	500
5	0	500
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
