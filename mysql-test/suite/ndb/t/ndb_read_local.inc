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
# TODO : UK combinations : But UK is currently broken

--echo Tests on T1 (!Blobs)
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

--echo Check 2 replicas 
select count(distinct node_id) 
  from ndbinfo.operations_per_fragment
 where fq_name='test/def/t1'
   and fragment_num=0;

--echo Check 1 NG (2 replicas + only 2 nodes)
select count(distinct node_id)
  from ndbinfo.operations_per_fragment;



--echo Test some basic NdbApi



--echo PK lookups
--replace_column 4 # 10 # 11 #
explain select * from test.t1 where t1.a=6;

--echo Error insert to ensure that TC requests are purely local
--exec $NDB_MGM -e "ALL ERROR 8083" >> $NDB_TOOLS_OUTPUT

--let $idx=$KEYS
--disable_query_log
--disable_result_log
begin; # So all reads are from one node, defeating hinting
while ($idx)
{
  --eval select * from test.t1 where a=$idx
  --dec $idx
}
commit;
--enable_result_log
--enable_query_log

--exec $NDB_MGM -e "ALL ERROR 0" >> $NDB_TOOLS_OUTPUT




# This currently works due to something in the fix for
# Bug #23001841 READS IN READ_COMMITTED MODE ALWAYS ABLE TO READ BACKUP REPLICA

--echo UI lookups
--replace_column 4 # 10 # 11 #
explain select * from test.t1 where t1.b=6;

--echo Error insert to ensure that TC requests are purely local
--exec $NDB_MGM -e "ALL ERROR 8083" >> $NDB_TOOLS_OUTPUT

--let $idx=$KEYS
--disable_query_log
--disable_result_log
begin; # So all reads are from one node, defeating hinting
while ($idx)
{
  --eval select * from test.t1 where b=$idx
  --dec $idx
}
commit;
--enable_result_log
--enable_query_log

--exec $NDB_MGM -e "ALL ERROR 0" >> $NDB_TOOLS_OUTPUT





--echo OI scan
--replace_column 4 # 10 # 11 #
explain select * from test.t1 where c > 6;

--echo Error insert to ensure that TC requests are purely local
--exec $NDB_MGM -e "ALL ERROR 8083" >> $NDB_TOOLS_OUTPUT

--disable_query_log
--disable_result_log
select * from test.t1 where c > 6;
--enable_result_log
--enable_query_log

--exec $NDB_MGM -e "ALL ERROR 0" >> $NDB_TOOLS_OUTPUT





--echo Table scan
--replace_column 4 # 10 # 11 #
explain select * from test.t1;

--echo Error insert to ensure that TC requests are purely local
--exec $NDB_MGM -e "ALL ERROR 8083" >> $NDB_TOOLS_OUTPUT

--disable_query_log
--disable_result_log
select * from test.t1;
--enable_result_log
--enable_query_log

--exec $NDB_MGM -e "ALL ERROR 0" >> $NDB_TOOLS_OUTPUT








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
--exec $NDB_MGM -e "ALL ERROR 17014" >> $NDB_TOOLS_OUTPUT

--let $idx=$KEYS
--disable_query_log
--disable_result_log
begin;
while ($idx)
{
  --eval select a.a, a.d, b.d from test.t1 a join test.t1 b on a.d = b.a where a.a = $idx
  --dec $idx
}
commit;
--enable_result_log
--enable_query_log

--exec $NDB_MGM -e "ALL ERROR 0" >> $NDB_TOOLS_OUTPUT




# Presumably this works despite Bug #23702848 	NDB : READBACKUP BEHAVIOUR WITH UNIQUE INDEXES
# due to something changed in the fix for 
# Bug #23001841 READS IN READ_COMMITTED MODE ALWAYS ABLE TO READ BACKUP REPLICA

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
--exec $NDB_MGM -e "ALL ERROR 17014" >> $NDB_TOOLS_OUTPUT

--let $idx=$KEYS
--disable_query_log
--disable_result_log
begin;
while ($idx)
{
  --eval select a.a, a.d, b.d from test.t1 a join test.t1 b on a.d = b.b where a.b = $idx
  --dec $idx
}
commit;
--enable_result_log
--enable_query_log

--exec $NDB_MGM -e "ALL ERROR 0" >> $NDB_TOOLS_OUTPUT





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
--exec $NDB_MGM -e "ALL ERROR 17014" >> $NDB_TOOLS_OUTPUT

--let $idx=$KEYS
--disable_query_log
--disable_result_log
begin;
while ($idx)
{
  --eval select a.a, a.d, b.d from test.t1 a join test.t1 b on a.d = b.a where a.b = $idx
  --dec $idx
}
commit;
--enable_result_log
--enable_query_log

--exec $NDB_MGM -e "ALL ERROR 0" >> $NDB_TOOLS_OUTPUT




# DISABLED as Unique keys do not receive 'Read Backup'(or FR) option from their
# related tables Bug #23702848 NDB : READBACKUP BEHAVIOUR WITH UNIQUE INDEXES
#
#--echo Pushed pk->uk
#--echo Lookup to get a.d, feeding uk lookup of b.b
#--replace_column 4 # 10 # 11 #
#explain select a.a, a.d, b.d
#   from 
#        test.t1 a 
#   join 
#        test.t1 b
#     on 
#        a.d = b.b
#  where a.a = 6;
#
#--echo Error insert to ensure that SPJ requests are purely local
#--exec $NDB_MGM -e "ALL ERROR 17014" >> $NDB_TOOLS_OUTPUT
#
#--let $idx=$KEYS
#--disable_query_log
#--disable_result_log
#begin;
#while ($idx)
#{
#  --eval select a.a, a.d, b.d from test.t1 a join test.t1 b on a.d = b.b where a.a = $idx
#  --dec $idx
#}
#commit;
#--enable_result_log
#--enable_query_log
#
#--exec $NDB_MGM -e "ALL ERROR 0" >> $NDB_TOOLS_OUTPUT




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
--exec $NDB_MGM -e "ALL ERROR 17014" >> $NDB_TOOLS_OUTPUT

--disable_result_log

select a.a, a.d, b.d 
   from 
        test.t1 a 
   join 
        test.t1 b
     on 
        a.d = b.a;

--enable_result_log


--exec $NDB_MGM -e "ALL ERROR 0" >> $NDB_TOOLS_OUTPUT



# DISABLED as Unique keys do not receive 'Read Backup' option from their
# related tables Bug #23702848 	NDB : READBACKUP BEHAVIOUR WITH UNIQUE INDEXES
#
#--echo Pushed scan originating sub-lookups
#--echo Table scan to get a.d, feeding uk lookups of b.b
#--replace_column 4 # 10 # 11 #
#explain select a.a, a.d, b.d 
#   from 
#        test.t1 a 
#   join 
#        test.t1 b
#     on 
#        a.d = b.b;
#
#--echo Error insert to ensure that SPJ requests are purely local
#--exec $NDB_MGM -e "ALL ERROR 17014" >> $NDB_TOOLS_OUTPUT
#
#--disable_result_log
#
#select a.a, a.d, b.d 
#   from 
#        test.t1 a 
#   join 
#        test.t1 b
#     on 
#        a.d = b.b;
#
#--enable_result_log
#
#
#--exec $NDB_MGM -e "ALL ERROR 0" >> $NDB_TOOLS_OUTPUT
#




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
--exec $NDB_MGM -e "ALL ERROR 17014" >> $NDB_TOOLS_OUTPUT

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


--exec $NDB_MGM -e "ALL ERROR 0" >> $NDB_TOOLS_OUTPUT



--echo Tests on T2 (Blobs)

--echo Put some data in.
insert into test.t2 select a,b,c,repeat('BJC',300) from test.t1;


--echo Test some basic NdbApi

--replace_column 4 # 10 # 11 #
explain select * from test.t2 where t2.a=6;

--echo Error insert to ensure that TC requests are purely local
--exec $NDB_MGM -e "ALL ERROR 8083" >> $NDB_TOOLS_OUTPUT

--let $idx=$KEYS
--disable_query_log
--disable_result_log
begin; # So all reads are from one node, defeating hinting
while ($idx)
{
  --eval select * from test.t2 where a=$idx
  --dec $idx
}
commit;
--enable_result_log
--enable_query_log

--exec $NDB_MGM -e "ALL ERROR 0" >> $NDB_TOOLS_OUTPUT


--echo UI lookups
--replace_column 4 # 10 # 11 #
explain select * from test.t2 where t2.b=6;

--echo Error insert to ensure that TC requests are purely local
--exec $NDB_MGM -e "ALL ERROR 8083" >> $NDB_TOOLS_OUTPUT

--let $idx=$KEYS
--disable_query_log
--disable_result_log
begin; # So all reads are from one node, defeating hinting
while ($idx)
{
  --eval select * from test.t2 where b=$idx
  --dec $idx
}
commit;
--enable_result_log
--enable_query_log

--exec $NDB_MGM -e "ALL ERROR 0" >> $NDB_TOOLS_OUTPUT

# Disabled due to problem reading ReadBackup 
# Blobs with OI scan
#  Bug #23703536 	NDB : READBACKUP OF BLOBS DOES NOT WORK VIA SCANS
# 
#--echo OI scan
#--replace_column 4 # 10 # 11 #
#explain select * from test.t2 where c > 6;
#
#--echo Error insert to ensure that TC requests are purely local
#--exec $NDB_MGM -e "ALL ERROR 8083" >> $NDB_TOOLS_OUTPUT
#
#--disable_query_log
#--disable_result_log
#select * from test.t2 where c > 6;
#--enable_result_log
#--enable_query_log
#
#--exec $NDB_MGM -e "ALL ERROR 0" >> $NDB_TOOLS_OUTPUT
#

# Disabled due to problem reading ReadBackup 
# Blobs with scan
#  Bug #23703536 	NDB : READBACKUP OF BLOBS DOES NOT WORK VIA SCANS
# 
#--echo Table scan
#--replace_column 4 # 10 # 11 #
#explain select * from test.t2;
#
#--echo Error insert to ensure that TC requests are purely local
#--exec $NDB_MGM -e "ALL ERROR 8083" >> $NDB_TOOLS_OUTPUT
#
#--disable_query_log
#--disable_result_log
#select * from test.t2;
#--enable_result_log
#--enable_query_log
#
#--exec $NDB_MGM -e "ALL ERROR 0" >> $NDB_TOOLS_OUTPUT
