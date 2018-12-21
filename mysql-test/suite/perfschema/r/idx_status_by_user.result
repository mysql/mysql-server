create user foo@localhost;
grant ALL on *.* to foo@localhost;
# Connection con

====================================================================
Testing index for columns USER, VARIABLE_NAME
====================================================================
############ Explain for Query ####################################
explain select VARIABLE_VALUE
from performance_schema.status_by_user
where USER = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	status_by_user	NULL	ref	USER	USER	129	const	#	100.00	NULL
############ Explain for Query ####################################
explain select VARIABLE_VALUE
from performance_schema.status_by_user
where USER > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	status_by_user	NULL	ALL	USER	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select VARIABLE_VALUE
from performance_schema.status_by_user
where USER < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	status_by_user	NULL	ALL	USER	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select VARIABLE_VALUE
from performance_schema.status_by_user
where USER = "foo";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	status_by_user	NULL	ref	USER	USER	129	const	#	100.00	NULL
############ Explain for Query ####################################
explain select VARIABLE_VALUE
from performance_schema.status_by_user
where USER = "impossible"
    and VARIABLE_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select VARIABLE_VALUE
from performance_schema.status_by_user
where USER = "foo"
and VARIABLE_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select VARIABLE_VALUE
from performance_schema.status_by_user
where USER = "foo"
and VARIABLE_NAME > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	status_by_user	NULL	ref	USER	USER	129	const	#	33.33	Using where
############ Explain for Query ####################################
explain select VARIABLE_VALUE
from performance_schema.status_by_user
where USER = "foo"
and VARIABLE_NAME < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	status_by_user	NULL	ref	USER	USER	129	const	#	33.33	Using where
############ Explain for Query ####################################
explain select VARIABLE_VALUE
from performance_schema.status_by_user
where USER = "foo"
and VARIABLE_NAME = "Max_execution_time_exceeded";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	status_by_user	NULL	const	USER	USER	387	const,const	#	100.00	NULL
############# Explain End #########################################
flush status;
select VARIABLE_VALUE
from performance_schema.status_by_user
where USER = "foo"
and VARIABLE_NAME = "Max_execution_time_exceeded";
VARIABLE_VALUE
#
OK: handler_read_key incremented
# Conenction default
drop user foo@localhost;
