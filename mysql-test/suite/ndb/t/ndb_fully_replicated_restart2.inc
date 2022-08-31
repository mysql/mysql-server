# Include file running tests for a specified
# restart type
# 
--echo ndb_fully_replicated_restart2.inc ($restart_type)

--echo Create FR table
use test;
create table test.t1 (
                      a int primary key, 
                      b int, 
                      c int, 
                      d int,
                      unique(c), key(d))
  comment="NDB_TABLE=FULLY_REPLICATED=1" engine=ndb;

--echo Put some stuff into it
--disable_query_log
--disable_result_log
--let $id=5120
while ($id)
{
  --eval insert into test.t1 values ($id,$id+1,$id+2,$id+3)
  --dec $id
}
--enable_result_log
--enable_query_log


--echo *************************
--echo Pre-restart checks
--echo *************************

--echo Check row count
select count(1) from test.t1 into @row_count;
select @row_count;

--echo Show physical rows per logical row (NoOfReplicas * NumNodeGroups)
select floor(sum(fixed_elem_count) / @row_count) as phys_rows_per_row
  from ndbinfo.memory_per_fragment 
 where fq_name='test/def/t1' into @prpr;
 select @prpr;

# Run checks on behaviour of FR table
--source ndb_fully_replicated_restart2_checks.inc


# Now perform a restart as specified by the caller

if ($restart_type == 'SR')
{
  --echo Now perform a system restart

  --exec $NDB_MGM --execute "all restart" >> $NDB_TOOLS_OUTPUT

  --echo Wait for SR to complete
  --exec $NDB_WAITER
}

if ($restart_type == 'NR')
{
  --echo Now perform a rolling restart
  # RR is more interesting than a single NR as it restarts every node, so any
  # required runtime state must be correctly rebuilt

  --echo Node 1
  --exec $NDB_MGM --execute "1 restart" >> $NDB_TOOLS_OUTPUT
  --exec $NDB_WAITER

  --echo Node 2
  --exec $NDB_MGM --execute "2 restart" >> $NDB_TOOLS_OUTPUT
  --exec $NDB_WAITER

  --echo Node 3
  --exec $NDB_MGM --execute "3 restart" >> $NDB_TOOLS_OUTPUT
  --exec $NDB_WAITER

  --echo Node 4
  --exec $NDB_MGM --execute "4 restart" >> $NDB_TOOLS_OUTPUT
  --exec $NDB_WAITER
}

if ($restart_type == 'SRTO')
{
  --echo Now perform SR with takeover

  --echo Node 1 restart -n
  --exec $NDB_MGM --execute "1 restart -n" >> $NDB_TOOLS_OUTPUT
  --echo Wait for Node 1 to enter NOSTART state
  --exec $NDB_WAITER --not-started --wait-nodes=1

  --echo Now make some change and trigger some LCP action
  # Potentially improve this to ensure LCPs complete
  --exec $NDB_MGM --execute "ALL DUMP 7099" >> $NDB_TOOLS_OUTPUT
  --sleep 10
  --exec $NDB_MGM --execute "ALL DUMP 7099" >> $NDB_TOOLS_OUTPUT
  --sleep 10
  --exec $NDB_MGM --execute "ALL DUMP 7099" >> $NDB_TOOLS_OUTPUT
  --sleep 10

  --echo Now bring down the other nodes
  --exec $NDB_MGM --execute "ALL restart -n" >> $NDB_TOOLS_OUTPUT

  --echo Wait for them to come down
  --exec $NDB_WAITER --not-started

  --echo Now start-em up
  --exec $NDB_MGM --execute "all start" >> $NDB_TOOLS_OUTPUT

  --echo Wait for SR to complete
  --exec $NDB_WAITER
}


--echo --- Wait to establish connection
--source include/ndb_not_readonly.inc


--echo **************************
--echo Post-restart checks
--echo **************************


# Run checks on behaviour of FR table
--source ndb_fully_replicated_restart2_checks.inc


--echo Delete the lot
delete from test.t1 limit 1024;
delete from test.t1 limit 1024;
delete from test.t1 limit 1024;
delete from test.t1 limit 1024;
delete from test.t1 limit 1024;
delete from test.t1 limit 1024;

select count(1) from test.t1 into @new_row_count;
select @new_row_count;

select sum(fixed_elem_count) from ndbinfo.memory_per_fragment where fq_name='test/def/t1';

#--echo Show fragment balance
#select avg(fixed_elem_count), std(fixed_elem_count), min(fixed_elem_count), max(fixed_elem_count) from ndbinfo.memory_per_fragment where fq_name='test/def/t1';

--echo Check new Physical row count against expected

select sum(fixed_elem_count) from ndbinfo.memory_per_fragment where fq_name='test/def/t1' into @sum;
set @diff = (@new_row_count * @prpr) - @sum;
select floor(@new_row_count * @prpr) as expected_phys_rows, 
       @sum as actual_phys_rows,
       @diff as diff;

--let $d=query_get_value(select @diff as d, d, 1)

if ($d)
{
  --echo Show actual fragments
  select * from ndbinfo.memory_per_fragment 
   where fq_name='test/def/t1' 
   order by 
         node_id, fragment_num;
  --die Physical vs Logical row count error
}

--echo Check UI physical row count against expected

select sum(fixed_elem_count) from ndbinfo.memory_per_fragment where parent_fq_name='test/def/t1' and fq_name like '%/c$unique' into @sum;
set @diff = (@new_row_count * @prpr) - @sum;
select floor(@new_row_count * @prpr) as expected_phys_rows, 
       @sum as actual_phys_rows,
       @diff as diff;

drop table test.t1;
