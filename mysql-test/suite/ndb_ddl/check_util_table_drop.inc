#
# Check that functionality works before and after
# dropping the given util table
#
# Arguments:
#  util_table_name - name of util table
#

if (!$util_table_name)
{
  die Need the name of the util table to drop;
}

--connection mysqld1
CREATE TABLE t1 (
  a int primary key
) ENGINE = NDB;

--connection mysqld3
SHOW CREATE TABLE t1;

--connection mysqld5
# Clear the binlog to prepare for checking if a LOST_EVENTS appears later
RESET MASTER;

--echo # Drop mysql.$util_table_name from NDB
--exec $NDB_DROP_TABLE -d mysql $util_table_name

--connection mysqld1
--echo # mysqld1: Wait until ready again...
--source include/ndb_not_readonly.inc

--connection mysqld3
--echo # mysqld3: Wait until ready again...
--source include/ndb_not_readonly.inc

--connection mysqld5
--echo # mysqld5: Wait until ready again...
--source include/ndb_not_readonly.inc

--connection mysqld5
# NOTE! Neither drop of ndb_schema or ndb_schema_result should have a LOST_EVENT
--echo # Check if a LOST_EVENTS event was written to the binlog
--source include/show_binlog_events.inc

--connection mysqld1
ALTER TABLE t1 ADD COLUMN b int not null;

--connection mysqld3
SHOW CREATE TABLE t1;

DROP TABLE t1;

# Reset args
let $util_table_name= ;
