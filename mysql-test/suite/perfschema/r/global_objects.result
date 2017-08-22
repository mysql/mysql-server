select count(*) from performance_schema.setup_instruments
where enabled='YES'
  and name like "wait/%";
count(*)
0
select count(*) > 0 from performance_schema.mutex_instances;
count(*) > 0
1
select count(*) > 0 from performance_schema.rwlock_instances;
count(*) > 0
1
select count(*) > 0 from performance_schema.cond_instances;
count(*) > 0
1
select count(*) > 0 from performance_schema.socket_instances;
count(*) > 0
1
select NAME from performance_schema.mutex_instances
where NAME = "wait/synch/mutex/sql/tz_LOCK";
NAME
wait/synch/mutex/sql/tz_LOCK
select NAME from performance_schema.rwlock_instances
where NAME = "wait/synch/rwlock/sql/LOCK_system_variables_hash";
NAME
wait/synch/rwlock/sql/LOCK_system_variables_hash
select NAME from performance_schema.cond_instances
where NAME = "wait/synch/cond/sql/COND_server_started";
NAME
wait/synch/cond/sql/COND_server_started
select EVENT_NAME from performance_schema.socket_instances
where EVENT_NAME = "wait/io/socket/sql/client_connection";
EVENT_NAME
wait/io/socket/sql/client_connection
