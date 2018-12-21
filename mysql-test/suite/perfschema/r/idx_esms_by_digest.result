create database explain_test_db;
use explain_test_db;
truncate table performance_schema.events_statements_summary_by_digest;
select 1;
1
1
select digest from performance_schema.events_statements_summary_by_digest
where SCHEMA_NAME = 'explain_test_db' limit 1
into @digest;
use test;

====================================================================
Testing index for columns SCHEMA_NAME, DIGEST
====================================================================
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.events_statements_summary_by_digest
where SCHEMA_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	events_statements_summary_by_digest	NULL	ref	SCHEMA_NAME	SCHEMA_NAME	259	const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.events_statements_summary_by_digest
where SCHEMA_NAME > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	events_statements_summary_by_digest	NULL	ALL	SCHEMA_NAME	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.events_statements_summary_by_digest
where SCHEMA_NAME < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	events_statements_summary_by_digest	NULL	ALL	SCHEMA_NAME	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.events_statements_summary_by_digest
where SCHEMA_NAME = "explain_test_db";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	events_statements_summary_by_digest	NULL	ref	SCHEMA_NAME	SCHEMA_NAME	259	const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.events_statements_summary_by_digest
where SCHEMA_NAME = "impossible"
    and DIGEST = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.events_statements_summary_by_digest
where SCHEMA_NAME = "explain_test_db"
and DIGEST = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.events_statements_summary_by_digest
where SCHEMA_NAME = "explain_test_db"
and DIGEST > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	events_statements_summary_by_digest	NULL	ref	SCHEMA_NAME	SCHEMA_NAME	259	const	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.events_statements_summary_by_digest
where SCHEMA_NAME = "explain_test_db"
and DIGEST < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	events_statements_summary_by_digest	NULL	ref	SCHEMA_NAME	SCHEMA_NAME	259	const	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.events_statements_summary_by_digest
where SCHEMA_NAME = "explain_test_db"
and DIGEST = @digest;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	events_statements_summary_by_digest	NULL	const	SCHEMA_NAME	SCHEMA_NAME	518	const,const	#	100.00	NULL
############# Explain End #########################################
flush status;
select COUNT_STAR
from performance_schema.events_statements_summary_by_digest
where SCHEMA_NAME = "explain_test_db"
and DIGEST = @digest;
COUNT_STAR
#
OK: handler_read_key incremented
drop database explain_test_db;
