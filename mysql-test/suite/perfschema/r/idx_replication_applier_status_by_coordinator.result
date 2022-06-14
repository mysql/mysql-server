include/master-slave.inc
[connection master]
include/stop_slave.inc
RESET SLAVE ALL;
SET @old_replica_parallel_workers= @@global.replica_parallel_workers;
SET GLOBAL replica_parallel_workers = 4;
include/start_slave.inc
include/sync_slave_sql_with_master.inc [FOR CHANNEL 'channel_1']
select CHANNEL_NAME, THREAD_ID
from performance_schema.replication_applier_status_by_coordinator
limit 1
into @c_name, @t_id;

====================================================================
Testing index for columns CHANNEL_NAME
====================================================================
############ Explain for Query ####################################
explain select SERVICE_STATE 
from performance_schema.replication_applier_status_by_coordinator 
where CHANNEL_NAME = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	#	NULL	no matching row in const table
############ Explain for Query ####################################
explain select SERVICE_STATE
from performance_schema.replication_applier_status_by_coordinator
where CHANNEL_NAME > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	replication_applier_status_by_coordinator	NULL	ALL	PRIMARY	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select SERVICE_STATE
from performance_schema.replication_applier_status_by_coordinator
where CHANNEL_NAME < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	replication_applier_status_by_coordinator	NULL	ALL	PRIMARY	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select SERVICE_STATE
from performance_schema.replication_applier_status_by_coordinator
where CHANNEL_NAME = @c_name;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	replication_applier_status_by_coordinator	NULL	const	PRIMARY	PRIMARY	256	const	#	100.00	NULL
############# Explain End #########################################
flush status;
select SERVICE_STATE
from performance_schema.replication_applier_status_by_coordinator
where CHANNEL_NAME = @c_name;
SERVICE_STATE
#
OK: handler_read_key incremented

====================================================================
Testing index for columns THREAD_ID
====================================================================
############ Explain for Query ####################################
explain select SERVICE_STATE 
from performance_schema.replication_applier_status_by_coordinator 
where THREAD_ID = "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	replication_applier_status_by_coordinator	NULL	ref	THREAD_ID	THREAD_ID	9	const	#	100.00	Using where
############ Explain for Query ####################################
explain select SERVICE_STATE
from performance_schema.replication_applier_status_by_coordinator
where THREAD_ID > "impossible";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	replication_applier_status_by_coordinator	NULL	ALL	THREAD_ID	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select SERVICE_STATE
from performance_schema.replication_applier_status_by_coordinator
where THREAD_ID < "2";
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	replication_applier_status_by_coordinator	NULL	ALL	THREAD_ID	NULL	NULL	NULL	#	33.33	Using where
############ Explain for Query ####################################
explain select SERVICE_STATE
from performance_schema.replication_applier_status_by_coordinator
where THREAD_ID = @t_id;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	replication_applier_status_by_coordinator	NULL	ref	THREAD_ID	THREAD_ID	9	const	#	100.00	NULL
############# Explain End #########################################
flush status;
select SERVICE_STATE
from performance_schema.replication_applier_status_by_coordinator
where THREAD_ID = @t_id;
SERVICE_STATE
#
OK: handler_read_key incremented
include/stop_slave.inc [FOR CHANNEL 'channel_1']
RESET SLAVE ALL;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
SET GLOBAL replica_parallel_workers= @old_replica_parallel_workers;
include/start_slave.inc
include/rpl_end.inc
