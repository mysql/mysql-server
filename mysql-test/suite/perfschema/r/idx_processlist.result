select connection_id() into @conid;

====================================================================
Testing index for columns ID
====================================================================
############ Explain for Query ####################################
explain select ID 
from performance_schema.processlist 
where ID = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select ID
from performance_schema.processlist
where ID > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	processlist	NULL	ALL	PRIMARY	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select ID
from performance_schema.processlist
where ID < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	processlist	NULL	ALL	PRIMARY	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select ID
from performance_schema.processlist
where ID = @conid;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	processlist	NULL	const	PRIMARY	PRIMARY	8	const	#	100.00	NULL
############# Explain End #########################################
flush status;
select ID
from performance_schema.processlist
where ID = @conid;
ID
#
OK: handler_read_key incremented
