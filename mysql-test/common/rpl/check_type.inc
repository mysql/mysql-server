# Helper file to perform one insert of a value into a table with
# different types on master and slave.  The file will insert the
# result into the type_conversions table *on the slave* to get a
# summary of failing and succeeding tests.

# Input:
#    $source_type      Type on the master
#    $target_type      Type on the slave
#    $source_value     Value on the master (inserted into the table)
#    $target_value     Value on the slave (expected value in the table
#                      on the slave)
#    $can_convert      True if conversion shall work, false if it
#                      shall generate an error 
#    $engine_type      The storage engine to be used for storing table
#                      on both master and slave

if (!$engine_type)
{
  # Use the default storage engine
  let $engine_type=`SELECT @@default_storage_engine`;
}

if ($rpl_debug)
{
  --echo source_type=$source_type target_type=$target_type source_value=$source_value target_value=$target_value can_convert=$can_convert engine_type=$engine_type
  SELECT @@GLOBAL.REPLICA_TYPE_CONVERSIONS;
}

--source include/rpl/connection_source.inc
disable_warnings;
DROP TABLE IF EXISTS t1;
enable_warnings;
eval CREATE TABLE t1(
  pk INT NOT NULL PRIMARY KEY,
  a $source_type
) ENGINE=$engine_type CHARACTER SET latin1;
--source include/rpl/sync_to_replica.inc
eval ALTER TABLE t1 MODIFY a $target_type;

--source include/rpl/connection_source.inc
eval INSERT INTO t1 VALUES(1, $source_value);
if ($can_convert) {
  --source include/rpl/sync_to_replica.inc
  eval SELECT a = $target_value into @compare FROM t1;
  eval INSERT INTO type_conversions SET
       Source = "$source_type",
       Target = "$target_type",
       Flags = @@replica_type_conversions,
       On_Master = $source_value,
       Expected = $target_value,
       Compare = @compare;
  UPDATE type_conversions
     SET On_Slave = (SELECT a FROM t1)
   WHERE TestNo = LAST_INSERT_ID();
}
if (!$can_convert) {
  --source include/rpl/connection_replica.inc
  wait_for_slave_to_stop;
  let $error = query_get_value("SHOW REPLICA STATUS", Last_SQL_Error, 1);
  eval INSERT INTO type_conversions SET
       Source = "$source_type",
       Target = "$target_type",
       Flags = @@replica_type_conversions,
       On_Master = $source_value,
       Error = "$error";
  SET GLOBAL SQL_REPLICA_SKIP_COUNTER = 1;
  START REPLICA;
}
