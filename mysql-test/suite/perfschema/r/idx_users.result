create user foo@localhost;
grant ALL on *.* to foo@localhost;

====================================================================
Testing index for columns USER
====================================================================
############ Explain for Query ####################################
explain select TOTAL_CONNECTIONS 
from performance_schema.users 
where USER = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select TOTAL_CONNECTIONS
from performance_schema.users
where USER > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	users	NULL	ALL	USER	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select TOTAL_CONNECTIONS
from performance_schema.users
where USER < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	users	NULL	ALL	USER	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select TOTAL_CONNECTIONS
from performance_schema.users
where USER = "foo";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	users	NULL	const	USER	USER	129	const	#	100.00	NULL
############# Explain End #########################################
flush status;
select TOTAL_CONNECTIONS
from performance_schema.users
where USER = "foo";
TOTAL_CONNECTIONS
#
OK: handler_read_key incremented
drop user foo@localhost;
