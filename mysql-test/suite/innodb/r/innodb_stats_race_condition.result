#
# Bug #33135425	RACE CONDITION LEADING TO ASSERTION ON UNINITIALIZED STATS
#
set global table_open_cache = 16;
create table t (i int, j int, primary key(i), key(j));
create table u (i int, j int, primary key(i), key(j));
select * from t;
i	j
select * from u;
i	j
select * from t;
i	j
select * from u;
i	j
select * from t;
i	j
SET DEBUG_SYNC = 'after_open_tables SIGNAL c0 WAIT_FOR c1';
SET DEBUG_SYNC = 'innodb.after_init_check SIGNAL c2 WAIT_FOR c3';
show keys in t;
SET DEBUG_SYNC = 'now WAIT_FOR c0';
SET DEBUG_SYNC = 'innodb.before_stats_deinit SIGNAL c1 WAIT_FOR c2 TIMEOUT 5 ';
select * from u;
i	j
Warnings:
Warning	1639	debug sync point wait timed out
SET DEBUG_SYNC = 'now SIGNAL c3';
Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression
t	0	PRIMARY	1	i	A	0	NULL	NULL		BTREE			YES	NULL
t	1	j	1	j	A	0	NULL	NULL	YES	BTREE			YES	NULL
SET DEBUG_SYNC ='RESET';
drop table t,u;
set global table_open_cache = default;
