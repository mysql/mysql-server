#
# Bug#22469130: FOREIGN KEY ON DELETE CASCADE NOT ALLOWED
#		WHEN A VIRTUAL INDEX EXISTS.
# Add the VIRTUAL INDEX contains fk constraINT column
# using INPLACE alter operatiON
CREATE TABLE t1(fld1 INT NOT NULL PRIMARY KEY);
CREATE TABLE t2(fld1 INT NOT NULL, fld2 INT NOT NULL,
fld3 INT AS (fld2) VIRTUAL, KEY(fld1),
FOREIGN KEY(fld1) REFERENCES t1(fld1) ON UPDATE CASCADE);
INSERT INTO t1(fld1) VALUES(1);
INSERT INTO t2(fld1, fld2) VALUES(1, 2);
# restart
UPDATE t1 SET fld1= 2;
SELECT fld3, fld1 FROM t2;
fld3	fld1
2	2
alter TABLE t2 ADD INDEX vk(fld3, fld1), ALGORITHM=INPLACE;
UPDATE t1 SET fld1=3;
SELECT fld3, fld1 FROM t2;
fld3	fld1
2	3
EXPLAIN SELECT fld3, fld1 FROM t2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
x	x	t2	x	index	x	vk	x	x	x	x	x
DROP TABLE t2, t1;
# TEMPORARY TABLE NAME and CHILD TABLE NAME are same
CREATE TABLE t1(fld1 INT NOT NULL PRIMARY KEY);
CREATE TABLE t2(fld1 INT NOT NULL,
fld2 INT AS (fld1) VIRTUAL,
KEY(fld2),
FOREIGN KEY(fld1) REFERENCES t1(fld1)
ON UPDATE CASCADE);
INSERT INTO t1 VALUES(1), (2);
INSERT INTO t2 VALUES(1, DEFAULT), (2, default);
# restart
CREATE TEMPORARY TABLE t2 (fld1 INT NOT NULL)ENGINE=INNODB;
UPDATE t1 SET fld1= 3 WHERE fld1= 2;
SELECT fld2 FROM t2;
fld2
1
3
EXPLAIN SELECT fld2 FROM t2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
x	x	t2	x	index	x	fld2	x	x	x	x	x
CHECK TABLE t2;
Table	Op	Msg_type	Msg_text
test.t2	check	status	OK
DROP TABLE t2;
DROP TABLE t2, t1;
