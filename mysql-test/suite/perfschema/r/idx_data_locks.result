# Connection con1
create database explain_test_db;
create table explain_test_db.explain_test_table(a int, b int, PRIMARY KEY (a, b)) engine=innodb
PARTITION BY RANGE( a )
SUBPARTITION BY HASH( b ) (
PARTITION p0 VALUES LESS THAN (1990) (
SUBPARTITION s0,
SUBPARTITION s1
),
PARTITION p1 VALUES LESS THAN (2000) (
SUBPARTITION s2,
SUBPARTITION s3
),
PARTITION p2 VALUES LESS THAN MAXVALUE (
SUBPARTITION s4,
SUBPARTITION s5
)
);
insert into explain_test_db.explain_test_table values (1, 100), (1995, 200);
start transaction;
update explain_test_db.explain_test_table set b=b+1;
# Connection default
select ENGINE, ENGINE_LOCK_ID, ENGINE_TRANSACTION_ID, THREAD_ID, EVENT_ID,
PARTITION_NAME, SUBPARTITION_NAME
from performance_schema.data_locks
where OBJECT_NAME = 'explain_test_table'
  and LOCK_TYPE = 'RECORD' and LOCK_DATA = '1, 100'
  into @engine, @lock_id, @trx_id, @thread_id, @event_id, @part, @subpart;
select @engine is null,
@lock_id is null,
@trx_id is null,
@thread_id is null,
@event_id is null,
@part, @subpart;
@engine is null	@lock_id is null	@trx_id is null	@thread_id is null	@event_id is null	@part	@subpart
0	0	0	0	0	p0	s0

====================================================================
Testing index for columns ENGINE_LOCK_ID, ENGINE
====================================================================
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where ENGINE_LOCK_ID = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	PRIMARY	PRIMARY	514	const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where ENGINE_LOCK_ID > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ALL	PRIMARY	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where ENGINE_LOCK_ID < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ALL	PRIMARY	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where ENGINE_LOCK_ID = @lock_id;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	PRIMARY	PRIMARY	514	const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where ENGINE_LOCK_ID = "impossible"
    and ENGINE = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where ENGINE_LOCK_ID = @lock_id
and ENGINE = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where ENGINE_LOCK_ID = @lock_id
and ENGINE > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	PRIMARY	PRIMARY	514	const	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where ENGINE_LOCK_ID = @lock_id
and ENGINE < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	PRIMARY	PRIMARY	514	const	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where ENGINE_LOCK_ID = @lock_id
and ENGINE = @engine;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	const	PRIMARY	PRIMARY	644	const,const	#	100.00	NULL
############# Explain End #########################################
flush status;
select COUNT(*)
from performance_schema.data_locks
where ENGINE_LOCK_ID = @lock_id
and ENGINE = @engine;
COUNT(*)
#
OK: handler_read_key incremented

====================================================================
Testing index for columns ENGINE_TRANSACTION_ID, ENGINE
====================================================================
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where ENGINE_TRANSACTION_ID = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	ENGINE_TRANSACTION_ID	ENGINE_TRANSACTION_ID	9	const	#	100.00	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where ENGINE_TRANSACTION_ID > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ALL	ENGINE_TRANSACTION_ID	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where ENGINE_TRANSACTION_ID < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ALL	ENGINE_TRANSACTION_ID	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where ENGINE_TRANSACTION_ID = @trx_id;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	ENGINE_TRANSACTION_ID	ENGINE_TRANSACTION_ID	9	const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where ENGINE_TRANSACTION_ID = "impossible"
    and ENGINE = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	ENGINE_TRANSACTION_ID	ENGINE_TRANSACTION_ID	139	const,const	#	100.00	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where ENGINE_TRANSACTION_ID = @trx_id
and ENGINE = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	ENGINE_TRANSACTION_ID	ENGINE_TRANSACTION_ID	139	const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where ENGINE_TRANSACTION_ID = @trx_id
and ENGINE > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	ENGINE_TRANSACTION_ID	ENGINE_TRANSACTION_ID	9	const	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where ENGINE_TRANSACTION_ID = @trx_id
and ENGINE < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	ENGINE_TRANSACTION_ID	ENGINE_TRANSACTION_ID	9	const	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where ENGINE_TRANSACTION_ID = @trx_id
and ENGINE = @engine;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	ENGINE_TRANSACTION_ID	ENGINE_TRANSACTION_ID	139	const,const	#	100.00	NULL
############# Explain End #########################################
flush status;
select COUNT(*)
from performance_schema.data_locks
where ENGINE_TRANSACTION_ID = @trx_id
and ENGINE = @engine;
COUNT(*)
#
OK: handler_read_key incremented

====================================================================
Testing index for columns THREAD_ID, EVENT_ID
====================================================================
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where THREAD_ID = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	THREAD_ID	THREAD_ID	9	const	#	100.00	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where THREAD_ID > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ALL	THREAD_ID	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where THREAD_ID < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ALL	THREAD_ID	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where THREAD_ID = @thread_id;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	THREAD_ID	THREAD_ID	9	const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where THREAD_ID = "impossible"
    and EVENT_ID = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	THREAD_ID	THREAD_ID	18	const,const	#	100.00	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where THREAD_ID = @thread_id
and EVENT_ID = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	THREAD_ID	THREAD_ID	18	const,const	#	100.00	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where THREAD_ID = @thread_id
and EVENT_ID > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	THREAD_ID	THREAD_ID	9	const	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where THREAD_ID = @thread_id
and EVENT_ID < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	THREAD_ID	THREAD_ID	9	const	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where THREAD_ID = @thread_id
and EVENT_ID = @event_id;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	THREAD_ID	THREAD_ID	18	const,const	#	100.00	NULL
############# Explain End #########################################
flush status;
select COUNT(*)
from performance_schema.data_locks
where THREAD_ID = @thread_id
and EVENT_ID = @event_id;
COUNT(*)
#
OK: handler_read_key incremented

====================================================================
Testing index for columns OBJECT_SCHEMA, OBJECT_NAME, PARTITION_NAME, SUBPARTITION_NAME
====================================================================
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	OBJECT_SCHEMA	OBJECT_SCHEMA	259	const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ALL	OBJECT_SCHEMA	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ALL	OBJECT_SCHEMA	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA = "explain_test_db";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	OBJECT_SCHEMA	OBJECT_SCHEMA	259	const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA = "impossible"
    and OBJECT_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	OBJECT_SCHEMA	OBJECT_SCHEMA	518	const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	OBJECT_SCHEMA	OBJECT_SCHEMA	518	const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	OBJECT_SCHEMA	OBJECT_SCHEMA	259	const	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	OBJECT_SCHEMA	OBJECT_SCHEMA	259	const	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "explain_test_table";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	OBJECT_SCHEMA	OBJECT_SCHEMA	518	const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA = "impossible"
    and OBJECT_NAME = "impossible"
    and PARTITION_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	OBJECT_SCHEMA	OBJECT_SCHEMA	777	const,const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "explain_test_table"
and PARTITION_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	OBJECT_SCHEMA	OBJECT_SCHEMA	777	const,const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "explain_test_table"
and PARTITION_NAME > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	OBJECT_SCHEMA	OBJECT_SCHEMA	518	const,const	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "explain_test_table"
and PARTITION_NAME < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	OBJECT_SCHEMA	OBJECT_SCHEMA	518	const,const	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "explain_test_table"
and PARTITION_NAME = @part;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	OBJECT_SCHEMA	OBJECT_SCHEMA	777	const,const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA = "impossible"
    and OBJECT_NAME = "impossible"
    and PARTITION_NAME = "impossible"
    and SUBPARTITION_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	OBJECT_SCHEMA	OBJECT_SCHEMA	1036	const,const,const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "explain_test_table"
and PARTITION_NAME = @part
and SUBPARTITION_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	OBJECT_SCHEMA	OBJECT_SCHEMA	1036	const,const,const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "explain_test_table"
and PARTITION_NAME = @part
and SUBPARTITION_NAME > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	OBJECT_SCHEMA	OBJECT_SCHEMA	777	const,const,const	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "explain_test_table"
and PARTITION_NAME = @part
and SUBPARTITION_NAME < "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	OBJECT_SCHEMA	OBJECT_SCHEMA	777	const,const,const	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "explain_test_table"
and PARTITION_NAME = @part
and SUBPARTITION_NAME = @subpart;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	data_locks	NULL	ref	OBJECT_SCHEMA	OBJECT_SCHEMA	1036	const,const,const,const	#	100.00	NULL
############# Explain End #########################################
flush status;
select COUNT(*)
from performance_schema.data_locks
where OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "explain_test_table"
and PARTITION_NAME = @part
and SUBPARTITION_NAME = @subpart;
COUNT(*)
#
OK: handler_read_key incremented
# Connection con1
commit;
drop table explain_test_db.explain_test_table;
drop database explain_test_db;
# Connection default
