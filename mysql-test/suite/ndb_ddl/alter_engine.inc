--source include/have_debug.inc
#
# Run variations of ALTER TABLE .. ENGINE based on the
# $engine passed by the calling test
#

if (!$engine)
{
  die Need the $engine variable to run the tests;
}

# Alter tables to $engine, they should then disappear from
# all but the server where they were altered
let $counter = 1;
while ($counter <= $num_tables)
{
  # Default table name is t$counter, ie. t1, t2, etc
  let $tx=t$counter;

  eval ALTER TABLE $tx
          ENGINE=$engine;

  inc $counter;
}

# Verify that t1 on first server contains the table in $engine
# while it does not exist on the others(checking only two other servers)

--connection mysqld1
select TABLE_SCHEMA, TABLE_NAME, ENGINE
  from information_schema.tables
  where TABLE_SCHEMA = 'ndb_ddl_test'
  order by TABLE_SCHEMA, TABLE_NAME;

--connection mysqld2
select TABLE_SCHEMA, TABLE_NAME, ENGINE
  from information_schema.tables
  where TABLE_SCHEMA = 'ndb_ddl_test'
  order by TABLE_SCHEMA, TABLE_NAME;

--connection mysqld3
select TABLE_SCHEMA, TABLE_NAME, ENGINE
  from information_schema.tables
  where TABLE_SCHEMA = 'ndb_ddl_test'
  order by TABLE_SCHEMA, TABLE_NAME;

--connection mysqld1

# Alter tables back to NDB and verify that DD on all
# servers are identical
let $counter = 1;
while ($counter <= $num_tables)
{
  # Default table name is t$counter, ie. t1, t2, etc
  let $tx=t$counter;

  eval ALTER TABLE $tx
          ENGINE=NDB;

  inc $counter;
}

# All servers should be identical again
--source verify_mysql_dd.inc

# Simulate a failure during the ALTER to the engine from ndbcluster
# and check if rollback restores the original version properly

# Simulate failure during rename table
set debug='+d,ndb_simulate_alter_failure_rename1';
--error ER_INTERNAL_ERROR
eval ALTER TABLE ndb_ddl_test.t1 ENGINE = $engine;

# All servers should be identical
--source verify_mysql_dd.inc

# Simulate a failure during the ALTER to ndbcluster from the engine
# and check if rollback restores the original version properly

# First ALTER t1 to the target $engine
eval ALTER TABLE ndb_ddl_test.t1 ENGINE = $engine;

# Simulate failure during rename table
set debug='+d,ndb_simulate_alter_failure_rename2';
--error ER_INTERNAL_ERROR
eval ALTER TABLE ndb_ddl_test.t1 ENGINE = NDB;

# Verify that there are no tables with temp name
# leftover in NDB due to failure and rollback
--source count_temp_tables.inc

# Retry now and all should be fine.
eval ALTER TABLE ndb_ddl_test.t1 ENGINE = NDB;
--source verify_mysql_dd.inc
