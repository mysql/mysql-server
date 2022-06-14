--source include/have_debug_sync.inc

# Allow system table access.
SET GLOBAL debug= '+d,skip_dd_table_access_check';

# Save the initial number of concurrent sessions.
--source include/count_sessions.inc
--enable_connect_log

#
# Create default thread and do setup
#
use test;

CREATE TABLE t1 (f1 int) COMMENT='abc';

# Create a non system view
CREATE VIEW not_system_view AS
  SELECT name as table_name, comment FROM mysql.tables;

SHOW CREATE VIEW not_system_view;

##
## Scenario 1: I_S query and 'SERIALIZABLE' isolation level.
##

# Start a transaction to select from view.
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
START TRANSACTION;

# Start thread1 which updates 'mysql.tables' DD table.
connect(con1,localhost,root,,);
START TRANSACTION;
UPDATE mysql.tables SET comment='mno' where name='t1';

# In 'default thread' execute SELECT on views.
connection default;

# Test that SELECT on a system view does not hang.
SELECT table_name, table_comment
  FROM INFORMATION_SCHEMA.TABLES
  WHERE table_name='t1';

--disable_testcase Bug#31896710
# Test that SELECT on non system view hangs.
--send SELECT table_name, comment FROM not_system_view WHERE table_name='t1';
--enable_testcase

connection con1;
--disable_testcase Bug#31896710
let $wait_condition= SELECT COUNT(*)>=1 FROM
  performance_schema.data_locks
  WHERE OBJECT_SCHEMA='mysql' AND
        OBJECT_NAME='tables' and LOCK_STATUS='WAITING';
--source include/wait_condition.inc 
--enable_testcase

rollback;

connection default;
--disable_testcase Bug#31896710
--reap
--enable_testcase
commit;


##
## Scenario 2: I_S query and 'REPEATABLE READ' isolation level.
##
connection default;

# Start a transaction to select from view.
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;
START TRANSACTION;

# Start thread1 which updates 'mysql.tables' DD table.
connection con1;
START TRANSACTION;
UPDATE mysql.tables SET comment='mno' where name='t1';

# In 'default thread' execute SELECT on views.
connection default;

# Test that SELECT on a system view and non system view does not hang.
SELECT table_name, table_comment
  FROM INFORMATION_SCHEMA.TABLES
  WHERE table_name='t1';
SELECT table_name, comment
  FROM not_system_view
  WHERE table_name='t1';

commit;
connection con1;
rollback;

##
## Scenario 3: I_S query and 'READ COMMITTED' isolation level.
##
connection default;

# Start a transaction to select from view.
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;
START TRANSACTION;

# Start thread1 which updates 'mysql.tables' DD table.
connection con1;
START TRANSACTION;
UPDATE mysql.tables SET comment='mno' where name='t1';

# In 'default thread' execute SELECT on views.
connection default;

# Test that SELECT on a system view and non system view does not hang.
SELECT table_name, table_comment
  FROM INFORMATION_SCHEMA.TABLES
  WHERE table_name='t1';
SELECT table_name, comment
  FROM not_system_view
  WHERE table_name='t1';

commit;
connection con1;
rollback;

##
## Scenario 4: I_S query and 'READ UNCOMMITTED' isolation level.
##
connection default;

# Start a transaction to select from view.
SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;
START TRANSACTION;

# Start thread1 which updates 'mysql.tables' DD table.
connection con1;
START TRANSACTION;
UPDATE mysql.tables SET comment='mno' where name='t1';

# In 'default thread' execute SELECT on views.
connection default;

# Test that SELECT on a system view and non system view does not hang.
SELECT table_name, table_comment
  FROM INFORMATION_SCHEMA.TABLES
  WHERE table_name='t1';
SELECT table_name, comment
  FROM not_system_view
  WHERE table_name='t1';

commit;
connection con1;
rollback;

##
## Scenario 5: I_S query with 'FOR UPDATE' and 'LOCK IN SHARE MODE'
##             is not allowed.
## Case 1: When UPDATE in progress.
##
connection default;

# Start a transaction to select from view.
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
START TRANSACTION;

# Start thread1 which updates 'mysql.tables' DD table.
connection con1;
START TRANSACTION;
UPDATE mysql.tables SET comment='mno' where name='t1';

# In 'default thread' execute SELECT on views while UPDATE in progress
connection default;

# Test that SELECT on a system view with LOCK IN SHARE MODE fails.
--error ER_IS_QUERY_INVALID_CLAUSE
SELECT table_name, table_comment
  FROM INFORMATION_SCHEMA.TABLES
  WHERE table_name='t1'
  LOCK IN SHARE MODE;

# Test that SELECT on a system view with FOR UPDATE fails.
--error ER_TABLEACCESS_DENIED_ERROR
SELECT table_name, table_comment
  FROM INFORMATION_SCHEMA.TABLES
  WHERE table_name='t1'
  FOR UPDATE;

# Rollback UPDATE operation
connection con1;
rollback;

#
# Scenario 5:
# Case 2: Try SELECT's again without UPDATE in progress.
#
connection default;

# Test that SELECT on a system view with LOCK IN SHARE MODE fails.
--error ER_IS_QUERY_INVALID_CLAUSE
SELECT table_name, table_comment
  FROM INFORMATION_SCHEMA.TABLES
  WHERE table_name='t1'
  LOCK IN SHARE MODE;

# Test that SELECT on a system view with FOR UPDATE fails.
--error ER_TABLEACCESS_DENIED_ERROR
SELECT table_name, table_comment
  FROM INFORMATION_SCHEMA.TABLES
  WHERE table_name='t1'
  FOR UPDATE;

# Test that SELECT on a non system view with 'LOCK IN SHARE MODE' succeeds.
SELECT table_name, comment
  FROM not_system_view
  WHERE table_name='t1'
  LOCK IN SHARE MODE;

# Test that SELECT on a non system view with 'FOR UPDATE' succeeds
SELECT table_name, comment
  FROM not_system_view
  WHERE table_name='t1'
  FOR UPDATE;

commit;

#
# Clean-up
#

connection con1;
disconnect con1;
connection default;
DROP VIEW not_system_view;
DROP TABLE t1;



# Check that all connections opened by test cases in this file are really
# gone so execution of other tests won't be affected by their presence.
--disable_connect_log
--source include/wait_until_count_sessions.inc

# Reset system table access.
SET GLOBAL debug= '-d,skip_dd_table_access_check';
