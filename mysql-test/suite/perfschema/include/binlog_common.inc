# Tests for PERFORMANCE_SCHEMA

RESET MASTER;

select count(*) > 0 from performance_schema.setup_instruments;

# Note:
# Do not include records that could depend on
# compiling options (storage engines, SSL),
# to ensure the expected output in the binlog is predictable.
update performance_schema.setup_instruments set enabled='NO'
  where name like "wait/synch/rwlock/sql/%"
  and name not in ("wait/synch/rwlock/sql/CRYPTO_dynlock_value::lock");

select count(*) > 0 from performance_schema.events_waits_current;

--disable_warnings
drop table if exists test.t1;
drop table if exists test.t2;
--enable_warnings

create table test.t1 (thread_id integer);
create table test.t2 (name varchar(128));

insert into test.t1
  select thread_id from performance_schema.events_waits_current;

insert into test.t2
  select name from performance_schema.setup_instruments
    where name like "wait/synch/rwlock/sql/%"
    and name not in ("wait/synch/rwlock/sql/CRYPTO_dynlock_value::lock");

drop table test.t1;
drop table test.t2;

update performance_schema.setup_instruments set enabled='YES'
  where name like "wait/synch/rwlock/sql/%"
  and name not in ("wait/synch/rwlock/sql/CRYPTO_dynlock_value::lock");

--source include/show_binlog_events.inc

