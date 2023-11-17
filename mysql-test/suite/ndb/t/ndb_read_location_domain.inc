# ndb_read_location_domain.inc
# - Assumes a 2 node, NoOfReplicas=2 cluster
# - Runs various types of table accesses and joins using ReadCommitted
#

# Fetch object ids for tables, unique index, and, blob tables.
select id into @t1_id from ndbinfo.ndb$dict_obj_info where fq_name = 'test/def/t1';
select id into @t1ui_id from ndbinfo.ndb$dict_obj_info where parent_obj_id = @t1_id and type = 3;
select id into @t2_id from ndbinfo.ndb$dict_obj_info where fq_name = 'test/def/t2';
select id into @t2ui_id from ndbinfo.ndb$dict_obj_info where parent_obj_id = @t2_id and type = 3;
select id into @t2bt_id from ndbinfo.ndb$dict_obj_info where parent_obj_id = @t2_id and fq_name like 'test/def/NDB$BLOB%';

# Prepare table t1 with data
--echo Put some data in.
--let $idx=$KEYS
--disable_query_log
while ($idx)
{
  --eval insert into test.t1 values ($idx,$idx,$idx,$idx)
  --dec $idx
}
--enable_query_log

--echo Make it more interesting for joining
update test.t1 set c=c+1, d=d+1;

# Count nodes belonging to some nodegroup
select count(distinct node_id) into @nodes
  from ndbinfo.ndb$membership where group_id < 0xFFFFFF00;

--echo Check replicas
select count(distinct node_id)
  from ndbinfo.operations_per_fragment
 where fq_name='test/def/t1'
   and fragment_num=0;

--echo Check nodes with table data
select count(distinct node_id)
  from ndbinfo.operations_per_fragment
  where fq_name='test/def/t1';

if (`select count(distinct node_id) <> @nodes from ndbinfo.operations_per_fragment where table_id = @t1_id`) {
  die "Table t1 have not data on all nodegroups!";
}

--echo Check UI replicas
select count(distinct node_id)
  from ndbinfo.operations_per_fragment
 where table_id = @t1ui_id
   and fragment_num=0;

--echo Check nodes with UI data
select count(distinct node_id)
  from ndbinfo.operations_per_fragment
  where table_id = @t1ui_id;

if (!$bug_ui) {
  if (`select count(distinct node_id) <> @nodes from ndbinfo.operations_per_fragment where table_id = @t1ui_id`) {
    die "Unique index for table t1 have not data on all nodegroups!";
} }

# Prepare table t2 with data

--echo Put some data in.
insert into test.t2 select a,b,c,repeat('BJC',300) from test.t1;

--echo Check blob table replicas
select count(distinct node_id)
  from ndbinfo.operations_per_fragment
 where table_id = @t2bt_id
   and fragment_num=0;

--echo Check nodes with blob data
select count(distinct node_id)
  from ndbinfo.operations_per_fragment
  where table_id = @t2bt_id;

if (!$bug_blob) {
  if (`select count(distinct node_id) <> @nodes from ndbinfo.operations_per_fragment where table_id = @t2bt_id`) {
    die "Blob table for table t2 have not data on all nodegroups!";
} }

#
# Create a table 'hints' with one row and partition per node in some node group.
# Note, there are no relation between the row values and node ids.
# All partitions are expected to have its primary replica on different nodes.
# Each partition will have one row with values for i from 1 to number of nodes.
#
let $nodes = `select @nodes`;
let $i = $nodes;
eval set @partitions = 'PARTITION p$i VALUES IN ($i)';
dec $i;
while ($i)
{
  eval set @partitions = concat(@partitions,',PARTITION p$i VALUES IN ($i)');
  dec $i;
}
let $partitions = `select @partitions`;
eval create table hints (i int primary key) engine=ndb partition by list(i) ($partitions);
let $i = $nodes;
while($i)
{
  eval insert into hints values ($i);
# Use 'eval select * from hints where i = $i;' first in a transaction to force
# connect to corresponding nodes DBTC.

--echo Hint node by $i of $nodes
--echo Tests on T1 (!Blobs)
--echo Test some basic NdbApi

select parent_fq_name, fq_name, type, node_id, sum(tot_key_reads), sum(tot_frag_scans)
  from ndbinfo.operations_per_fragment
  where (parent_fq_name='test/def/t1' OR fq_name='test/def/t1') and
        (tot_key_reads > 0 or tot_frag_scans > 0)
  group by parent_fq_name, fq_name, type, node_id;

--echo PK lookups
--replace_column 4 # 10 # 11 #
explain select * from test.t1 where t1.a=6;

select node_id, counter_name, sum(val) from ndbinfo.counters
  where counter_name = 'READS' or counter_name = 'LOCAL_READS'
  group by node_id, counter_name;

--let $idx=$KEYS
--disable_query_log
--disable_result_log
while ($idx)
{
  --eval select * from test.t1 where a=$idx
  --dec $idx
}
--enable_result_log
--enable_query_log
select parent_fq_name, fq_name, type, node_id, sum(tot_key_reads), sum(tot_frag_scans)
  from ndbinfo.operations_per_fragment
  where (parent_fq_name='test/def/t1' OR fq_name='test/def/t1') and
        (tot_key_reads > 0 or tot_frag_scans > 0)
  group by parent_fq_name, fq_name, type, node_id;

select node_id, counter_name, sum(val) from ndbinfo.counters
  where counter_name = 'READS' or counter_name = 'LOCAL_READS'
  group by node_id, counter_name;

if (!$bug_ui) {
--echo UI lookups
--replace_column 4 # 10 # 11 #
explain select * from test.t1 where t1.b=6;

--let $idx=$KEYS
--disable_query_log
--disable_result_log
while ($idx)
{
  --eval select * from test.t1 where b=$idx
  --dec $idx
}
--enable_result_log
--enable_query_log
select parent_fq_name, fq_name, type, node_id, sum(tot_key_reads), sum(tot_frag_scans)
  from ndbinfo.operations_per_fragment
  where (parent_fq_name='test/def/t1' OR fq_name='test/def/t1') and
        (tot_key_reads > 0 or tot_frag_scans > 0)
  group by parent_fq_name, fq_name, type, node_id;

select node_id, counter_name, sum(val) from ndbinfo.counters
  where counter_name = 'READS' or counter_name = 'LOCAL_READS'
  group by node_id, counter_name;
}

--echo OI scan
--replace_column 4 # 10 # 11 #
explain select * from test.t1 where c > 6;

--disable_query_log
--disable_result_log
select * from test.t1 where c > 6;
--enable_result_log
--enable_query_log
select parent_fq_name, fq_name, type, node_id, sum(tot_key_reads), sum(tot_frag_scans)
  from ndbinfo.operations_per_fragment
  where (parent_fq_name='test/def/t1' OR fq_name='test/def/t1') and
        (tot_key_reads > 0 or tot_frag_scans > 0)
  group by parent_fq_name, fq_name, type, node_id;

--echo Table scan
--replace_column 4 # 10 # 11 #
explain select * from test.t1;

--disable_query_log
--disable_result_log
select * from test.t1;
--enable_result_log
--enable_query_log
select parent_fq_name, fq_name, type, node_id, sum(tot_key_reads), sum(tot_frag_scans)
  from ndbinfo.operations_per_fragment
  where (parent_fq_name='test/def/t1' OR fq_name='test/def/t1') and
        (tot_key_reads > 0 or tot_frag_scans > 0)
  group by parent_fq_name, fq_name, type, node_id;

--echo Test pushed queries (SPJ)

--echo Pushed pk->pk
--echo Lookup to get a.d, feeding pk lookup of b.a
--replace_column 4 # 10 # 11 #
explain select a.a, a.d, b.d
   from
        test.t1 a
   join
        test.t1 b
     on
        a.d = b.a
  where a.a = 6;

--let $idx=$KEYS
--disable_query_log
--disable_result_log
while ($idx)
{
  --eval select a.a, a.d, b.d from test.t1 a join test.t1 b on a.d = b.a where a.a = $idx
  --dec $idx
}
--enable_result_log
--enable_query_log
select parent_fq_name, fq_name, type, node_id, sum(tot_key_reads), sum(tot_frag_scans)
  from ndbinfo.operations_per_fragment
  where (parent_fq_name='test/def/t1' OR fq_name='test/def/t1') and
        (tot_key_reads > 0 or tot_frag_scans > 0)
  group by parent_fq_name, fq_name, type, node_id;

--echo Pushed uk->uk
--echo Lookup to get a.d, feeding uk lookup of b.b
--replace_column 4 # 10 # 11 #
explain select a.a, a.d, b.d
   from
        test.t1 a
   join
        test.t1 b
     on
        a.d = b.b
  where a.b = 6;

--let $idx=$KEYS
--disable_query_log
--disable_result_log
while ($idx)
{
  --eval select a.a, a.d, b.d from test.t1 a join test.t1 b on a.d = b.b where a.b = $idx
  --dec $idx
}
--enable_result_log
--enable_query_log
select parent_fq_name, fq_name, type, node_id, sum(tot_key_reads), sum(tot_frag_scans)
  from ndbinfo.operations_per_fragment
  where (parent_fq_name='test/def/t1' OR fq_name='test/def/t1') and
        (tot_key_reads > 0 or tot_frag_scans > 0)
  group by parent_fq_name, fq_name, type, node_id;

--echo Pushed uk->pk
--echo Lookup to get a.d, feeding pk lookup of b.a
--replace_column 4 # 10 # 11 #
explain select a.a, a.d, b.d
   from
        test.t1 a
   join
        test.t1 b
     on
        a.d = b.a
  where a.b = 6;

--let $idx=$KEYS
--disable_query_log
--disable_result_log
while ($idx)
{
  --eval select a.a, a.d, b.d from test.t1 a join test.t1 b on a.d = b.a where a.b = $idx
  --dec $idx
}
--enable_result_log
--enable_query_log
select parent_fq_name, fq_name, type, node_id, sum(tot_key_reads), sum(tot_frag_scans)
  from ndbinfo.operations_per_fragment
  where (parent_fq_name='test/def/t1' OR fq_name='test/def/t1') and
        (tot_key_reads > 0 or tot_frag_scans > 0)
  group by parent_fq_name, fq_name, type, node_id;

--echo Pushed pk->uk
--echo Lookup to get a.d, feeding uk lookup of b.b
--replace_column 4 # 10 # 11 #
explain select a.a, a.d, b.d
   from
        test.t1 a
   join
        test.t1 b
     on
        a.d = b.b
  where a.a = 6;

--let $idx=$KEYS
--disable_query_log
--disable_result_log
while ($idx)
{
  --eval select a.a, a.d, b.d from test.t1 a join test.t1 b on a.d = b.b where a.a = $idx
  --dec $idx
}
--enable_result_log
--enable_query_log
select parent_fq_name, fq_name, type, node_id, sum(tot_key_reads), sum(tot_frag_scans)
  from ndbinfo.operations_per_fragment
  where (parent_fq_name='test/def/t1' OR fq_name='test/def/t1') and
        (tot_key_reads > 0 or tot_frag_scans > 0)
  group by parent_fq_name, fq_name, type, node_id;

--echo Pushed scan originating sub-lookups
--echo Table scan to get a.d, feeding pk lookups of b.a
--replace_column 4 # 10 # 11 #
explain select a.a, a.d, b.d
   from
        test.t1 a
   join
        test.t1 b
     on
        a.d = b.a;

--disable_result_log
select a.a, a.d, b.d
   from
        test.t1 a
   join
        test.t1 b
     on
        a.d = b.a;

--enable_result_log
select parent_fq_name, fq_name, type, node_id, sum(tot_key_reads), sum(tot_frag_scans)
  from ndbinfo.operations_per_fragment
  where (parent_fq_name='test/def/t1' OR fq_name='test/def/t1') and
        (tot_key_reads > 0 or tot_frag_scans > 0)
  group by parent_fq_name, fq_name, type, node_id;

--echo Pushed scan originating sub-lookups
--echo Table scan to get a.d, feeding uk lookups of b.b
--replace_column 4 # 10 # 11 #
explain select a.a, a.d, b.d
   from
        test.t1 a
   join
        test.t1 b
     on
        a.d = b.b;

--disable_result_log
select a.a, a.d, b.d
   from
        test.t1 a
   join
        test.t1 b
     on
        a.d = b.b;

--enable_result_log
select parent_fq_name, fq_name, type, node_id, sum(tot_key_reads), sum(tot_frag_scans)
  from ndbinfo.operations_per_fragment
  where (parent_fq_name='test/def/t1' OR fq_name='test/def/t1') and
        (tot_key_reads > 0 or tot_frag_scans > 0)
  group by parent_fq_name, fq_name, type, node_id;

--echo Pushed scan originating sub-scans
--echo Range scan on a.a > 10 getting a.d, feeding range scans on b.c = a.d

--replace_column 4 # 10 # 11 #
explain select a.a, a.d, b.d
   from
        test.t1 a
   join
        test.t1 b
     on
        b.c=a.d
  where
        a.a>10;

--disable_result_log
select a.a, a.d, b.d
   from
        test.t1 a
   join
        test.t1 b
     on
        b.c=a.d
  where
        a.a>10;

--enable_result_log
select parent_fq_name, fq_name, type, node_id, sum(tot_key_reads), sum(tot_frag_scans)
  from ndbinfo.operations_per_fragment
  where (parent_fq_name='test/def/t1' OR fq_name='test/def/t1') and
        (tot_key_reads > 0 or tot_frag_scans > 0)
  group by parent_fq_name, fq_name, type, node_id;

--echo Tests on T2 (Blobs)

select parent_fq_name, fq_name, type, node_id, sum(tot_key_reads), sum(tot_frag_scans)
  from ndbinfo.operations_per_fragment
  where (parent_fq_name='test/def/t2' OR fq_name='test/def/t2') and
        (tot_key_reads > 0 or tot_frag_scans > 0)
  group by parent_fq_name, fq_name, type, node_id;

select node_id, counter_name, sum(val) from ndbinfo.counters
  where counter_name = 'READS' or counter_name = 'LOCAL_READS'
  group by node_id, counter_name;

if (!$bug_blob)
{

--echo Test some basic NdbApi

--replace_column 4 # 10 # 11 #
explain select * from test.t2 where t2.a=6;

--let $idx=$KEYS
--disable_query_log
--disable_result_log
while ($idx)
{
  --eval select * from test.t2 where a=$idx
  --dec $idx
}
--enable_result_log
--enable_query_log
select parent_fq_name, fq_name, type, node_id, sum(tot_key_reads), sum(tot_frag_scans)
  from ndbinfo.operations_per_fragment
  where (parent_fq_name='test/def/t2' OR fq_name='test/def/t2') and
        (tot_key_reads > 0 or tot_frag_scans > 0)
  group by parent_fq_name, fq_name, type, node_id;

select node_id, counter_name, sum(val) from ndbinfo.counters
  where counter_name = 'READS' or counter_name = 'LOCAL_READS'
  group by node_id, counter_name;

--echo UI lookups
--replace_column 4 # 10 # 11 #
explain select * from test.t2 where t2.b=6;

--let $idx=$KEYS
--disable_query_log
--disable_result_log
while ($idx)
{
  --eval select * from test.t2 where b=$idx
  --dec $idx
}
--enable_result_log
--enable_query_log
select parent_fq_name, fq_name, type, node_id, sum(tot_key_reads), sum(tot_frag_scans)
  from ndbinfo.operations_per_fragment
  where (parent_fq_name='test/def/t2' OR fq_name='test/def/t2') and
        (tot_key_reads > 0 or tot_frag_scans > 0)
  group by parent_fq_name, fq_name, type, node_id;

select node_id, counter_name, sum(val) from ndbinfo.counters
  where counter_name = 'READS' or counter_name = 'LOCAL_READS'
  group by node_id, counter_name;

--echo OI scan
--replace_column 4 # 10 # 11 #
explain select * from test.t2 where c > 6;

--disable_query_log
--disable_result_log
select * from test.t2 where c > 6;
--enable_result_log
--enable_query_log
select parent_fq_name, fq_name, type, node_id, sum(tot_key_reads), sum(tot_frag_scans)
  from ndbinfo.operations_per_fragment
  where (parent_fq_name='test/def/t2' OR fq_name='test/def/t2') and
        (tot_key_reads > 0 or tot_frag_scans > 0)
  group by parent_fq_name, fq_name, type, node_id;

--echo Table scan
--replace_column 4 # 10 # 11 #
explain select * from test.t2;

--disable_query_log
--disable_result_log
select * from test.t2;
--enable_result_log
--enable_query_log
select parent_fq_name, fq_name, type, node_id, sum(tot_key_reads), sum(tot_frag_scans)
  from ndbinfo.operations_per_fragment
  where (parent_fq_name='test/def/t2' OR fq_name='test/def/t2') and
        (tot_key_reads > 0 or tot_frag_scans > 0)
  group by parent_fq_name, fq_name, type, node_id;
}

# Decrement $i to next node hint value and rerun all tests against a new node
  let $i=0;
}
drop table hints;

delete from t1;
delete from t2;
