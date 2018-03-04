select component_urn from mysql.component;
component_urn
INSTALL COMPONENT "file://component_pfs_example";
select component_urn from mysql.component;
component_urn
file://component_pfs_example
select * from performance_schema.setup_instruments
where NAME like "%/pfs_example/%";
NAME	ENABLED	TIMED	PROPERTIES	VOLATILITY	DOCUMENTATION
wait/synch/cond/pfs_example/X	YES	YES		0	NULL
wait/synch/cond/pfs_example/Y	YES	YES		0	NULL
wait/synch/mutex/pfs_example/T	YES	YES		3	And more.
wait/synch/mutex/pfs_example/X	YES	YES	singleton	1	Example doc, permanent mutex, singleton.
wait/synch/mutex/pfs_example/Y	YES	YES		7	Example doc, very volatile mutexes.
wait/synch/mutex/pfs_example/Z	YES	YES	singleton	2	Another example.
wait/synch/rwlock/pfs_example/T	YES	YES		0	NULL
wait/synch/rwlock/pfs_example/X	YES	YES		0	NULL
wait/synch/rwlock/pfs_example/Y	YES	YES		0	NULL
wait/synch/rwlock/pfs_example/Z	YES	YES		0	NULL
wait/synch/sxlock/pfs_example/S1	YES	YES		0	NULL
wait/synch/sxlock/pfs_example/S2	YES	YES		0	NULL
select NAME, LOCKED_BY_THREAD_ID is not NULL
from performance_schema.mutex_instances
where NAME like "wait/synch/mutex/pfs_example/%";
NAME	LOCKED_BY_THREAD_ID is not NULL
wait/synch/mutex/pfs_example/T	0
wait/synch/mutex/pfs_example/X	0
wait/synch/mutex/pfs_example/Y	0
wait/synch/mutex/pfs_example/Z	0
select NAME, WRITE_LOCKED_BY_THREAD_ID is not NULL, READ_LOCKED_BY_COUNT
from performance_schema.rwlock_instances
where NAME like "wait/synch/%/pfs_example/%";
NAME	WRITE_LOCKED_BY_THREAD_ID is not NULL	READ_LOCKED_BY_COUNT
wait/synch/rwlock/pfs_example/T	0	0
wait/synch/rwlock/pfs_example/X	0	0
wait/synch/rwlock/pfs_example/Y	0	0
wait/synch/rwlock/pfs_example/Z	0	0
wait/synch/sxlock/pfs_example/S1	0	0
wait/synch/sxlock/pfs_example/S2	0	0
select NAME
from performance_schema.cond_instances
where NAME like "wait/synch/%/pfs_example/%";
NAME
wait/synch/cond/pfs_example/X
wait/synch/cond/pfs_example/Y
UNINSTALL COMPONENT "file://component_pfs_example";
select component_urn from mysql.component;
component_urn
select * from performance_schema.setup_instruments
where NAME like "wait/synch/mutex/pfs_example/%";
NAME	ENABLED	TIMED	PROPERTIES	VOLATILITY	DOCUMENTATION
wait/synch/mutex/pfs_example/T	YES	YES		3	And more.
wait/synch/mutex/pfs_example/X	YES	YES	singleton	1	Example doc, permanent mutex, singleton.
wait/synch/mutex/pfs_example/Y	YES	YES		7	Example doc, very volatile mutexes.
wait/synch/mutex/pfs_example/Z	YES	YES	singleton	2	Another example.
select *
from performance_schema.mutex_instances
where NAME like "%/pfs_example/%";
NAME	OBJECT_INSTANCE_BEGIN	LOCKED_BY_THREAD_ID
select *
from performance_schema.rwlock_instances
where NAME like "%/pfs_example/%";
NAME	OBJECT_INSTANCE_BEGIN	WRITE_LOCKED_BY_THREAD_ID	READ_LOCKED_BY_COUNT
select *
from performance_schema.cond_instances
where NAME like "%/pfs_example/%";
NAME	OBJECT_INSTANCE_BEGIN
select * from performance_schema.global_status
where variable_name like "performance_schema%classes_lost";
VARIABLE_NAME	VARIABLE_VALUE
Performance_schema_cond_classes_lost	0
Performance_schema_file_classes_lost	0
Performance_schema_memory_classes_lost	0
Performance_schema_mutex_classes_lost	0
Performance_schema_rwlock_classes_lost	0
Performance_schema_socket_classes_lost	0
Performance_schema_stage_classes_lost	0
Performance_schema_statement_classes_lost	0
Performance_schema_thread_classes_lost	0
