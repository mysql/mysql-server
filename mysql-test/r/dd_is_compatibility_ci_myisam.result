#########################################################
# WL#6599: New data dictionary and I_S.
# 
# The re-implemntation of I_S as views on top of DD tables,
# together with the modified way of retrieving statistics
# information, introduces some differences when comparing
# with the previous I_S implementation. The purpose of this
# test is to focus on these behavioral differences, both
# for the purpose of regression testing, and to document
# the changes. The issues below refer to the items listed
# in the WL#6599 text (HLS section 6).
#########################################################
# WL#6599 HLS/6m
# MySQL 'DISABLE KEYS' functionality is only applicable to MyISAM
# tables, but looks like its marked for InnoDB table too in trunk.
# This problem is fixed now. It has been discussed with Martin Hansson
# who worked on WL8697 which introduced invisible index. He agreed that
# the behavior on trunk is bad and needed to be fixed. We have fixed
# that now.
#########################################################
CREATE TABLE t1 ( a INT, KEY (a) INVISIBLE );
CREATE TABLE t2 ( a INT, KEY (a) INVISIBLE ) ENGINE=MyISAM;
SHOW KEYS FROM t1;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t1	1	a	1	a	A	NULL	NULL	NULL	YES	BTREE			NO	NULL
SHOW KEYS FROM t2;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t2	1	a	1	a	A	NULL	NULL	NULL	YES	BTREE			NO	NULL
CREATE TABLE t3 ( a INT, KEY (a));
SHOW KEYS FROM t3;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t3	1	a	1	a	A	NULL	NULL	NULL	YES	BTREE			YES	NULL
ALTER TABLE t3 ALTER INDEX a INVISIBLE;
SHOW KEYS FROM t3;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t3	1	a	1	a	A	NULL	NULL	NULL	YES	BTREE			NO	NULL
CREATE TABLE t4 ( a INT, KEY (a)) ENGINE=MyISAM;
SHOW KEYS FROM t4;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t4	1	a	1	a	A	NULL	NULL	NULL	YES	BTREE			YES	NULL
ALTER TABLE t4 ALTER INDEX a INVISIBLE;
SHOW KEYS FROM t4;
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t4	1	a	1	a	A	NULL	NULL	NULL	YES	BTREE			NO	NULL
DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;
