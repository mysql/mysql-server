# ndb_read_local.inc
# - Assumes a 2 node, NoOfReplicas=2 cluster
# - Runs various types of table accesses and joins using ReadCommitted
# - Uses ERROR_INSERT codes in TC and SPJ that check that all
#   reads are TC-local (e.g. use TC local SPJ + LQH instances)
#   If there are any non-local reads, then a data node will fail with
#   ndbrequire(false), causing testcase failure.
# - This verifies that the benefit of locality is available for
#   Read_Backup and FullyReplicated on 1 NG clusters.
#
# TODO : Same test for FullyReplicated on > 1 NG clusters...

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
set @@new = 1;
eval create table hints (i int primary key) engine=ndb partition by list(i) ($partitions);
let $i = $nodes;
while ($i)
{
  eval insert into hints values ($i);
# Use 'eval select * from hints where i = $i;' first in a transaction to force
# connect to corresponding nodes DBTC.

--echo Hint node by $i of $nodes

--echo Tests on T1 (!Blobs)


--echo Test some basic NdbApi



--echo PK lookups
--replace_column 4 # 10 # 11 #
explain select * from test.t1 where t1.a=6;

--echo Error insert to ensure that TC requests are purely local
--exec $NDB_MGM --verbose=0 -e "ALL ERROR 8083"

--let $idx=$KEYS
--disable_query_log
--disable_result_log
begin; # So all reads are from one node, defeating hinting
eval select * from hints where i = $i;
while ($idx)
{
  --eval select * from test.t1 where a=$idx
  --dec $idx
}
commit;
--enable_result_log
--enable_query_log

--exec $NDB_MGM --verbose=0 -e "ALL ERROR 0"



if (!$bug_ui) {
--echo UI lookups
--replace_column 4 # 10 # 11 #
explain select * from test.t1 where t1.b=6;

--echo Error insert to ensure that TC requests are purely local
--exec $NDB_MGM --verbose=0 -e "ALL ERROR 8083"

--let $idx=$KEYS
--disable_query_log
--disable_result_log
begin; # So all reads are from one node, defeating hinting
eval select * from hints where i = $i;
while ($idx)
{
  --eval select * from test.t1 where b=$idx
  --dec $idx
}
commit;
--enable_result_log
--enable_query_log

--exec $NDB_MGM --verbose=0 -e "ALL ERROR 0"
}

--echo OI scan
--replace_column 4 # 10 # 11 #
explain select * from test.t1 where c > 6;

--echo Error insert to ensure that TC requests are purely local
--exec $NDB_MGM --verbose=0 -e "ALL ERROR 8083"

--disable_query_log
--disable_result_log
begin;
eval select * from hints where i = $i;
select * from test.t1 where c > 6;
commit;
--enable_result_log
--enable_query_log

--exec $NDB_MGM --verbose=0 -e "ALL ERROR 0"

--echo Table scan
--replace_column 4 # 10 # 11 #
explain select * from test.t1;

--echo Error insert to ensure that TC requests are purely local
--exec $NDB_MGM --verbose=0 -e "ALL ERROR 8083"

--disable_query_log
--disable_result_log
begin;
eval select * from hints where i = $i;
select * from test.t1;
commit;
--enable_result_log
--enable_query_log

--exec $NDB_MGM --verbose=0 -e "ALL ERROR 0"



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

--echo Error insert to ensure that SPJ requests are purely local
--exec $NDB_MGM --verbose=0 -e "ALL ERROR 17014"

--let $idx=$KEYS
--disable_query_log
--disable_result_log
begin;
eval select * from hints where i = $i;
while ($idx)
{
  --eval select a.a, a.d, b.d from test.t1 a join test.t1 b on a.d = b.a where a.a = $idx
  --dec $idx
}
commit;
--enable_result_log
--enable_query_log

--exec $NDB_MGM --verbose=0 -e "ALL ERROR 0"


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

--echo Error insert to ensure that SPJ requests are purely local
--exec $NDB_MGM --verbose=0 -e "ALL ERROR 17014"

--let $idx=$KEYS
--disable_query_log
--disable_result_log
begin;
eval select * from hints where i = $i;
while ($idx)
{
  --eval select a.a, a.d, b.d from test.t1 a join test.t1 b on a.d = b.b where a.b = $idx
  --dec $idx
}
commit;
--enable_result_log
--enable_query_log

--exec $NDB_MGM --verbose=0 -e "ALL ERROR 0"

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

--echo Error insert to ensure that SPJ requests are purely local
--exec $NDB_MGM --verbose=0 -e "ALL ERROR 17014"

--let $idx=$KEYS
--disable_query_log
--disable_result_log
begin;
eval select * from hints where i = $i;
while ($idx)
{
  --eval select a.a, a.d, b.d from test.t1 a join test.t1 b on a.d = b.a where a.b = $idx
  --dec $idx
}
commit;
--enable_result_log
--enable_query_log

--exec $NDB_MGM --verbose=0 -e "ALL ERROR 0"

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

--echo Error insert to ensure that SPJ requests are purely local
--exec $NDB_MGM --verbose=0 -e "ALL ERROR 17014"

--let $idx=$KEYS
--disable_query_log
--disable_result_log
begin;
eval select * from hints where i = $i;
while ($idx)
{
  --eval select a.a, a.d, b.d from test.t1 a join test.t1 b on a.d = b.b where a.a = $idx
  --dec $idx
}
commit;
--enable_result_log
--enable_query_log

--exec $NDB_MGM --verbose=0 -e "ALL ERROR 0"



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

--echo Error insert to ensure that SPJ requests are purely local
--exec $NDB_MGM --verbose=0 -e "ALL ERROR 17014"

--disable_result_log

begin;
eval select * from hints where i = $i;
select a.a, a.d, b.d
   from
        test.t1 a
   join
        test.t1 b
     on
        a.d = b.a;
commit;

--enable_result_log


--exec $NDB_MGM --verbose=0 -e "ALL ERROR 0"



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

--echo Error insert to ensure that SPJ requests are purely local
--exec $NDB_MGM --verbose=0 -e "ALL ERROR 17014"

--disable_result_log

begin;
eval select * from hints where i = $i;
select a.a, a.d, b.d
   from
        test.t1 a
   join
        test.t1 b
     on
        a.d = b.b;
commit;
--enable_result_log


--exec $NDB_MGM --verbose=0 -e "ALL ERROR 0"

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

--echo Error insert to ensure that SPJ requests are purely local
--exec $NDB_MGM --verbose=0 -e "ALL ERROR 17014"

--disable_result_log

begin;
eval select * from hints where i = $i;
select a.a, a.d, b.d
   from
        test.t1 a
   join
        test.t1 b
     on
        b.c=a.d
  where
        a.a>10;
commit;

--enable_result_log


--exec $NDB_MGM --verbose=0 -e "ALL ERROR 0"


--echo Tests on T2 (Blobs)

if (!$bug_blob)
{

--echo Test some basic NdbApi

--replace_column 4 # 10 # 11 #
explain select * from test.t2 where t2.a=6;

--echo Error insert to ensure that TC requests are purely local
--exec $NDB_MGM --verbose=0 -e "ALL ERROR 8083"

--let $idx=$KEYS
--disable_query_log
--disable_result_log
begin; # So all reads are from one node, defeating hinting
eval select * from hints where i = $i;
while ($idx)
{
  --eval select * from test.t2 where a=$idx
  --dec $idx
}
commit;
--enable_result_log
--enable_query_log

--exec $NDB_MGM --verbose=0 -e "ALL ERROR 0"


--echo UI lookups
--replace_column 4 # 10 # 11 #
explain select * from test.t2 where t2.b=6;

--echo Error insert to ensure that TC requests are purely local
--exec $NDB_MGM --verbose=0 -e "ALL ERROR 8083"

--let $idx=$KEYS
--disable_query_log
--disable_result_log
begin; # So all reads are from one node, defeating hinting
eval select * from hints where i = $i;
while ($idx)
{
  --eval select * from test.t2 where b=$idx
  --dec $idx
}
commit;
--enable_result_log
--enable_query_log

--exec $NDB_MGM --verbose=0 -e "ALL ERROR 0"

--echo OI scan
--replace_column 4 # 10 # 11 #
explain select * from test.t2 where c > 6;

--echo Error insert to ensure that TC requests are purely local
--exec $NDB_MGM --verbose=0 -e "ALL ERROR 8083"

--disable_query_log
--disable_result_log
begin;
eval select * from hints where i = $i;
select * from test.t2 where c > 6;
commit;
--enable_result_log
--enable_query_log

--exec $NDB_MGM --verbose=0 -e "ALL ERROR 0"

--echo Table scan
--replace_column 4 # 10 # 11 #
explain select * from test.t2;

--echo Error insert to ensure that TC requests are purely local
--exec $NDB_MGM --verbose=0 -e "ALL ERROR 8083"

--disable_query_log
--disable_result_log
begin;
eval select * from hints where i = $i;
select * from test.t2;
commit;
--enable_result_log
--enable_query_log

--exec $NDB_MGM --verbose=0 -e "ALL ERROR 0"
}

# Decrement $i to next node hint value and rerun all tests against a new node
  dec $i;
}
drop table hints;

delete from t1;
delete from t2;

