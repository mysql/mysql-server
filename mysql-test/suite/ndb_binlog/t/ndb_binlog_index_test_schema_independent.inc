
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

reset master;

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
reset master;

--connection server2con1
reset master;

--connection server1con1

set sql_log_bin=0;
delimiter %;
create procedure dmlload (seconds int)
begin
  set @x=time_to_sec(current_time()) + seconds;
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
  until @x <= time_to_sec(current_time())
  end repeat;
end%

create procedure ddlload(seconds int)
begin
  set @x=time_to_sec(current_time()) + seconds;
  repeat
    create table fmc (a int) engine=myisam;
    create table bah(a int) engine=ndb;
    drop table bah;
    drop table fmc;
  until @x <= time_to_sec(current_time())
  end repeat;
end%

delimiter ;%

set sql_log_bin=1;

# Set DDL running in 'background'
--connection server1con2
send call ddlload(25);

# And do DML in 'foreground'
--connection server1con1
call dmlload(25);

# Retrieve DDL result
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

drop procedure ddlload;
drop procedure dmlload;


--echo Cleanup
drop table t1;
drop table t2;
drop table t3;

--connection default
--disconnect server1con1
--disconnect server1con2
--disconnect server2con1
