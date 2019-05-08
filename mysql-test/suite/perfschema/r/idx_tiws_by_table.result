create database explain_test_db;
create table explain_test_db.explain_test_table(c int);
truncate table performance_schema.table_io_waits_summary_by_table;
insert into explain_test_db.explain_test_table values('1');

====================================================================
Testing index for columns OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME
====================================================================
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.table_io_waits_summary_by_table
where OBJECT_TYPE = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	table_io_waits_summary_by_table	NULL	ref	OBJECT	OBJECT	259	const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.table_io_waits_summary_by_table
where OBJECT_TYPE > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	table_io_waits_summary_by_table	NULL	ALL	OBJECT	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.table_io_waits_summary_by_table
where OBJECT_TYPE < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	table_io_waits_summary_by_table	NULL	ALL	OBJECT	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.table_io_waits_summary_by_table
where OBJECT_TYPE = "TABLE";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	table_io_waits_summary_by_table	NULL	ref	OBJECT	OBJECT	259	const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.table_io_waits_summary_by_table
where OBJECT_TYPE = "impossible"
    and OBJECT_SCHEMA = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	table_io_waits_summary_by_table	NULL	ref	OBJECT	OBJECT	518	const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.table_io_waits_summary_by_table
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	table_io_waits_summary_by_table	NULL	ref	OBJECT	OBJECT	518	const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.table_io_waits_summary_by_table
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	table_io_waits_summary_by_table	NULL	ref	OBJECT	OBJECT	259	const	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.table_io_waits_summary_by_table
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	table_io_waits_summary_by_table	NULL	ref	OBJECT	OBJECT	259	const	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.table_io_waits_summary_by_table
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA = "explain_test_db";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	table_io_waits_summary_by_table	NULL	ref	OBJECT	OBJECT	518	const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.table_io_waits_summary_by_table
where OBJECT_TYPE = "impossible"
    and OBJECT_SCHEMA = "impossible"
    and OBJECT_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.table_io_waits_summary_by_table
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.table_io_waits_summary_by_table
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	table_io_waits_summary_by_table	NULL	ref	OBJECT	OBJECT	518	const,const	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.table_io_waits_summary_by_table
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	table_io_waits_summary_by_table	NULL	ref	OBJECT	OBJECT	518	const,const	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.table_io_waits_summary_by_table
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "explain_test_table";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	table_io_waits_summary_by_table	NULL	const	OBJECT	OBJECT	777	const,const,const	#	100.00	NULL
############# Explain End #########################################
flush status;
select COUNT_STAR
from performance_schema.table_io_waits_summary_by_table
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "explain_test_table";
COUNT_STAR
#
OK: handler_read_key incremented
drop table explain_test_db.explain_test_table;
drop database explain_test_db;
