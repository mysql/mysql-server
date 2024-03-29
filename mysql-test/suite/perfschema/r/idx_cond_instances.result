select OBJECT_INSTANCE_BEGIN, NAME from performance_schema.cond_instances
limit 1
into @oib, @name;

====================================================================
Testing index for columns OBJECT_INSTANCE_BEGIN
====================================================================
############ Explain for Query ####################################
explain select count(*) 
from performance_schema.cond_instances 
where OBJECT_INSTANCE_BEGIN = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select count(*)
from performance_schema.cond_instances
where OBJECT_INSTANCE_BEGIN > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	cond_instances	NULL	ALL	PRIMARY	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select count(*)
from performance_schema.cond_instances
where OBJECT_INSTANCE_BEGIN < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	cond_instances	NULL	ALL	PRIMARY	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select count(*)
from performance_schema.cond_instances
where OBJECT_INSTANCE_BEGIN = @oib;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	cond_instances	NULL	const	PRIMARY	PRIMARY	8	const	#	100.00	NULL
############# Explain End #########################################
flush status;
select count(*)
from performance_schema.cond_instances
where OBJECT_INSTANCE_BEGIN = @oib;
count(*)
#
OK: handler_read_key incremented

====================================================================
Testing index for columns NAME
====================================================================
############ Explain for Query ####################################
explain select count(*) 
from performance_schema.cond_instances 
where NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	cond_instances	NULL	ref	NAME	NAME	514	const	#	100.00	NULL
############ Explain for Query ####################################
explain select count(*)
from performance_schema.cond_instances
where NAME > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	cond_instances	NULL	ALL	NAME	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select count(*)
from performance_schema.cond_instances
where NAME < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	cond_instances	NULL	ALL	NAME	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select count(*)
from performance_schema.cond_instances
where NAME = @name;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	cond_instances	NULL	ref	NAME	NAME	514	const	#	100.00	NULL
############# Explain End #########################################
flush status;
select count(*)
from performance_schema.cond_instances
where NAME = @name;
count(*)
#
OK: handler_read_key incremented
