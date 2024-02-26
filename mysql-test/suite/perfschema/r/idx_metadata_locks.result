# Connection con1
create database explain_test_db;
create table explain_test_db.explain_test_table(a int);
lock table explain_test_db.explain_test_table write;
# Connection default
select OBJECT_INSTANCE_BEGIN, OWNER_THREAD_ID, OWNER_EVENT_ID
from performance_schema.metadata_locks
where OBJECT_TYPE = 'TABLE'
   and OBJECT_SCHEMA = 'explain_test_db'
   and OBJECT_NAME = 'explain_test_table'
   and LOCK_STATUS = 'GRANTED'
   into @oib, @o_tid, @o_eid;
select @oib is not null, @o_tid is not null, @o_eid is not null;
@oib is not null	@o_tid is not null	@o_eid is not null
1	1	1

====================================================================
Testing index for columns OBJECT_INSTANCE_BEGIN
====================================================================
############ Explain for Query ####################################
explain select LOCK_TYPE 
from performance_schema.metadata_locks 
where OBJECT_INSTANCE_BEGIN = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_INSTANCE_BEGIN > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ALL	PRIMARY	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_INSTANCE_BEGIN < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ALL	PRIMARY	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_INSTANCE_BEGIN = @oib;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	const	PRIMARY	PRIMARY	8	const	#	100.00	NULL
############# Explain End #########################################
flush status;
select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_INSTANCE_BEGIN = @oib;
LOCK_TYPE
#
OK: handler_read_key incremented

====================================================================
Testing index for columns OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, COLUMN_NAME
====================================================================
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OBJECT_TYPE	OBJECT_TYPE	258	const	#	100.00	NULL
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ALL	OBJECT_TYPE	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ALL	OBJECT_TYPE	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE = "TABLE";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OBJECT_TYPE	OBJECT_TYPE	258	const	#	100.00	NULL
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE = "impossible"
    and OBJECT_SCHEMA = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OBJECT_TYPE	OBJECT_TYPE	517	const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OBJECT_TYPE	OBJECT_TYPE	517	const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OBJECT_TYPE	OBJECT_TYPE	258	const	#	33.33	Using where
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OBJECT_TYPE	OBJECT_TYPE	258	const	#	33.33	Using where
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA = "explain_test_db";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OBJECT_TYPE	OBJECT_TYPE	517	const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE = "impossible"
    and OBJECT_SCHEMA = "impossible"
    and OBJECT_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OBJECT_TYPE	OBJECT_TYPE	776	const,const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OBJECT_TYPE	OBJECT_TYPE	776	const,const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OBJECT_TYPE	OBJECT_TYPE	517	const,const	#	33.33	Using where
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OBJECT_TYPE	OBJECT_TYPE	517	const,const	#	33.33	Using where
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "explain_test_table";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OBJECT_TYPE	OBJECT_TYPE	776	const,const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE = "impossible"
    and OBJECT_SCHEMA = "impossible"
    and OBJECT_NAME = "impossible"
    and COLUMN_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OBJECT_TYPE	OBJECT_TYPE	1035	const,const,const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "explain_test_table"
and COLUMN_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OBJECT_TYPE	OBJECT_TYPE	1035	const,const,const,const	#	100.00	NULL
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "explain_test_table"
and COLUMN_NAME > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OBJECT_TYPE	OBJECT_TYPE	776	const,const,const	#	33.33	Using where
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "explain_test_table"
and COLUMN_NAME < "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OBJECT_TYPE	OBJECT_TYPE	776	const,const,const	#	33.33	Using where
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "explain_test_table"
and COLUMN_NAME = "a";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OBJECT_TYPE	OBJECT_TYPE	1035	const,const,const,const	#	100.00	NULL
############# Explain End #########################################
flush status;
select LOCK_TYPE
from performance_schema.metadata_locks
where OBJECT_TYPE = "TABLE"
and OBJECT_SCHEMA = "explain_test_db"
and OBJECT_NAME = "explain_test_table"
and COLUMN_NAME = "a";
LOCK_TYPE
OK: handler_read_key incremented

====================================================================
Testing index for columns OWNER_THREAD_ID, OWNER_EVENT_ID
====================================================================
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OWNER_THREAD_ID = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OWNER_THREAD_ID	OWNER_THREAD_ID	9	const	#	100.00	Using where
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OWNER_THREAD_ID > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ALL	OWNER_THREAD_ID	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OWNER_THREAD_ID < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ALL	OWNER_THREAD_ID	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OWNER_THREAD_ID = @o_tid;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OWNER_THREAD_ID	OWNER_THREAD_ID	9	const	#	100.00	NULL
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OWNER_THREAD_ID = "impossible"
    and OWNER_EVENT_ID = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OWNER_THREAD_ID	OWNER_THREAD_ID	18	const,const	#	100.00	Using where
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OWNER_THREAD_ID = @o_tid
and OWNER_EVENT_ID = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OWNER_THREAD_ID	OWNER_THREAD_ID	18	const,const	#	100.00	Using where
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OWNER_THREAD_ID = @o_tid
and OWNER_EVENT_ID > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OWNER_THREAD_ID	OWNER_THREAD_ID	9	const	#	33.33	Using where
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OWNER_THREAD_ID = @o_tid
and OWNER_EVENT_ID < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OWNER_THREAD_ID	OWNER_THREAD_ID	9	const	#	33.33	Using where
############ Explain for Query ####################################
explain select LOCK_TYPE
from performance_schema.metadata_locks
where OWNER_THREAD_ID = @o_tid
and OWNER_EVENT_ID = @o_eid;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	metadata_locks	NULL	ref	OWNER_THREAD_ID	OWNER_THREAD_ID	18	const,const	#	100.00	NULL
############# Explain End #########################################
flush status;
select LOCK_TYPE
from performance_schema.metadata_locks
where OWNER_THREAD_ID = @o_tid
and OWNER_EVENT_ID = @o_eid;
LOCK_TYPE
#
OK: handler_read_key incremented
# Connection con1
unlock tables;
drop table explain_test_db.explain_test_table;
drop database explain_test_db;
# Connection default
