################################################################################
# This is an auxillary file used by rpl_rename_index.test and
# gr_rename_index.test to test index rename with GR and rpl.
#
# Steps:
# 1. Create table on master with index (FULLTEXT/SPATIAL/SIMPLE INDEX).
# 2. Rename the index using ALTER TABLE..RENAME INDEX command.
# 3. Check that index is renamed sucessfully on both the servers.
# 4. Clean-up
#
# Usage:
# --let $master= server1
# --let $slave = server2
# --source common/rpl/rename_index.inc
#
# $master - master server.
# $slave - slave server.
################################################################################

--let $rename_index= 0
while ($rename_index < 3)
{

  --echo
  --echo #1. Create table on master with index
  --let $rpl_connection_name= $master
  --source include/connection.inc
  if ($rename_index == 0)
  {
    CREATE TABLE t1 (a int PRIMARY KEY, b char(10), FULLTEXT KEY (b));
    INSERT INTO t1 VALUES (1,'abc'), (2, 'def'), (3,'ghi');
    --let $update_query= UPDATE t1 SET b='mysql' WHERE b='abc'
  }

  if ($rename_index == 1)
  {
    CREATE TABLE t1 (a int PRIMARY KEY, b GEOMETRY NOT NULL SRID 0);
    CREATE SPATIAL INDEX b ON t1 (b);
    INSERT INTO t1 VALUES (1, ST_GEOMFROMText('POINT(1 1)')),
                         (2, ST_GEOMFROMTEXT('POLYGON((1 2,5 4,9 9,1 9,1 2))')),
                         (3, ST_GEOMFROMTEXT('LINESTRING(0 0,10 10)'));
    --let $update_query= UPDATE t1 SET b=ST_GEOMFROMText('POINT(5 5)') WHERE b=ST_GEOMFROMTEXT('LINESTRING(0 0,10 10)')
  }

  if ($rename_index == 2)
  {
    CREATE TABLE t1 ( a INT PRIMARY KEY, b int,  KEY (b));
    INSERT INTO t1 VALUES (1,10),(2,20),(3,30);
    --let $update_query= UPDATE t1 SET b=11 WHERE b=10
  }

  # Check that t1 has a index named b on both the servers.
  --let $assert_text= There should be a index named b on table t1.
  --let $assert_cond= [SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = "test" AND TABLE_NAME = "t1" AND INDEX_NAME= "b"]=1;
  --source include/assert.inc

  --source include/rpl/sync.inc

  --let $rpl_connection_name= $slave
  --source include/connection.inc

  --let $assert_text= There should be a index named b on table t1.
  --let $assert_cond= [SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = "test" AND TABLE_NAME = "t1" AND INDEX_NAME= "b"]=1;
  --source include/assert.inc

  --echo
  --echo #2. Rename the index using ALTER TABLE..RENAME INDEX command.
  --let $rpl_connection_name= $master
  --source include/connection.inc
  ALTER TABLE t1 RENAME index b to idx;

  --echo
  --echo #3. Check that index is renamed sucessfully on both the servers.
  --let $assert_text= There should not be a index named b on table t1.
  --let $assert_cond= [SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = "test" AND TABLE_NAME = "t1" AND INDEX_NAME= "b"]=0;
  --source include/assert.inc

  --let $assert_text= There should be a index named idx on table t1.
  --let $assert_cond= [SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = "test" AND TABLE_NAME = "t1" AND INDEX_NAME= "idx" ]=1;
  --source include/assert.inc

  --source include/rpl/sync.inc

  --let $rpl_connection_name= $slave
  --source include/connection.inc

  --let $assert_text= There should not be a index named b on table t1.
  --let $assert_cond= [SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = "test" AND TABLE_NAME = "t1" AND INDEX_NAME= "b" ]=0;
  --source include/assert.inc

  --let $assert_text= There should be a index named idx on table t1.
  --let $assert_cond= [SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS WHERE TABLE_SCHEMA = "test" AND TABLE_NAME = "t1" AND INDEX_NAME= "idx" ]=1;
  --source include/assert.inc

  # Perform another dml operation on t1.
  --let $rpl_connection_name= $master
  --source include/connection.inc
  --eval $update_query

  # Ensure that data is consistent on both the servers
  --source include/rpl/sync.inc
  --let $diff_tables=$master:t1, $slave:t1
  --source include/diff_tables.inc

  --echo
  --echo #4. Clean-up
  DROP TABLE t1;

  --source include/rpl/sync.inc

  --inc $rename_index
}
