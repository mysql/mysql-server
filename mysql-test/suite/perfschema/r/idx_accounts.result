create user foo@localhost;
grant ALL on *.* to foo@localhost;

====================================================================
Testing index for columns USER, HOST
====================================================================
############ Explain for Query ####################################
explain select TOTAL_CONNECTIONS
from performance_schema.accounts
where USER = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	accounts	NULL	ref	ACCOUNT	ACCOUNT	129	const	#	100.00	NULL
############ Explain for Query ####################################
explain select TOTAL_CONNECTIONS
from performance_schema.accounts
where USER > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	accounts	NULL	ALL	ACCOUNT	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select TOTAL_CONNECTIONS
from performance_schema.accounts
where USER < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	accounts	NULL	ALL	ACCOUNT	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select TOTAL_CONNECTIONS
from performance_schema.accounts
where USER = "foo";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	accounts	NULL	ref	ACCOUNT	ACCOUNT	129	const	#	100.00	NULL
############ Explain for Query ####################################
explain select TOTAL_CONNECTIONS
from performance_schema.accounts
where USER = "impossible"
    and HOST = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select TOTAL_CONNECTIONS
from performance_schema.accounts
where USER = "foo"
and HOST = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select TOTAL_CONNECTIONS
from performance_schema.accounts
where USER = "foo"
and HOST > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	accounts	NULL	ref	ACCOUNT	ACCOUNT	129	const	#	33.33	Using where
############ Explain for Query ####################################
explain select TOTAL_CONNECTIONS
from performance_schema.accounts
where USER = "foo"
and HOST < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	accounts	NULL	ref	ACCOUNT	ACCOUNT	129	const	#	33.33	Using where
############ Explain for Query ####################################
explain select TOTAL_CONNECTIONS
from performance_schema.accounts
where USER = "foo"
and HOST = "localhost";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	accounts	NULL	const	ACCOUNT	ACCOUNT	385	const,const	#	100.00	NULL
############# Explain End #########################################
flush status;
select TOTAL_CONNECTIONS
from performance_schema.accounts
where USER = "foo"
and HOST = "localhost";
TOTAL_CONNECTIONS
#
OK: handler_read_key incremented
drop user foo@localhost;
