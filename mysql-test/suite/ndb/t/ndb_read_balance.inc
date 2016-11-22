--echo
--echo
--echo
--echo *******************************************************
--echo ndb_read_balance.inc
--echo Table comment : $comment
--echo Keys : $KEYS
--echo Test_step : $TEST_STEP
--echo Expect : $expect
--echo *******************************************************
--echo

--echo Create table with int pk, unique secondary int and key int columns
--echo Expect that secondary unique is distributed in the same way as main table
--eval CREATE TABLE test.t1 (a int, b int, c int, d int, primary key (a), unique(b) using hash, key(c)) comment='$comment' engine=ndb;


--disable_query_log
--disable_result_log
--let $idx=$KEYS
while ($idx)
{
  --eval insert into test.t1 values ($idx, $idx, $idx, $idx)
  --dec $idx
}
--enable_result_log
--enable_query_log

#select * from ndbinfo.memory_per_fragment where fq_name='test/def/t1';
--echo Check that all fragments are in-use, else raise key count
select count(1) from ndbinfo.memory_per_fragment where fq_name='test/def/t1' and fixed_elem_count=0;

# Debug Solaris failure
#   Some empty fragments remain
--disable_query_log
--disable_result_log
select @ef:=count(1) from ndbinfo.memory_per_fragment where fq_name='test/def/t1' and fixed_elem_count=0;
let $empty_frags=query_get_value(select @ef as ef, ef, 1);

if ($empty_frags)
{
  --enable_query_log
  --enable_result_log
  select * from ndbinfo.memory_per_fragment where fq_name='test/def/t1';
  select * from test.t1;
  --disable_result_log
  --disable_query_log
}

--enable_query_log
--enable_result_log
# /Debug Solaris failure


--echo Build fragment identity table
--disable_query_log
--disable_result_log
create table fragments (node_id int, 
                        block_instance int, 
                        table_id int,
                        fragment_num int,
                        obj_type varchar(10),
                        prim_frag int, 
                        primary key (node_id, block_instance, table_id, fragment_num));

# select * from ndbinfo.operations_per_fragment where parent_fq_name='test/def/t1';

insert into fragments select node_id, block_instance, table_id, fragment_num, "TABLE", 0
    from ndbinfo.operations_per_fragment where fq_name='test/def/t1';
insert into fragments select node_id, block_instance, table_id, fragment_num, "OI_PRIMARY", 0
    from ndbinfo.operations_per_fragment 
   where parent_fq_name='test/def/t1' and 
         fq_name like '%PRIMARY';
insert into fragments select node_id, block_instance, table_id, fragment_num, "UI_B", 0
    from ndbinfo.operations_per_fragment 
   where parent_fq_name='test/def/t1' and 
         fq_name like '%b$unique';
insert into fragments select node_id, block_instance, table_id, fragment_num, "OI_C", 0
    from ndbinfo.operations_per_fragment 
   where parent_fq_name='test/def/t1' and 
         fq_name like '%c';

#select * from fragments order by fragment_num;

--let $idx=$KEYS
while ($idx)
{
  # Assume that FOR UPDATE reads primary replica
  begin;
  --eval select * from test.t1 where a=$idx for update;
  select @n:=node_id, @b:=block_instance, @f:=fragmentid from ndbinfo.server_locks;
  commit;
  update fragments set prim_frag=1 where node_id=@n and block_instance=@b and fragment_num=@f;
  --dec $idx
}

#select * from fragments order by fragment_num, node_id, table_id;
# Note that some fragments may be unrepresented in the data, and so have
# no primary marked


#select @pk:=ceiling(rand() * 10);

create view counters_of_interest as
  select node_id, block_instance, table_id, fragment_num, 
         tot_key_reads, 
         tot_key_inserts, tot_key_writes, tot_key_updates, tot_key_deletes, 
         tot_frag_scans 
  from ndbinfo.operations_per_fragment 
 where fq_name='test/def/t1' or 
       parent_fq_name='test/def/t1';

create table baseline as select * from counters_of_interest;

create view baseline_diff as 
  select a.node_id as node_id, 
         a.block_instance as block_instance, 
         a.table_id as table_id,
         a.fragment_num as fragment_num, 
         a.tot_key_reads - b.tot_key_reads as key_reads,
         a.tot_key_inserts - b.tot_key_inserts as key_inserts,
         a.tot_key_writes - b.tot_key_writes as key_writes,
         a.tot_key_updates - b.tot_key_updates as key_updates,
         a.tot_key_deletes - b.tot_key_deletes as key_deletes,
         a.tot_frag_scans - b.tot_frag_scans as frag_scans
    from 
         counters_of_interest a 
    join
         baseline b
      on 
         a.node_id = b.node_id  and
         a.block_instance = b.block_instance and
         a.fragment_num = b.fragment_num and
         a.table_id = b.table_id;

create view min_baseline_diff as
  select * from baseline_diff 
   where key_reads > 0 or
         key_inserts > 0 or
         key_writes > 0 or
         key_updates > 0 or
         key_deletes > 0 or
         frag_scans > 0;

create view min_baseline_diff_type as
  select t.obj_type,
         if(t.prim_frag=1, 'PRIMARY', 'BACKUP') as fragment_type,
         bl.key_reads,
         bl.key_inserts,
         bl.key_writes,
         bl.key_updates,
         bl.key_deletes,
         bl.frag_scans
    from 
         min_baseline_diff bl
    join
         fragments t
      on
         bl.node_id = t.node_id and
         bl.block_instance = t.block_instance and
         bl.table_id = t.table_id and
         bl.fragment_num = t.fragment_num;

create table unrelated(a int primary key) engine=ndb;

--enable_result_log
--enable_query_log

--echo Test lock free pk reads to show fragment types read
--let $idx=$KEYS

--disable_query_log
--disable_result_log
delete from baseline;
insert into baseline select * from counters_of_interest;
# Using a transaction, the first will hint to some node, so that some of
# the other reads will be remote
begin;
while ($idx)
{
  --eval select * from test.t1 where a=$idx;  
  --dec $idx
}
commit;
--enable_result_log
--enable_query_log

select distinct obj_type, fragment_type from min_baseline_diff_type order by obj_type, fragment_type;

--echo Test a number of scenarios to show read routing

--let $idx=$KEYS
while ($idx)
{
  --disable_query_log
  --echo
  --echo ****
  --echo Row with key : $idx
  --echo
  --echo Hinted PK read, no lock
  --disable_result_log
  delete from baseline;
  insert into baseline select * from counters_of_interest;
  --eval select * from test.t1 where a=$idx;
  --enable_result_log
  
  select * from min_baseline_diff_type;

  --echo
  --echo Unhinted PK read, shared lock
  --disable_result_log
  delete from baseline;
  insert into baseline select * from counters_of_interest;
  begin;
  select * from unrelated;
  --eval select * from test.t1 where a=$idx lock in share mode;
  commit;
  --enable_result_log
  
  select * from min_baseline_diff_type;
  
  --echo
  --echo Unhinted PK read, exclusive lock
  --disable_result_log
  delete from baseline;
  insert into baseline select * from counters_of_interest;
  begin;
  select * from unrelated;
  --eval select * from test.t1 where a=$idx for update;
  commit;
  --enable_result_log
  
  select * from min_baseline_diff_type;

  --echo 
  --echo Unhinted UI read, no lock
  --disable_result_log
  delete from baseline;
  insert into baseline select * from counters_of_interest;
  begin;
  select * from unrelated;
  --eval select * from test.t1 where b=$idx;
  commit;
  --enable_result_log
  
  select * from min_baseline_diff_type;

  --echo
  --echo Unhinted UI read, shared lock
  --disable_result_log
  delete from baseline;
  insert into baseline select * from counters_of_interest;
  begin;
  select * from unrelated;
  --eval select * from test.t1 where b=$idx lock in share mode;
  commit;
  --enable_result_log
  
  select * from min_baseline_diff_type;

  --echo
  --echo Unhinted UI read, exclusive lock
  --disable_result_log
  delete from baseline;
  insert into baseline select * from counters_of_interest;
  begin;
  select * from unrelated;
  --eval select * from test.t1 where b=$idx for update;
  commit;
  --enable_result_log
  
  select * from min_baseline_diff_type;

  --enable_query_log

  # Skip over some of the keys
  --let $i=$TEST_STEP
  while($i)
  {
    --dec $idx
    --dec $i
  }
}
--echo
--echo *********
--echo Scans

--disable_query_log
--echo
--echo Unhinted PK OI read, no lock
--disable_result_log
delete from baseline;
insert into baseline select * from counters_of_interest;
begin;
select * from unrelated;
--eval select * from test.t1 where a > 0;
commit;
--enable_result_log

select distinct obj_type, fragment_type from min_baseline_diff_type order by obj_type, fragment_type;

--echo
--echo Unhinted PK OI read, shared lock
--disable_result_log
delete from baseline;
insert into baseline select * from counters_of_interest;
begin;
select * from unrelated;
--eval select * from test.t1 where a > 0 lock in share mode;
commit;
--enable_result_log
  
select distinct obj_type, fragment_type from min_baseline_diff_type order by obj_type, fragment_type;

--echo
--echo Unhinted PK OI read, exclusive lock
--disable_result_log
delete from baseline;
insert into baseline select * from counters_of_interest;
begin;
select * from unrelated;
--eval select * from test.t1 where a > 0 for update;
commit;
--enable_result_log
  
select distinct obj_type, fragment_type from min_baseline_diff_type order by obj_type, fragment_type;


--echo
--echo Table scan, no lock
--disable_result_log
delete from baseline;
insert into baseline select * from counters_of_interest;
--eval select * from test.t1;
--enable_result_log
  
select distinct obj_type, fragment_type from min_baseline_diff_type order by obj_type, fragment_type;

--echo
--echo Table scan, shared lock
--disable_result_log
delete from baseline;
insert into baseline select * from counters_of_interest;
--eval select * from test.t1 lock in share mode;
--enable_result_log
  
select distinct obj_type, fragment_type from min_baseline_diff_type order by obj_type, fragment_type;

--echo
--echo Table scan, exclusive lock
--disable_result_log
delete from baseline;
insert into baseline select * from counters_of_interest;
--eval select * from test.t1 for update;
--enable_result_log
  
select distinct obj_type, fragment_type from min_baseline_diff_type order by obj_type, fragment_type;


drop table unrelated;
drop table fragments;
drop view counters_of_interest;
drop table baseline;
drop view baseline_diff;
drop view min_baseline_diff;
drop view min_baseline_diff_type;
drop table test.t1;
--enable_query_log
