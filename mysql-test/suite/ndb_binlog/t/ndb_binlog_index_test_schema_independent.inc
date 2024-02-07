
let have_next_pos=query_get_value(select count(1) as have_next_file from information_schema.COLUMNS where table_schema='mysql' and table_name='ndb_binlog_index' and column_name='next_file', have_next_file, 1);
let next_file_col=next_file;
let next_pos_col=next_position;

if (!$have_next_pos)
{
  let next_file_col="------"; # Just used in right($next_file_col, 6)
  let next_pos_col=1;         # Just used in ($next_pos_col = 0)
}

--echo mysql.ndb_binlog_index has next_file column? $have_next_pos

create table t1 (a int, b varchar(400)) engine=ndb;

#
# Test that entries in the mysql.ndb_binlog_index file correctly align
# with the offsets in the binlog
# Most testing is done with the next_position, as the position is
# subject to concurrency issues.
#

--echo Test pure epochs
--echo ----------------

insert into t1 values(1, repeat('B', 400));

--disable_query_log
--disable_result_log
show binlog events; # wait for binlog-sync and therefore epoch end
--enable_result_log
--enable_query_log

insert into t1 values(1, repeat('F', 400));

--disable_query_log
--disable_result_log
show binlog events; # wait for binlog-sync and therefore epoch end
--enable_result_log
--enable_query_log

insert into t1 values(1, repeat('E', 400));

--disable_query_log
--disable_result_log
show binlog events; # wait for binlog-sync and therefore epoch end
--enable_result_log
--enable_query_log

flush logs;
--source suite/ndb_binlog/t/ndb_binlog_check_binlog_index.inc

reset binary logs and gtids;

--echo Test interleaved epochs and DDL
--echo ------------------------------

insert into t1 values(1, repeat('R', 400));

create table t2 (a int) engine=ndb;

insert into t1 values(1, repeat('A', 400));

create table t3 (a int) engine=ndb;

insert into t1 value(1, repeat('A', 400));


flush logs;
--source suite/ndb_binlog/t/ndb_binlog_check_binlog_index.inc


--echo Test multithreaded interleaved epochs and DDL
--echo ---------------------------------------------
--echo Issue DDL and DML concurrently on server1
--echo They will interleave in the Binlog according to Binlog mutex
--echo interactions between DDL executing server thread and binlog injector
--echo
--echo Check Binlog on DDL-source MySQLD to ensure that binlog index positions
--echo 'cover' the Binlog
--echo Check Binlog on other MySQLD to ensure that binlog index positions
--echo 'cover' the Binlog (DDL here is 'fabricated' by Binlog injector thread
--echo

--connect (server1con1, 127.0.0.1,root,,test,$MASTER_MYPORT,)
--connect (server1con2, 127.0.0.1,root,,test,$MASTER_MYPORT,)
--connect (server2con1, 127.0.0.1,root,,test,$MASTER_MYPORT1,)

--connection server1con1
reset binary logs and gtids;

--connection server2con1
reset binary logs and gtids;

--connection server1con1

set sql_log_bin=0;

# Table to tell dmlload to stop
create table dmlload_stop (
  value int primary key
);

delimiter %;
create procedure dmlload ()
begin
  declare stop int default 0;
  repeat
    start transaction;
      insert into t1 values (2, repeat('I', 400));
    commit;
    start transaction;
      update t1 set b=repeat('Z', 400) where a=2;
    commit;
    start transaction;
      delete from t1 where a=2;
    commit;
    select value from dmlload_stop where value = 1 into stop;
  until stop
  end repeat;
end%


delimiter ;%

set sql_log_bin=1;

# Set DML running in 'background'
--connection server1con2
send call dmlload();

# Do DDL in 'foreground' until binlog size reach 2Mb.
# more sensible
--connection server1con1
let $i = 1;
let $continue = 1;
let $start_time= `select time_to_sec(current_time())`;
while ($continue)
{
  --disable_query_log
  create table fmc (a int) engine=myisam;
  create table bah(a int) engine=ndb;
  drop table bah;
  drop table fmc;
  --enable_query_log

  # Stop generating binlog when:
  # - at least 4 loops AND binlog size exceeds 2M (fast machine)
  #  OR
  # - 25 seconds has passed (slow machine, like with valgrind)
  # These limit are choosen to avoid that too much binlog is generated
  # which then take very long to analyze (using `mysqlbinlog --verbose` + load
  # into table and select).
  # For example fast machine generates 20M binlog during 25 seconds which
  # yields 600000 lines to analyze, while slow only generates 136k binlog
  # during those 25 seconds which are possible to analyze even on slow machine
  if ($i >= 4)
  {
    let $log_pos = query_get_value("SHOW BINARY LOGS", File_size, 1);
    if ($log_pos > 2000000)
    {
     #echo [$i]: Size of binlog, log_pos: $log_pos;
     let $continue = 0;
    }
  }
  if (`select time_to_sec(current_time()) >= $start_time + 25`)
  {
    #echo [$i]: time exceeded;
    #eval select $start_time, time_to_sec(current_time());
    let $continue = 0;
  }
  inc $i;
}
# Stop the DML load
insert into dmlload_stop values(1);

# Retrieve DML result
--connection server1con2
reap;

--connection server1con1

--echo Now check binlog index vs binlog itself on Server1
flush logs;
--source suite/ndb_binlog/t/ndb_binlog_check_binlog_index.inc

--connection server2con1
--echo Now check binlog index vs binlog itself on Server2
flush logs;
--source suite/ndb_binlog/t/ndb_binlog_check_binlog_index.inc

--connection server1con1

drop procedure dmlload;
drop table dmlload_stop;

--echo Cleanup
drop table t1;
drop table t2;
drop table t3;

--connection default
--disconnect server1con1
--disconnect server1con2
--disconnect server2con1
