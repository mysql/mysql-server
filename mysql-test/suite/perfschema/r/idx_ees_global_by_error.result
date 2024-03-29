
====================================================================
Testing index for columns ERROR_NUMBER
====================================================================
############ Explain for Query ####################################
explain select SUM_ERROR_RAISED 
from performance_schema.events_errors_summary_global_by_error 
where ERROR_NUMBER = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select SUM_ERROR_RAISED
from performance_schema.events_errors_summary_global_by_error
where ERROR_NUMBER > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	events_errors_summary_global_by_error	NULL	ALL	ERROR_NUMBER	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select SUM_ERROR_RAISED
from performance_schema.events_errors_summary_global_by_error
where ERROR_NUMBER < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	events_errors_summary_global_by_error	NULL	ALL	ERROR_NUMBER	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select SUM_ERROR_RAISED
from performance_schema.events_errors_summary_global_by_error
where ERROR_NUMBER = "1146";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	events_errors_summary_global_by_error	NULL	const	ERROR_NUMBER	ERROR_NUMBER	5	const	#	100.00	NULL
############# Explain End #########################################
flush status;
select SUM_ERROR_RAISED
from performance_schema.events_errors_summary_global_by_error
where ERROR_NUMBER = "1146";
SUM_ERROR_RAISED
#
OK: handler_read_key incremented
