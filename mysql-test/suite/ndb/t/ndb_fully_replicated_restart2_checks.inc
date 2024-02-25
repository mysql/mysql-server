--echo Check row count
select count(1) from test.t1 into @row_count;
select @row_count;


--echo Check Physical row count against expected
select sum(fixed_elem_count) from ndbinfo.memory_per_fragment where fq_name='test/def/t1' into @sum;
set @diff = (@row_count * @prpr) - @sum;
select floor(@row_count * @prpr) as expected_phys_rows, 
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
set @diff = (@row_count * @prpr) - @sum;
select floor(@row_count * @prpr) as expected_phys_rows,
       @sum as actual_phys_rows,
       @diff as diff;

#--echo Show actual fragments
#select * from ndbinfo.memory_per_fragment 
# where fq_name='test/def/t1' 
# order by 
#       node_id, fragment_num;
#
## indices
#select * from ndbinfo.memory_per_fragment 
# where parent_fq_name='test/def/t1' 
# order by 
#       node_id, fragment_num;

#--echo Show fragment balance
#select avg(fixed_elem_count), 
#       std(fixed_elem_count), 
#       min(fixed_elem_count), 
#       max(fixed_elem_count) 
#  from ndbinfo.memory_per_fragment 
# where fq_name='test/def/t1';

--echo Delete some data
delete from test.t1 limit 512;

--echo Show new logical row count
select count(1) from test.t1 into @new_row_count;
select @new_row_count;

#--echo Show fragment balance
#select avg(fixed_elem_count), 
#       std(fixed_elem_count), 
#       min(fixed_elem_count), 
#       max(fixed_elem_count) 
#  from ndbinfo.memory_per_fragment 
# where fq_name='test/def/t1';

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
--disable_query_log
--disable_result_log
select @m:=max(a) from test.t1;

--echo Insert some data
insert into test.t1 select a+@m, b+@m, c+@m, d+@m  from test.t1 limit 800;
--enable_query_log
--enable_result_log

--echo Show new logical row count
select count(1) from test.t1 into @new_row_count;
select @new_row_count;

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

--echo Show read-locality at all data nodes

# Handy table to allow cluster_transactions to show
# the TC node chosen for each iteration
create table test.t2(a int primary key) engine=ndb;
insert into test.t2 values (1);

create table nodes(id int primary key);

# Enough iterations to hopefully run on each node
--let $ITERATIONS=20

--disable_query_log
--disable_result_log

while($ITERATIONS)
{
  begin;
  select * from mysql.ndb_apply_status;
  select * from test.t2 for update;
  
#  --echo Error insert to ensure that TC requests are purely local
  --exec $NDB_MGM -e "ALL ERROR 8083" >> $NDB_TOOLS_OUTPUT
  # PK
  select * from test.t1 where a = 100000;

  # STILL DISABLED as UK read is not always local
  # UK
  #select * from test.t1 where c= 100000;

  # PK OI scan
  select * from test.t1 where a > 100000;  

  # Secondary OI scan
  select * from test.t1 where d > 100000;

  # Pushed PK -> PK join
  select * 
    from test.t1 a join test.t1 b
       on a.d = b.a
  where a.a=2;

  # Pushed PK -> UK join
  select * 
    from test.t1 a join test.t1 b
       on a.d = b.c
  where a.a=2;
  
  # Pushed UK -> UK join
  select * 
    from test.t1 a join test.t1 b
       on a.d = b.c
  where a.c=2;

  # Pushed OI -> PK join
  select *
     from test.t1 a join test.t1 b
        on a.b = b.a
   where a.d < 5;

  # Pushed UI -> OI join
  select *
     from test.t1 a join test.t1 b
        on a.b = b.c
   where a.a=2;

  # Pushed OI -> OI join
  select *
     from test.t1 a join test.t1 b
        on b.d = a.c
    where a.d < 6;



  --exec $NDB_MGM -e "ALL ERROR 0" >> $NDB_TOOLS_OUTPUT
  replace into nodes select node_id from ndbinfo.cluster_transactions;
  commit;
  --dec $ITERATIONS
}
--enable_query_log
--enable_result_log

--echo Expect that the transaction ran on all 4 nodes :
select count(1) from nodes;
drop table nodes;

drop table test.t2;
