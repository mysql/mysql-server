
====================================================================
Testing index for columns VARIABLE_NAME
====================================================================
############ Explain for Query ####################################
explain select VARIABLE_VALUE 
from performance_schema.session_status 
where VARIABLE_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select VARIABLE_VALUE
from performance_schema.session_status
where VARIABLE_NAME > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	session_status	NULL	ALL	PRIMARY	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select VARIABLE_VALUE
from performance_schema.session_status
where VARIABLE_NAME < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	session_status	NULL	ALL	PRIMARY	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select VARIABLE_VALUE
from performance_schema.session_status
where VARIABLE_NAME = "Performance_schema_users_lost";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	session_status	NULL	const	PRIMARY	PRIMARY	258	const	#	100.00	NULL
############# Explain End #########################################
flush status;
select VARIABLE_VALUE
from performance_schema.session_status
where VARIABLE_NAME = "Performance_schema_users_lost";
VARIABLE_VALUE
#
OK: handler_read_key incremented
