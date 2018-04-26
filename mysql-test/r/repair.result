CALL mtr.add_suppression("Enabling keys got errno");
drop table if exists t1;
create table t1 SELECT 1,"table 1";
repair table t1 use_frm;
Table	Op	Msg_type	Msg_text
test.t1	repair	warning	Number of rows changed from 0 to 1
test.t1	repair	status	OK
alter table t1 ENGINE=HEAP;
repair table t1 use_frm;
Table	Op	Msg_type	Msg_text
test.t1	repair	note	The storage engine for the table doesn't support repair
drop table t1;
create table t1(id int PRIMARY KEY, st varchar(10), KEY st_key(st));
insert into t1 values(1, "One");
alter table t1 disable keys;
analyze table t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
show keys from t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	0	PRIMARY	1	id	A	1	NULL	NULL		BTREE			YES	NULL
t1	1	st_key	1	st	A	NULL	NULL	NULL	YES	BTREE	disabled		YES	NULL
repair table t1 extended;
Table	Op	Msg_type	Msg_text
test.t1	repair	status	OK
analyze table t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
show keys from t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	0	PRIMARY	1	id	A	1	NULL	NULL		BTREE			YES	NULL
t1	1	st_key	1	st	A	NULL	NULL	NULL	YES	BTREE	disabled		YES	NULL
drop table t1;
repair table t1 use_frm;
Table	Op	Msg_type	Msg_text
test.t1	repair	Error	Table 'test.t1' doesn't exist
test.t1	repair	status	Operation failed
create table t1 engine=myisam SELECT 1,"table 1";
flush tables;
repair table t1;
Table	Op	Msg_type	Msg_text
test.t1	repair	Error	Incorrect file format 't1'
test.t1	repair	error	Corrupt
repair table t1 use_frm;
Table	Op	Msg_type	Msg_text
test.t1	repair	warning	Number of rows changed from 0 to 1
test.t1	repair	status	OK
drop table t1;
CREATE TABLE t1(a INT, KEY(a));
INSERT INTO t1 VALUES(1),(2),(3),(4),(5);
SET myisam_repair_threads=2;
REPAIR TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	repair	status	OK
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	Table is already up to date
SHOW INDEX FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	a	1	a	A	5	NULL	NULL	YES	BTREE			YES	NULL
SET myisam_repair_threads=@@global.myisam_repair_threads;
DROP TABLE t1;
CREATE TABLE t1(a INT);
USE mysql;
REPAIR TABLE test.t1 USE_FRM;
Table	Op	Msg_type	Msg_text
test.t1	repair	status	OK
USE test;
DROP TABLE t1;
CREATE TABLE t1(a CHAR(255), KEY(a)) charset latin1;
SET myisam_sort_buffer_size=4096;
INSERT INTO t1 VALUES
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0');
Warnings:
Error	1034	myisam_sort_buffer_size is too small
Error	1034	Number of rows changed from 0 to 157
SET myisam_repair_threads=2;
REPAIR TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	repair	error	myisam_sort_buffer_size is too small
test.t1	repair	warning	Number of rows changed from 0 to 157
test.t1	repair	status	OK
SET myisam_repair_threads=@@global.myisam_repair_threads;
SET myisam_sort_buffer_size=@@global.myisam_sort_buffer_size;
DROP TABLE t1;
CREATE TABLE t1(a CHAR(255), KEY(a)) charset latin1;
SET myisam_sort_buffer_size=4496;
INSERT INTO t1 VALUES
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),('0'),
('0'),('0'),('0'),('0'),('0'),('0'),('0');
SET myisam_repair_threads=2;
REPAIR TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	repair	status	OK
SET myisam_repair_threads=@@global.myisam_repair_threads;
SET myisam_sort_buffer_size=@@global.myisam_sort_buffer_size;
DROP TABLE t1;
End of 4.1 tests
DROP TABLE IF EXISTS tt1;
CREATE TEMPORARY TABLE tt1 (c1 INT);
REPAIR TABLE tt1 USE_FRM;
Table	Op	Msg_type	Msg_text
tt1	repair	error	Cannot repair temporary table from .frm file
DROP TABLE tt1;
#
# Bug #48248 assert in MDL_ticket::upgrade_shared_lock_to_exclusive
#
DROP TABLE IF EXISTS t1;
CREATE TABLE t1(a INT);
LOCK TABLES t1 READ;
REPAIR TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	repair	Error	Table 't1' was locked with a READ lock and can't be updated
test.t1	repair	status	Operation failed
UNLOCK TABLES;
DROP TABLE t1;
#
# Test for bug #50784 "MDL: Assertion `m_tickets.is_empty() ||
#                      m_tickets.front() == m_trans_sentinel'"
#
drop tables if exists t1, t2;
create table t1 (i int);
create table t2 (j int);
set @@autocommit= 0;
repair table t1, t2;
Table	Op	Msg_type	Msg_text
test.t1	repair	status	OK
test.t2	repair	status	OK
set @@autocommit= default;
drop tables t1, t2;
