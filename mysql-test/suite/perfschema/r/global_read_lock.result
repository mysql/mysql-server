use performance_schema;
update performance_schema.setup_instruments set enabled='YES';
create user pfsuser@localhost;
grant SELECT, UPDATE, LOCK TABLES on performance_schema.* to pfsuser@localhost;
flush privileges;
connect (con1, localhost, pfsuser, , test);
lock tables performance_schema.setup_instruments read;
select * from performance_schema.setup_instruments;
unlock tables;
lock tables performance_schema.setup_instruments write;
update performance_schema.setup_instruments set enabled='NO';
update performance_schema.setup_instruments set enabled='YES';
unlock tables;
connection default;
flush tables with read lock;
connection con1;
lock tables performance_schema.setup_instruments read;
select * from performance_schema.setup_instruments;
unlock tables;
lock tables performance_schema.setup_instruments write;
connection default;
select event_name,
left(source, locate(":", source)) as short_source,
if(timer_end IS NULL, NULL, "SET") as timer_end,
if(timer_wait IS NULL, NULL, "SET") as timer_wait,
operation
from performance_schema.events_waits_current
where event_name like "wait/synch/cond/sql/MDL_context::COND_wait_status";
event_name	short_source	timer_end	timer_wait	operation
wait/synch/cond/sql/MDL_context::COND_wait_status	mdl.cc:	SET	SET	timed_wait
unlock tables;
update performance_schema.setup_instruments set enabled='NO';
update performance_schema.setup_instruments set enabled='YES';
unlock tables;
connection default;
drop user pfsuser@localhost;
flush privileges;
