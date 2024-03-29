SET @old_innodb_file_per_table = @@innodb_file_per_table;
SET GLOBAL innodb_file_per_table = 1;
SELECT @@innodb_file_per_table;
@@innodb_file_per_table
1
CREATE TABLE t1 (
col_1 CHAR (255),
col_2 VARCHAR (255)
) ENGINE = InnoDB charset latin1;
CREATE INDEX idx1 ON t1(col_1);
CREATE INDEX idx2 ON t1(col_2);
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	idx1	1	col_1	A	0	NULL	NULL	YES	BTREE			YES	NULL
t1	1	idx2	1	col_2	A	0	NULL	NULL	YES	BTREE			YES	NULL
INSERT INTO t1  VALUES ("col1_00001", "col2_00001"), ("col1_00002", "col2_00002");
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	idx1	1	col_1	A	2	NULL	NULL	YES	BTREE			YES	NULL
t1	1	idx2	1	col_2	A	2	NULL	NULL	YES	BTREE			YES	NULL
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	idx1	1	col_1	A	2	NULL	NULL	YES	BTREE			YES	NULL
t1	1	idx2	1	col_2	A	2	NULL	NULL	YES	BTREE			YES	NULL
FLUSH TABLES t1 FOR EXPORT;
backup: t1
UNLOCK TABLES;
DROP TABLE t1;
CREATE TABLE t1 (
col_1 CHAR (255),
col_2 VARCHAR (255)
) ENGINE = InnoDB charset latin1;
CREATE INDEX idx1 ON t1(col_1);
CREATE INDEX idx2 ON t1(col_2);
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	idx1	1	col_1	A	0	NULL	NULL	YES	BTREE			YES	NULL
t1	1	idx2	1	col_2	A	0	NULL	NULL	YES	BTREE			YES	NULL
INSERT INTO t1  VALUES ("col1_00001", "col2_00001");
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	idx1	1	col_1	A	1	NULL	NULL	YES	BTREE			YES	NULL
t1	1	idx2	1	col_2	A	1	NULL	NULL	YES	BTREE			YES	NULL
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	idx1	1	col_1	A	1	NULL	NULL	YES	BTREE			YES	NULL
t1	1	idx2	1	col_2	A	1	NULL	NULL	YES	BTREE			YES	NULL
ALTER TABLE t1 DISCARD TABLESPACE;
restore: t1 .ibd and .cfg files
ALTER TABLE t1 IMPORT TABLESPACE;
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	idx1	1	col_1	A	2	NULL	NULL	YES	BTREE			YES	NULL
t1	1	idx2	1	col_2	A	2	NULL	NULL	YES	BTREE			YES	NULL
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	idx1	1	col_1	A	2	NULL	NULL	YES	BTREE			YES	NULL
t1	1	idx2	1	col_2	A	2	NULL	NULL	YES	BTREE			YES	NULL
DROP TABLE t1;
SET GLOBAL innodb_file_per_table = @old_innodb_file_per_table;
