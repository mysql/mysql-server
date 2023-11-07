SET timestamp=1;
CREATE DATABASE test1;
use test1;
#
# A) Test displaying create_options from I_S.TABLES.CREATE_OPTIONS
#
CREATE TABLE t1 (a INT NOT NULL,b INT NOT NULL, PRIMARY KEY USING BTREE (a))
ENGINE=MYISAM COMMENT="TESTING" PACK_KEYS=1 DELAY_KEY_WRITE=1
STATS_PERSISTENT=1 CHECKSUM=1 MIN_ROWS=1 MAX_ROWS=100;
CREATE TABLE t2 (a INT NOT NULL,b INT NOT NULL, PRIMARY KEY USING BTREE (a))
ENGINE=MYISAM COMMENT="TESTING" PACK_KEYS=0 DELAY_KEY_WRITE=1
STATS_PERSISTENT=1 CHECKSUM=1 MIN_ROWS=1 MAX_ROWS=100;
SELECT CREATE_OPTIONS FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_NAME='t1' OR TABLE_NAME='t2';
CREATE_OPTIONS
max_rows=100 min_rows=1 stats_persistent=1 pack_keys=0 checksum=1 delay_key_write=1
max_rows=100 min_rows=1 stats_persistent=1 pack_keys=1 checksum=1 delay_key_write=1
DROP TABLE t1;
DROP TABLE t2;
#
# C) Test SHOW COLUMNS/KEYS on temporary tables.
#
CREATE TEMPORARY TABLE t1 (
a INTEGER NOT NULL DEFAULT 1,
b INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY,
c INTEGER,
d INTEGER GENERATED ALWAYS AS (a * 2) STORED,
e POINT NOT NULL SRID 0,
f CHAR(30),
g INT,
h TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
KEY(g), SPATIAL KEY(e), FULLTEXT(f)) engine=myisam;
CREATE INDEX b_index USING btree ON t1( b );
CREATE INDEX h_index USING hash ON t1( c );
Warnings:
Note	3502	This storage engine does not support the HASH index algorithm, storage engine default was used instead.
CREATE INDEX g_index ON t1( f(4) ) COMMENT 'ABC';
CREATE INDEX r_index USING rtree ON t1( a ) USING btree;
SHOW FULL COLUMNS FROM t1;
Field	Type	Collation	Null	Key	Default	Extra	Privileges	Comment
a	int	NULL	NO	MUL	1	NULL	select,insert,update,references	
b	int	NULL	NO	PRI	NULL	auto_increment	select,insert,update,references	
c	int	NULL	YES	MUL	NULL	NULL	select,insert,update,references	
d	int	NULL	YES		NULL	STORED GENERATED	select,insert,update,references	
e	point	NULL	NO	MUL	NULL	NULL	select,insert,update,references	
f	char(30)	utf8mb4_0900_ai_ci	YES	MUL	NULL	NULL	select,insert,update,references	
g	int	NULL	YES	MUL	NULL	NULL	select,insert,update,references	
h	timestamp	NULL	NO		CURRENT_TIMESTAMP	on update CURRENT_TIMESTAMP	select,insert,update,references	
SHOW KEYS FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	0	PRIMARY	1	b	A	0	NULL	NULL		BTREE			YES	NULL
t1	1	g	1	g	A	NULL	NULL	NULL	YES	BTREE			YES	NULL
t1	1	e	1	e	A	NULL	32	NULL		SPATIAL			YES	NULL
t1	1	b_index	1	b	A	NULL	NULL	NULL		BTREE			YES	NULL
t1	1	h_index	1	c	A	NULL	NULL	NULL	YES	BTREE			YES	NULL
t1	1	g_index	1	f	A	NULL	4	NULL	YES	BTREE		ABC	YES	NULL
t1	1	r_index	1	a	A	NULL	NULL	NULL		BTREE			YES	NULL
t1	1	f	1	f	NULL	NULL	NULL	NULL	YES	FULLTEXT			YES	NULL
SHOW FULL COLUMNS FROM t1 WHERE FIELD = 'a' OR FIELD = 'b';
Field	Type	Collation	Null	Key	Default	Extra	Privileges	Comment
a	int	NULL	NO	MUL	1	NULL	select,insert,update,references	
b	int	NULL	NO	PRI	NULL	auto_increment	select,insert,update,references	
SHOW KEYS FROM t1 WHERE KEY_NAME = 'r_index' OR KEY_NAME = 'h_index';
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	h_index	1	c	A	NULL	NULL	NULL	YES	BTREE			YES	NULL
t1	1	r_index	1	a	A	NULL	NULL	NULL		BTREE			YES	NULL
DROP TEMPORARY TABLE t1;
DROP DATABASE test1;
