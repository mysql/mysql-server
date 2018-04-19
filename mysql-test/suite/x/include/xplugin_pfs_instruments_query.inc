# Gathering Reports for Xplugin Instruments in Performance 
# Schema

# Following test fragment needs to be reworked
#select DISTINCT EVENT_NAME from performance_schema.events_waits_current 
#where EVENT_NAME like '%mysqlx%'
#ORDER BY EVENT_NAME; 

-->sql
select DISTINCT EVENT_NAME from performance_schema.events_waits_history 
where EVENT_NAME like '%mysqlx%'
ORDER BY EVENT_NAME;

select DISTINCT EVENT_NAME from performance_schema.events_waits_history_long 
where EVENT_NAME like '%mysqlx%'
ORDER BY EVENT_NAME;

SELECT DISTINCT name FROM performance_schema.threads 
WHERE name LIKE 'thread/mysqlx/%' 
ORDER BY name;

SELECT DISTINCT name FROM performance_schema.mutex_instances
WHERE name LIKE 'wait/synch/mutex/mysqlx/%' 
ORDER BY name;

SELECT DISTINCT name FROM performance_schema.cond_instances
WHERE name LIKE 'wait/synch/cond/mysqlx/%' 
ORDER BY name;

SELECT DISTINCT name FROM performance_schema.rwlock_instances
WHERE name LIKE 'wait/synch/rwlock/mysqlx/%' 
ORDER BY name;
-->endsql

