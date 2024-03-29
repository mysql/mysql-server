#
# Bug #21789000 SPATIAL INDEX CAUSES INCORRECT CARDINALITY FOR ALL
# NON-PRIMARY INDEXES
#
CREATE TABLE t1 (
id INT NOT NULL AUTO_INCREMENT,
a VARCHAR(10) NOT NULL,
b VARCHAR(5) NOT NULL,
c GEOMETRY NOT NULL SRID 0,
PRIMARY KEY (id),
SPATIAL INDEX c (c),
INDEX a (a),
INDEX b (b) 
) ENGINE=MyISAM;
CREATE TABLE t2 (
id INT NOT NULL AUTO_INCREMENT,
a VARCHAR(10) NOT NULL,
b VARCHAR(5) NOT NULL,
c GEOMETRY NOT NULL,
PRIMARY KEY (id),
INDEX a (a),
INDEX b (b) 
) ENGINE=MyISAM;
INSERT INTO t1(a, b, c) VALUES
('a1', 'b1', POINT(0, 0)),
('a2', 'b2', POINT(0, 0)),
('a3', 'b3', POINT(0, 0)),
('a4', 'b4', POINT(0, 0)),
('a5', 'b5', POINT(0, 0)),
('a6', 'b6', POINT(0, 0)),
('a7', 'b7', POINT(0, 0));
INSERT INTO t2 SELECT * FROM t1;
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SHOW INDEXES FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	0	PRIMARY	1	id	A	7	NULL	NULL		BTREE			YES	NULL
t1	1	c	1	c	A	1	32	NULL		SPATIAL			YES	NULL
t1	1	a	1	a	A	7	NULL	NULL		BTREE			YES	NULL
t1	1	b	1	b	A	7	NULL	NULL		BTREE			YES	NULL
ANALYZE TABLE t2;
Table	Op	Msg_type	Msg_text
test.t2	analyze	status	Table is already up to date
SHOW INDEXES FROM t2;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t2	0	PRIMARY	1	id	A	7	NULL	NULL		BTREE			YES	NULL
t2	1	a	1	a	A	7	NULL	NULL		BTREE			YES	NULL
t2	1	b	1	b	A	7	NULL	NULL		BTREE			YES	NULL
DROP TABLE t1, t2;
