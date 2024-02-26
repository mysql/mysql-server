
====================================================================
Testing index for columns EVENT_NAME
====================================================================
############ Explain for Query ####################################
explain select LOW_COUNT_USED 
from performance_schema.memory_summary_global_by_event_name 
where EVENT_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select LOW_COUNT_USED
from performance_schema.memory_summary_global_by_event_name
where EVENT_NAME > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	memory_summary_global_by_event_name	NULL	ALL	PRIMARY	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select LOW_COUNT_USED
from performance_schema.memory_summary_global_by_event_name
where EVENT_NAME < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	memory_summary_global_by_event_name	NULL	ALL	PRIMARY	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select LOW_COUNT_USED
from performance_schema.memory_summary_global_by_event_name
where EVENT_NAME = "memory/performance_schema/users";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	memory_summary_global_by_event_name	NULL	const	PRIMARY	PRIMARY	514	const	#	100.00	NULL
############# Explain End #########################################
flush status;
select LOW_COUNT_USED
from performance_schema.memory_summary_global_by_event_name
where EVENT_NAME = "memory/performance_schema/users";
LOW_COUNT_USED
#
OK: handler_read_key incremented
