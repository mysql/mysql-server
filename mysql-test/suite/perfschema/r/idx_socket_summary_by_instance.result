select OBJECT_INSTANCE_BEGIN, EVENT_NAME
from performance_schema.socket_summary_by_instance
limit 1
into @oib, @e_name;

====================================================================
Testing index for columns OBJECT_INSTANCE_BEGIN
====================================================================
############ Explain for Query ####################################
explain select COUNT_STAR 
from performance_schema.socket_summary_by_instance 
where OBJECT_INSTANCE_BEGIN = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.socket_summary_by_instance
where OBJECT_INSTANCE_BEGIN > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	socket_summary_by_instance	NULL	ALL	PRIMARY	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.socket_summary_by_instance
where OBJECT_INSTANCE_BEGIN < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	socket_summary_by_instance	NULL	ALL	PRIMARY	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.socket_summary_by_instance
where OBJECT_INSTANCE_BEGIN = @oib;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	socket_summary_by_instance	NULL	const	PRIMARY	PRIMARY	8	const	#	100.00	NULL
############# Explain End #########################################
flush status;
select COUNT_STAR
from performance_schema.socket_summary_by_instance
where OBJECT_INSTANCE_BEGIN = @oib;
COUNT_STAR
#
OK: handler_read_key incremented

====================================================================
Testing index for columns EVENT_NAME
====================================================================
############ Explain for Query ####################################
explain select COUNT_STAR 
from performance_schema.socket_summary_by_instance 
where EVENT_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	socket_summary_by_instance	NULL	ref	EVENT_NAME	EVENT_NAME	514	const	#	100.00	NULL
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.socket_summary_by_instance
where EVENT_NAME > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	socket_summary_by_instance	NULL	ALL	EVENT_NAME	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.socket_summary_by_instance
where EVENT_NAME < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	socket_summary_by_instance	NULL	ALL	EVENT_NAME	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select COUNT_STAR
from performance_schema.socket_summary_by_instance
where EVENT_NAME = @e_name;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	socket_summary_by_instance	NULL	ref	EVENT_NAME	EVENT_NAME	514	const	#	100.00	NULL
############# Explain End #########################################
flush status;
select COUNT_STAR
from performance_schema.socket_summary_by_instance
where EVENT_NAME = @e_name;
COUNT_STAR
#
OK: handler_read_key incremented
