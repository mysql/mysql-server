create user foo@localhost;
grant ALL on *.* to foo@localhost;
select connection_id() into @conid;
select thread_id from performance_schema.threads
where PROCESSLIST_ID = @conid
into @tid;

====================================================================
Testing index for columns USER, EVENT_NAME
====================================================================
############ Explain for Query ####################################
explain select LOW_COUNT_USED
from performance_schema.memory_summary_by_user_by_event_name
where USER = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	memory_summary_by_user_by_event_name	NULL	ref	USER	USER	129	const	#	100.00	NULL
############ Explain for Query ####################################
explain select LOW_COUNT_USED
from performance_schema.memory_summary_by_user_by_event_name
where USER > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	memory_summary_by_user_by_event_name	NULL	ALL	USER	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select LOW_COUNT_USED
from performance_schema.memory_summary_by_user_by_event_name
where USER < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	memory_summary_by_user_by_event_name	NULL	ALL	USER	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select LOW_COUNT_USED
from performance_schema.memory_summary_by_user_by_event_name
where USER = "foo";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	memory_summary_by_user_by_event_name	NULL	ref	USER	USER	129	const	#	100.00	NULL
############ Explain for Query ####################################
explain select LOW_COUNT_USED
from performance_schema.memory_summary_by_user_by_event_name
where USER = "impossible"
    and EVENT_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select LOW_COUNT_USED
from performance_schema.memory_summary_by_user_by_event_name
where USER = "foo"
and EVENT_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select LOW_COUNT_USED
from performance_schema.memory_summary_by_user_by_event_name
where USER = "foo"
and EVENT_NAME > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	memory_summary_by_user_by_event_name	NULL	ref	USER	USER	129	const	#	33.33	Using where
############ Explain for Query ####################################
explain select LOW_COUNT_USED
from performance_schema.memory_summary_by_user_by_event_name
where USER = "foo"
and EVENT_NAME < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	memory_summary_by_user_by_event_name	NULL	ref	USER	USER	129	const	#	33.33	Using where
############ Explain for Query ####################################
explain select LOW_COUNT_USED
from performance_schema.memory_summary_by_user_by_event_name
where USER = "foo"
and EVENT_NAME = "memory/sql/help";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	memory_summary_by_user_by_event_name	NULL	const	USER	USER	643	const,const	#	100.00	NULL
############# Explain End #########################################
flush status;
select LOW_COUNT_USED
from performance_schema.memory_summary_by_user_by_event_name
where USER = "foo"
and EVENT_NAME = "memory/sql/help";
LOW_COUNT_USED
#
OK: handler_read_key incremented
drop user foo@localhost;
