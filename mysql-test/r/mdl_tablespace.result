###########################################################
#
# WL#7957: MDL FOR TABLESPACES
# ============================
#
# We have the following tablespace DDL operations that acquire
# exclusive MDL locks on tablespaces:
#
#   CREATE, ALTER and DROP TABLESPACE
#
# Then, we have the following non-tablespace related DDL
# operations that acquire intention exclusive locks on
# tablespaces:
#
#   CREATE, ALTER, RENAME, TRUNCATE, LOCK, DROP TABLE
#
# and
#
#   CREATE, DROP INDEX
#
# Below are test cases to verify that:
#
# 1. We can *not* run DDL statements manipulating the same
#    tablespace concurrently.
# 2. We can run different DDL statements manipulating different
#    tablespaces concurrently.
# 3. We can run different DDL statements accessing different
#    tables in the same  tablespace concurrently.
# 4. We can *not* run DDL statements on tables in a given
#    tablespace concurrently with DDL operations on the
#    same tablespace.
# 5. Verify that dropping a schema with tables in various
#    tablespaces cannot run concurrentlty with dropping the
#    tablespaces.
# 6. Verify that tablespace IX locks are not acquired in
#    the context of 'FLUSH TABLE WITH READ LOCK',
#    'FLUSH TABLE FOR EXPORT' or 'LOCK TABLE READ'.
# 7. Verify that statements 'FLUSH TABLE WITH READ LOCK',
#    'FLUSH TABLE FOR EXPORT' and 'LOCK TABLE READ'
#    can run concurrently with DDL operations on the
#    tablespace.
# 8. Verify that tablespace IX locks are acquired in
#    the context of 'LOCK TABLE WRITE'.
# 9. Verify that table DML (e.g. SELECT ... FOR UPDATE)
#    can run concurrently with DDL on the tablespace.
# 10. Verify that MDL with prepared statements works as
#     expected, using a prepared ALTER TABLE.
# 11. Verify that a non-empty tablespace cannot be dropped.
# 12. Verify that the tablespace name length is checked
#     properly.
# 13. Extend test coverage:
#       i) Run tablespace DDL within lock tables mode.
#      ii) Run tablespace DDL when the global read lock
#          is taken.
#     iii) Provoke timeout for table DDL waiting for
#          tablespace MDL IX lock.
#      iv) Provoke timeout for tablespace DDL waiting for
#          tablespace MDL X lock.
# 14. Verify that CREATE TABLE ... LIKE statements are
#     handled correctly, both with and without source
#     tables in general tablespaces.
# 15. Verify that explicit invalid tablespace names make
#     table related DDL be rejected.
# 16. Verify correct P_S information.
###########################################################
#
# Test setup: Create reusable connections:
#
connect  con1, localhost, root;
connect  con2, localhost, root;
connect  con3, localhost, root;
connect  con4, localhost, root;
connect  con5, localhost, root;
###########################################################
# TC1. Verify that we can *not* run DDL statements manipulating
#      the same tablespace concurrently.
#
#
# Start executing CREATE TABLESPACE, but wait after
# getting the MDL X lock on the tablespace name.
connection con1;
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name
  SIGNAL got_lock_con1
  WAIT_FOR cont_con1';
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1_1.ibd';
#
# Start executing ALTER TABLESPACE.
connection con2;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con1';
ALTER TABLESPACE ts1 ADD DATAFILE 'ts1_2.ibd';
#
# Verify that the ALTER TABLESPACE is waiting for an MDL
# X lock on the tablespace name (not logged). Then, signal
# con1 to make the CREATE TABLESPACE proceed.
connection default;
SET DEBUG_SYNC= 'now SIGNAL cont_con1';
#
# Reap the connections, reset DEBUG_SYNC and drop objects.
connection con1;
connection con2;
ERROR HY000: Failed to alter: TABLESPACE ts1
connection default;
DROP TABLESPACE ts1;
SET DEBUG_SYNC= 'RESET';
###########################################################
# TC2. Verify that we can run different DDL statements
#      manipulating different tablespaces concurrently.
#
#
# First, create a tablespace.
connection default;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1_1.ibd';
#
# Start executing CREATE TABLESPACE, but wait after
# getting the MDL X lock on the tablespace name.
connection con1;
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name
  SIGNAL got_lock_con1
  WAIT_FOR cont_con1';
CREATE TABLESPACE ts2 ADD DATAFILE 'ts2.ibd';
#
# Wait for con1 to get its X lock, then start executing
# ALTER TABLESPACE, but wait after getting the MDL X
# lock on the tablespace name.
connection con2;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con1';
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name
  SIGNAL got_lock_con2
  WAIT_FOR cont_con2';
ALTER TABLESPACE ts1 ADD DATAFILE 'ts1_2.ibd';
#
# Verify that both con1 and con2 have X locks. This will be the
# case when con2 signals got_lock_con2.
connection default;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con2';
#
# Then, signal both con1 to and con2 to make the execution
# proceed.
SET DEBUG_SYNC= 'now SIGNAL cont_con1';
SET DEBUG_SYNC= 'now SIGNAL cont_con2';
#
# Reap the connections, reset DEBUG_SYNC and drop objects.
connection con1;
connection con2;
ERROR HY000: Failed to alter: TABLESPACE ts1
connection default;
DROP TABLESPACE ts1;
DROP TABLESPACE ts2;
SET DEBUG_SYNC= 'RESET';
###########################################################
# TC3. Verify that we can run different DDL statements
#      accessing different tables in the same tablespace
#      concurrently.
# +
#
# TC4. Verify that we can *not* run DDL statements on tables
#      in a given tablespace concurrently with DDL operations
#      on the same tablespace.
#
#
# First, create tablespaces ts1 and ts2, and tables in the
# tablespaces.
connection default;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1_1.ibd';
CREATE TABLESPACE ts2 ADD DATAFILE 'ts2_1.ibd';
CREATE TABLE t1 (pk INTEGER PRIMARY KEY) TABLESPACE ts1;
CREATE TABLE t2 (pk INTEGER PRIMARY KEY) TABLESPACE ts1;
CREATE TABLE t3 (pk INTEGER PRIMARY KEY) TABLESPACE ts2;
#
# On connection con1, truncate the table in ts2, but wait after
# getting IX lock on the tablespace.
connection con1;
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table
  SIGNAL got_lock_con1
  WAIT_FOR cont_con1';
TRUNCATE TABLE t3;
#
# On connection con2, alter table t1 to use ts2 instead of ts1,
# but wait after getting IX lock on the tablespace.
connection con2;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con1';
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table
  SIGNAL got_lock_con2
  WAIT_FOR cont_con2';
ALTER TABLE t1 TABLESPACE ts2;
#
# On connection con3, drop table t2, but wait after getting
# IX lock on the tablespace.
connection con3;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con2';
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table
  SIGNAL got_lock_con3
  WAIT_FOR cont_con3';
DROP TABLE t2;
#
# On connection con4, do DROP TABLESPACE ts1, which will
# wait for X lock on the tablepsace name.
connection con4;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con3';
SET DEBUG_SYNC= 'now SIGNAL waiting_con4';
DROP TABLESPACE ts1;
#
# On connection con5, do ALTER TABLESPACE ts2, which will
# wait for X lock on the tablespace name.
connection con5;
SET DEBUG_SYNC= 'now WAIT_FOR waiting_con4';
ALTER TABLESPACE ts2 ADD DATAFILE 'ts2_2.ibd';
#
# Back on the default connection, we can start unwinding the
# situation. First, verify that we have two threads waiting for
# MDL locks on tablespaces.
connection default;
#
# Next, we signal con2 and con3 to continue. This will make
# all MDL IX locks on s1 be released, which again will make
# con4 finish. Thus, we verify that we have only one thread
# left waiting for MDL lcoks on tablespaces.
SET DEBUG_SYNC= 'now SIGNAL cont_con2';
SET DEBUG_SYNC= 'now SIGNAL cont_con3';
#
# Finally, signal con1 to finish, which will make all MDL
# locks on s2 be released. This, in its turn, will make
# con5 be able to continue and finish. We verify this by
# checking that we have no threads left waiting for MDL
# locks on tablespaces.
SET DEBUG_SYNC= 'now SIGNAL cont_con1';
#
# Reap the connections, reset DEBUG_SYNC and drop objects.
connection con1;
connection con2;
connection con3;
connection con4;
connection con5;
ERROR HY000: Failed to alter: TABLESPACE ts2
connection default;
DROP TABLE t1;
DROP TABLE t3;
DROP TABLESPACE ts2;
SET DEBUG_SYNC= 'RESET';
###########################################################
# TC5. Verify that dropping a schema with tables in various
#      tablespaces cannot run concurrentlty with dropping the
#      tablespaces.
#
#
# First create two tablespaces and a schema with two tables
# in it, but put the tables in different tablespaces.
connection default;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd';
CREATE TABLESPACE ts2 ADD DATAFILE 'ts2.ibd';
CREATE SCHEMA s1;
CREATE TABLE s1.t1 (pk INTEGER PRIMARY KEY) TABLESPACE ts1;
CREATE TABLE s1.t2 (pk INTEGER PRIMARY KEY) TABLESPACE ts2;
#
# Next, on connection con1, issue a DROP SCHEMA but halt it
# after the tablespace locks have been acquired.
connection con1;
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table
  SIGNAL got_lock_con1
  WAIT_FOR cont_con1';
DROP SCHEMA s1;
#
# Then, on connection con2 and con3, issue DROP TABLESPACE
# statements to drop the tablespaces of the two tables in
# the schema being dropped.
connection con2;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con1';
SET DEBUG_SYNC= 'now SIGNAL go_con3';
DROP TABLESPACE ts1;
connection con3;
SET DEBUG_SYNC= 'now WAIT_FOR go_con3';
DROP TABLESPACE ts2;
#
# Verify that the two DROP TABLESPACE statements are stuck
# waiting for MDL locks.
connection default;
#
# Signal connection con1 to make it continue dropping the
# schema.
SET DEBUG_SYNC= 'now SIGNAL cont_con1';
#
# Verify that the DROP TABLESPACE statements eventually get
# the MDL locks.
#
# Reap the connections, reset DEBUG_SYNC and drop objects.
connection con1;
connection con2;
connection con3;
connection default;
SET DEBUG_SYNC= 'RESET';
###########################################################
# TC6. Verify that tablespace IX locks are not acquired in
#      the context of 'FLUSH TABLE WITH READ LOCK',
#      'FLUSH TABLE FOR EXPORT' or 'LOCK TABLE READ'.
#
# First, create tablespaces ts1, ts2 and ts3, and tables in the
# tablespaces.
connection default;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1_1.ibd';
CREATE TABLESPACE ts2 ADD DATAFILE 'ts2_1.ibd';
CREATE TABLESPACE ts3 ADD DATAFILE 'ts3_1.ibd';
CREATE TABLE t1 (pk INTEGER PRIMARY KEY) TABLESPACE ts1;
CREATE TABLE t2 (pk INTEGER PRIMARY KEY) TABLESPACE ts2;
CREATE TABLE t3 (pk INTEGER PRIMARY KEY) TABLESPACE ts3;
#
# Set a trap in the sync point after acquiring tablespace IX
# locks, and execute FLUSH TABLE WITH READ LOCK.
connection con1;
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table HIT_LIMIT 1';
FLUSH TABLE t1 WITH READ LOCK;
#
# Set a trap in the sync point after acquiring tablespace IX
# locks, and execute FLUSH TABLE FOR EXPORT.
connection con2;
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table HIT_LIMIT 1';
FLUSH TABLE t2 FOR EXPORT;
#
# Set a trap in the sync point after acquiring tablespace IX
# locks, and execute LOCK TABLE READ.
connection con3;
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table HIT_LIMIT 1';
LOCK TABLE t3 READ;
#
# Reap the connections, reset DEBUG_SYNC and drop objects.
connection con1;
Warnings:
Warning	1235	InnoDB: This version of MySQL doesn't yet support 'FLUSH TABLES FOR EXPORT on table `test`.`t1` in a general tablespace.'
UNLOCK TABLES;
connection con2;
Warnings:
Warning	1235	InnoDB: This version of MySQL doesn't yet support 'FLUSH TABLES FOR EXPORT on table `test`.`t2` in a general tablespace.'
UNLOCK TABLES;
connection con3;
UNLOCK TABLES;
connection default;
DROP TABLE t1;
DROP TABLESPACE ts1;
DROP TABLE t2;
DROP TABLESPACE ts2;
DROP TABLE t3;
DROP TABLESPACE ts3;
SET DEBUG_SYNC= 'RESET';
###########################################################
# TC7. Verify that statements 'FLUSH TABLE WITH READ LOCK',
#      'FLUSH TABLE FOR EXPORT' and 'LOCK TABLE READ'
#      can run concurrently with operations on the
#      tablespace.
#
# First, create tablespaces ts1, ts2 and ts3, and tables in the
# tablespaces.
connection default;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1_1.ibd';
CREATE TABLESPACE ts2 ADD DATAFILE 'ts2_1.ibd';
CREATE TABLESPACE ts3 ADD DATAFILE 'ts3_1.ibd';
CREATE TABLE t1 (pk INTEGER PRIMARY KEY) TABLESPACE ts1;
CREATE TABLE t2 (pk INTEGER PRIMARY KEY) TABLESPACE ts2;
CREATE TABLE t3 (pk INTEGER PRIMARY KEY) TABLESPACE ts3;
#
# Do a tablespace operation on ts1, and park it after getting
# the tablespace X lock.
connection con1;
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name
  SIGNAL got_lock_con1
  WAIT_FOR cont_con1';
ALTER TABLESPACE ts1 ADD DATAFILE 'ts1_2.ibd';
#
# Do a tablespace operation on ts2, and park it after getting
# the tablespace X lock.
connection con2;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con1';
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name
  SIGNAL got_lock_con2
  WAIT_FOR cont_con2';
ALTER TABLESPACE ts2 ADD DATAFILE 'ts2_2.ibd';
#
# Do a tablespace operation on ts3, and park it after getting
# the tablespace X lock.
connection con3;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con2';
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name
  SIGNAL got_lock_con3
  WAIT_FOR cont_con3';
ALTER TABLESPACE ts3 ADD DATAFILE 'ts3_2.ibd';
#
# Execute statements that should not be blocked
# even with parked tablespace DDL statements
# holding tablespace name X locks.
connection default;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con3';
FLUSH TABLE t1 WITH READ LOCK;
Warnings:
Warning	1235	InnoDB: This version of MySQL doesn't yet support 'FLUSH TABLES FOR EXPORT on table `test`.`t1` in a general tablespace.'
UNLOCK TABLES;
FLUSH TABLE t2 FOR EXPORT;
Warnings:
Warning	1235	InnoDB: This version of MySQL doesn't yet support 'FLUSH TABLES FOR EXPORT on table `test`.`t2` in a general tablespace.'
UNLOCK TABLES;
LOCK TABLE t3 READ;
UNLOCK TABLES;
SET DEBUG_SYNC= 'now SIGNAL cont_con1';
SET DEBUG_SYNC= 'now SIGNAL cont_con2';
SET DEBUG_SYNC= 'now SIGNAL cont_con3';
#
# Reap the connections, reset DEBUG_SYNC and drop objects.
connection con1;
ERROR HY000: Failed to alter: TABLESPACE ts1
connection con2;
ERROR HY000: Failed to alter: TABLESPACE ts2
connection con3;
ERROR HY000: Failed to alter: TABLESPACE ts3
connection default;
DROP TABLE t1;
DROP TABLESPACE ts1;
DROP TABLE t2;
DROP TABLESPACE ts2;
DROP TABLE t3;
DROP TABLESPACE ts3;
SET DEBUG_SYNC= 'RESET';
###########################################################
# TC8. Verify that tablespace IX locks are acquired in
#      the context of 'LOCK TABLE WRITE'.
#
# First, create tablespace ts1 with one table.
connection default;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1_1.ibd';
CREATE TABLE t1 (pk INTEGER PRIMARY KEY) TABLESPACE ts1;
#
# Set a trap in the sync point after acquiring tablespace IX
# locks, and execute LOCK TABLE WRITE.
connection con1;
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table
  SIGNAL got_lock_con1
  WAIT_FOR cont_con1';
LOCK TABLE t1 WRITE;
#
# Wait for con1 to get lock, then signal it to continue.
connection default;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con1';
SET DEBUG_SYNC= 'now SIGNAL cont_con1';
#
# Reap the connections, reset DEBUG_SYNC and drop objects.
connection con1;
UNLOCK TABLES;
connection default;
DROP TABLE t1;
DROP TABLESPACE ts1;
SET DEBUG_SYNC= 'RESET';
###########################################################
# TC9. Verify that table DML (e.g. SELECT ... FOR UPDATE)
#      can run concurrently with DDL on the tablespace.
#
# First, create tablespace ts1 with one table and one row.
connection default;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1_1.ibd';
CREATE TABLE t1 (pk INTEGER PRIMARY KEY) TABLESPACE ts1;
INSERT INTO t1 (pk) VALUES (1);
#
# Set a trap in the sync point after acquiring tablespace X
# lock, and execute ALTER TABLESPACE.
connection con1;
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name
  SIGNAL got_lock_con1
  WAIT_FOR cont_con1';
ALTER TABLESPACE ts1 ADD DATAFILE 'ts1_2.ibd';
#
# Wait for con1 to get lock, then do DML.
connection con2;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con1';
SELECT * FROM t1 FOR UPDATE;
pk
1
SET DEBUG_SYNC= 'now SIGNAL done_con2';
#
# Wait for con2 to finish, then signal con1 to continue.
connection default;
SET DEBUG_SYNC= 'now WAIT_FOR done_con2';
SET DEBUG_SYNC= 'now SIGNAL cont_con1';
#
# Reap the connections, reset DEBUG_SYNC and drop objects.
connection con1;
ERROR HY000: Failed to alter: TABLESPACE ts1
connection default;
DROP TABLE t1;
DROP TABLESPACE ts1;
SET DEBUG_SYNC= 'RESET';
###########################################################
# TC10. Verify that MDL with prepared statements works as
#       expected, using a prepared ALTER TABLE.
#
# First, create tablespace ts1 with one table.
connection default;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1_1.ibd';
CREATE TABLE t1 (pk INTEGER PRIMARY KEY) TABLESPACE ts1;
#
# Set a trap in the sync point after acquiring tablespace IX
# lock, and prepare a statement. Set a new sync point after
# preparing, now expecting the sync point to be reached, and
# execute the prepared statement.
connection con1;
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table HIT_LIMIT 1';
PREPARE stmt1 FROM 'ALTER TABLE t1 ADD COLUMN (i INTEGER)';
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table
  SIGNAL got_lock_con1
  WAIT_FOR cont_con1';
EXECUTE stmt1;
#
# Wait for con1 to get the lock while executing, then signal
# it to continue.
connection default;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con1';
SET DEBUG_SYNC= 'now SIGNAL cont_con1';
#
# Reap the connections, reset DEBUG_SYNC and drop objects.
connection con1;
DEALLOCATE PREPARE stmt1;
connection default;
DROP TABLE t1;
DROP TABLESPACE ts1;
SET DEBUG_SYNC= 'RESET';
###########################################################
# TC11. Verify that a non-empty tablespace cannot be dropped.
#
#
# First, create a tablespace ts1 with one table.
connection default;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1_1.ibd';
CREATE TABLE t1 (pk INTEGER PRIMARY KEY) TABLESPACE ts1;
#
# Dropping ts1 should fail.
DROP TABLESPACE ts1;
ERROR HY000: Tablespace `ts1` is not empty.
#
# Initiate a drop of table t1 and park it after
# acquiring IX lock on ts1.
connection con1;
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table
  SIGNAL got_lock_con1
  WAIT_FOR cont_con1';
DROP TABLE t1;
#
# Initiate a drop of tablespace ts1 as soon as con1
# got its lock.
connection con2;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con1 NO_CLEAR_EVENT';
DROP TABLESPACE ts1;
#
# Verify that the tablespace drop is waiting
# for X lock on ts1.
connection default;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con1 NO_CLEAR_EVENT';
#
# Let the table drop continue, and verify that the
# tablespace drop is not waiting anymore.
SET DEBUG_SYNC= 'now SIGNAL cont_con1';
#
# Reap the connections, reset DEBUG_SYNC and drop objects.
connection con1;
connection con2;
connection default;
DROP TABLESPACE ts1;
ERROR HY000: Tablespace ts1 doesn't exist.
SET DEBUG_SYNC= 'RESET';
###########################################################
# TC12. Verify that the tablespace name length is checked
#       properly.
#
#
# Set UTF8 character set to allow multi byte characters.
SET CHARACTER SET UTF8;
#
# An empty tablespace name should be rejected.
CREATE TABLESPACE `` ADD DATAFILE 'x.ibd';
ERROR 42000: Incorrect tablespace name ``
#
# A tablespace name up to 64 1-byte characters should be accepted.
CREATE TABLESPACE xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx ADD DATAFILE 'x.ibd';
DROP TABLESPACE xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx;
#
# A tablespace name of 65 1-byte characters should be rejected.
CREATE TABLESPACE xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx ADD DATAFILE 'x.ibd';
ERROR 42000: Identifier name 'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx' is too long
#
# A tablespace name up to 64 2-byte characters should be accepted.
CREATE TABLESPACE `¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥` ADD DATAFILE 'x.ibd';
DROP TABLESPACE `¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥`;
#
# A tablespace name of 65 2-byte characters should be rejected.
CREATE TABLESPACE `¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥` ADD DATAFILE 'x.ibd';
ERROR 42000: Identifier name '¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥¥' is too long
#
# A tablespace name up to 64 3-byte characters should be accepted.
CREATE TABLESPACE `€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€` ADD DATAFILE 'x.ibd';
DROP TABLESPACE `€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€`;
#
# A tablespace name of 65 3-byte characters should be rejected.
CREATE TABLESPACE `€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€` ADD DATAFILE 'x.ibd';
ERROR 42000: Identifier name '€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€?' is too long
#
# A tablespace name containing supplementary UTF characters should be rejected.
CREATE TABLESPACE `a𐍈` ADD DATAFILE 'x.ibd';
ERROR HY000: Invalid utf8mb3 character string: 'a\xF0\x90\x8D\x88'
###########################################################
# TC13. Extend test coverage:
#    i) Run tablespace DDL within lock tables mode.
#   ii) Run tablespace DDL when the global read lock
#       is taken.
#  iii) Provoke timeout for table DDL waiting for
#       tablespace MDL IX lock.
#   iv) Provoke timeout for tablespace DDL waiting for
#       tablespace MDL X lock.
#
#
# First, create a tablespace ts1 with one table.
connection default;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1_1.ibd';
CREATE TABLE t1 (pk INTEGER PRIMARY KEY) TABLESPACE ts1;
#
# Run LOCK TABLE followed by tablespace DDL,
# which should fail due to thd->locked_tables_mode.
LOCK TABLE t1 READ;
ALTER TABLESPACE ts1 ADD DATAFILE 'ts1_2.ibd';
ERROR HY000: Can't execute the given command because you have active locked tables or an active transaction
UNLOCK TABLES;
#
# Run FLUSH TABLES WITH READ LOCK followed by tablespace
# DDL, which should fail due to global read lock.
FLUSH TABLES WITH READ LOCK;
ALTER TABLESPACE ts1 ADD DATAFILE 'ts1_2.ibd';
ERROR HY000: Can't execute the query because you have a conflicting read lock
UNLOCK TABLES;
#
# In connection 1, run tablespace DDL and park it
# after getting MDL X lock on the tablespace name.
connection con1;
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name
  SIGNAL got_lock_con1
  WAIT_FOR cont_con1';
ALTER TABLESPACE ts1 ADD DATAFILE 'ts1_2.ibd';
#
# Run table DDL concurrently with a short
# lock wait timeout. Should be aborted due
# to timeout.
connection default;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con1';
SET @start_session_value= @@session.lock_wait_timeout;
SET @@session.lock_wait_timeout= 1;
DROP TABLE t1;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET DEBUG_SYNC= 'now SIGNAL cont_con1';
#
# In connection 1, run table DDL and park it
# after getting MDL IX lock on the tablespace name.
connection con1;
ERROR HY000: Failed to alter: TABLESPACE ts1
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table
  SIGNAL got_lock_con1
  WAIT_FOR cont_con1';
DROP TABLE t1;
#
# Run tablespace DDL concurrently with a short
# lock wait timeout. Should be aborted due
# to timeout.
connection default;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con1';
DROP TABLESPACE ts1;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET DEBUG_SYNC= 'now SIGNAL cont_con1';
#
# Reap the connections, reset DEBUG_SYNC, reset
# lock_wait_timeout and drop objects.
connection con1;
connection default;
SET @@session.lock_wait_timeout= @start_session_value;
DROP TABLESPACE ts1;
SET DEBUG_SYNC= 'RESET';
###########################################################
# TC14. Verify that CREATE TABLE ... LIKE statements are
#       handled correctly, both with and without source
#       tables in general tablespaces.
#
# First, create a tablespace ts1 with one table, and
# another table without an explicit tablespace clause.
connection default;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1_1.ibd';
CREATE TABLE t1_src (pk INTEGER PRIMARY KEY) TABLESPACE ts1;
CREATE TABLE t2_src (pk INTEGER PRIMARY KEY);
#
# Create tables like the source tables, then show the
# definitions, and drop the tables.
CREATE TABLE t1_new LIKE t1_src;
SHOW CREATE TABLE t1_new;
Table	Create Table
t1_new	CREATE TABLE `t1_new` (
  `pk` int NOT NULL,
  PRIMARY KEY (`pk`)
) /*!50100 TABLESPACE `ts1` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t1_new;
CREATE TABLE t2_new LIKE t2_src;
SHOW CREATE TABLE t2_new;
Table	Create Table
t2_new	CREATE TABLE `t2_new` (
  `pk` int NOT NULL,
  PRIMARY KEY (`pk`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
DROP TABLE t2_new;
#
# Reset DEBUG_SYNC and drop objects.
connection default;
DROP TABLE t1_src;
DROP TABLE t2_src;
DROP TABLESPACE ts1;
SET DEBUG_SYNC= 'RESET';
###########################################################
# TC15. Verify that explicit invalid tablespace names make
#       table related DDL be rejected.
#
# First, create a tablespace ts1 with one table.
connection default;
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1_1.ibd';
CREATE TABLE t1 (pk INTEGER PRIMARY KEY) TABLESPACE ts1;
#
# Alter table t1 to use a tablespace with an empty name.
ALTER TABLE t1 TABLESPACE ``;
ERROR 42000: Incorrect tablespace name ``
#
# Alter table t1 to use a tablespace with a name which is
# too long.
ALTER TABLE t1 TABLESPACE `xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx`;
ERROR 42000: Identifier name 'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx' is too long
#
# Create table t2 using a tablespace with an empty name.
CREATE TABLE t2 (pk INTEGER PRIMARY KEY) TABLESPACE ``;
ERROR 42000: Incorrect tablespace name ``
#
# Create table t2 using a tablespace with an name which is
# too long.
CREATE TABLE t2 (pk INTEGER PRIMARY KEY) TABLESPACE `xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx`;
ERROR 42000: Identifier name 'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx' is too long
#
# Drop objects.
connection default;
DROP TABLE t1;
DROP TABLESPACE ts1;
###########################################################
# TC16. Verify correct P_S information.
#
#
# Start executing CREATE TABLESPACE, but wait after
# getting the MDL X lock on the tablespace name.
connection con1;
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name
  SIGNAL got_lock_con1
  WAIT_FOR cont_con1';
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1_1.ibd';
#
# Start executing ALTER TABLESPACE.
connection con2;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con1';
ALTER TABLESPACE ts1 ADD DATAFILE 'ts1_2.ibd';
#
# Verify that the ALTER TABLESPACE is waiting for an MDL
# X lock on the tablespace name (not logged). Then, verify
# that we are also seeing the correct entries in the P_S MDL
# and events_waits_current tables. Further, signal con1 to
# make the CREATE TABLESPACE proceed.
connection default;
SELECT object_type, object_name, lock_type, lock_duration, lock_status
FROM performance_schema.metadata_locks
WHERE object_type LIKE 'TABLESPACE'
  ORDER BY lock_status;
object_type	object_name	lock_type	lock_duration	lock_status
TABLESPACE	ts1	EXCLUSIVE	TRANSACTION	GRANTED
TABLESPACE	ts1	EXCLUSIVE	TRANSACTION	PENDING
SELECT event_name, object_name, object_type, operation
FROM performance_schema.events_waits_current
WHERE object_type LIKE 'TABLESPACE';
event_name	object_name	object_type	operation
wait/lock/metadata/sql/mdl	ts1	TABLESPACE	metadata lock
CALL sys.ps_setup_disable_instrument('wait');
summary
Disabled X instruments
CALL sys.ps_setup_enable_instrument('wait/lock/metadata/sql/mdl');
summary
Enabled 1 instrument
TRUNCATE TABLE performance_schema.events_waits_history_long;
SET DEBUG_SYNC= 'now SIGNAL cont_con1';
#
# Now, the tablespace MDL information will eventually be
# gone from both I_S and P_S, except it will remain in the
# P_S event wait history.
SELECT event_name, object_name, object_type, operation
FROM performance_schema.events_waits_current
WHERE object_type LIKE 'TABLESPACE';
event_name	object_name	object_type	operation
SELECT event_name, object_name, object_type, operation
FROM performance_schema.events_waits_history_long
WHERE object_type LIKE 'TABLESPACE' AND object_name LIKE 'ts1';
event_name	object_name	object_type	operation
wait/lock/metadata/sql/mdl	ts1	TABLESPACE	metadata lock
#
# Reap the connections, reset DEBUG_SYNC and drop objects.
connection con1;
connection con2;
ERROR HY000: Failed to alter: TABLESPACE ts1
connection default;
DROP TABLESPACE ts1;
SET DEBUG_SYNC= 'RESET';
CALL sys.ps_setup_enable_instrument('wait');
summary
Enabled X instruments
###########################################################
#
# Test teardown: Disconnect
#
connection con1;
disconnect con1;
connection con2;
disconnect con2;
connection con3;
disconnect con3;
connection con4;
disconnect con4;
connection con5;
disconnect con5;
connection default;
# Bug#21376265 - ENHANCEMENT: ACQUIRE MDL LOCKS ON ALL TABLESPACE
#                REFERRED BY A STATEMENT
CREATE TABLESPACE ts1 ADD DATAFILE 'ts1.ibd';
CREATE TABLE t1 (
a INT NOT NULL,
PRIMARY KEY (a)
)
ENGINE=InnoDB
TABLESPACE ts1;
#############################################
# Case1: Checking TRUNCATE TABLE
# Sending 'TRUNCATE TABLE t1;'
SET DEBUG_SYNC= 'RESET';
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table
  SIGNAL got_lock
  WAIT_FOR cont';
TRUNCATE TABLE t1;;
# Verify that the tablespace name is locked.
SET DEBUG_SYNC= 'now WAIT_FOR got_lock';
SET DEBUG_SYNC= 'now SIGNAL cont';
# Reaping 'TRUNCATE TABLE t1'
#############################################
# Case2: Checking RENAME TABLE
# Sending 'RENAME TABLE t1 to t2;'
SET DEBUG_SYNC= 'RESET';
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table
  SIGNAL got_lock
  WAIT_FOR cont';
RENAME TABLE t1 to t2;;
# Verify that the tablespace name is locked.
SET DEBUG_SYNC= 'now WAIT_FOR got_lock';
SET DEBUG_SYNC= 'now SIGNAL cont';
# Reaping 'RENAME TABLE t1 to t2'
#############################################
# Case3: Checking ALTER TABLE
# Sending 'ALTER TABLE t1 ADD COLUMN f int;'
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table
  SIGNAL got_lock
  WAIT_FOR cont';
ALTER TABLE t2 ADD COLUMN f int;;
# Verify that the tablespace name is locked.
SET DEBUG_SYNC= 'now WAIT_FOR got_lock';
SET DEBUG_SYNC= 'now SIGNAL cont';
# Reaping 'ALTER TABLE t2 ADD COLUMN f int;'
#############################################
# Case4: Checking CREATE TABLE
SET DEBUG_SYNC= 'RESET';
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table
  SIGNAL got_lock
  WAIT_FOR cont';
CREATE TABLE t3 ( a INT NOT NULL, PRIMARY KEY (a)) ENGINE=InnoDB TABLESPACE ts1;;
# Verify that the tablespace name is locked.
SET DEBUG_SYNC= 'now WAIT_FOR got_lock';
SET DEBUG_SYNC= 'now SIGNAL cont';
# Reaping 'CREATE TABLE'
#############################################
# Case5: Checking CREATE TABLE LIKE
SET DEBUG_SYNC= 'RESET';
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table
  SIGNAL got_lock
  WAIT_FOR cont';
CREATE TABLE t4 LIKE t3;;
# Verify that the tablespace name is locked.
SET DEBUG_SYNC= 'now WAIT_FOR got_lock';
SET DEBUG_SYNC= 'now SIGNAL cont';
# Reaping 'CREATE TABLE LIKE'
#############################################
# Case7: Checking LOCK TABLE t2 WRITE
SET DEBUG_SYNC= 'RESET';
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table
  SIGNAL got_lock
  WAIT_FOR cont';
LOCK TABLE t2 WRITE;
# Verify that the tablespace name is locked.
SET DEBUG_SYNC= 'now WAIT_FOR got_lock';
Timeout in wait_condition.inc for SELECT COUNT(*) = 1 FROM performance_schema.metadata_locks
WHERE object_type LIKE 'TABLESPACE' AND object_name LIKE 'ts2'
SET DEBUG_SYNC= 'now SIGNAL cont';
# Reaping 'LOCK TABLE'
UNLOCK TABLES;
# Cleanup
SET DEBUG_SYNC= 'RESET';
DROP TABLE t2;
DROP TABLE t3;
DROP TABLE t4;
DROP TABLESPACE ts1;
#
# Tescases for wl#8972. Rename a general tablespace.
#
# Verify atomicity of rename
#
CREATE TABLESPACE ts1 ADD DATAFILE 'df1.ibd';
ALTER TABLESPACE ts1 RENAME TO ts2;
ALTER TABLESPACE ts2 RENAME TO ts1;
SET SESSION debug="+d,tspr_post_se";
ALTER TABLESPACE ts1 RENAME TO ts2;
ERROR HY000: Unknown error
SET SESSION debug="-d,tspr_post_se";
ALTER TABLESPACE ts1 RENAME TO ts2;
SET SESSION debug="+d,tspr_post_update";
ALTER TABLESPACE ts2 RENAME TO ts3;
ERROR HY000: Unknown error
SET SESSION debug="-d,tspr_post_update";
ALTER TABLESPACE ts2 RENAME TO ts3;
SET SESSION debug="+d,tspr_post_intcmt";
ALTER TABLESPACE ts3 RENAME TO ts4;
ERROR HY000: Unknown error
SET SESSION debug="-d,tspr_post_intcmt";
ALTER TABLESPACE ts3 RENAME TO ts4;
DROP TABLESPACE ts4;
#
# Verify that rename is rejected under FLUSH TABLES WITH READ LOCK
#
CREATE TABLESPACE ts1 ADD DATAFILE 'df1.ibd';
FLUSH TABLES WITH READ LOCK;
SET @@session.lock_wait_timeout= 1;
ALTER TABLESPACE ts1 RENAME TO ts2;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
UNLOCK TABLES;
#
# Verify that rename is rejected under LOCK TABLES on a table in
# the tablepsace
#
CREATE TABLE t1(i INT) TABLESPACE ts1;
LOCK TABLES t1 READ;
SET @@session.lock_wait_timeout= 1;
ALTER TABLESPACE ts1 RENAME TO ts2;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
UNLOCK TABLES;
DROP TABLE t1;
DROP TABLESPACE ts1;
#
# Verify proper MDL locking on tablespace
#
CREATE TABLESPACE ts1 ADD DATAFILE 'df1.ibd';
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table
  SIGNAL got_lock_con1
  WAIT_FOR cont_con1';
ALTER TABLESPACE ts1 RENAME TO ts2;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con1';
SET @@session.lock_wait_timeout= 1;
DROP TABLESPACE ts1;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET DEBUG_SYNC= 'now SIGNAL cont_con1';
DROP TABLESPACE ts2;
#
# Verify that MDL on tables in a tablespace blocks rename
#
CREATE TABLESPACE ts1 ADD DATAFILE 'df1.ibd';
CREATE TABLE t1(i INT) TABLESPACE ts1;
SET DEBUG_SYNC= 'after_wait_locked_tablespace_name_for_table
  SIGNAL got_lock_con1
  WAIT_FOR cont_con1';
ALTER TABLE t1 RENAME TO t2;
SET DEBUG_SYNC= 'now WAIT_FOR got_lock_con1';
SET @@session.lock_wait_timeout= 1;
ALTER TABLESPACE ts1 RENAME TO t1;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET DEBUG_SYNC= 'now SIGNAL cont_con1';
DROP TABLE t2;
DROP TABLESPACE ts1;
SET @@session.lock_wait_timeout= DEFAULT;
#############################################
#
# Bug#22486020 TARGET TABLESPACE META DATA LOCK
#              NOT ACQUIRED DURING ALTER UNDER
#              LOCK TABLE
#
# Make sure that target tablespaces are locked
# when issuing ALTER TABLE under LOCK TABLE.
#
CREATE TABLESPACE s1 ADD DATAFILE 's1.ibd';
CREATE TABLE t1 (pk INTEGER PRIMARY KEY);
LOCK TABLE t1 WRITE;
ALTER TABLE t1 TABLESPACE s1;
UNLOCK TABLE;
DROP TABLE t1;
DROP TABLESPACE s1;
#
# Bug#27472087: ALTER TABLE HANGS IN "WAITING FOR TABLESPACE METADATA
# LOCK" STATE
#
# Verify that MDLs taken for implicit table and partition tablespaces
# handled correctly in lock tables mode.
#
CREATE TABLE test1(a INT NOT NULL, b CHAR(2) NOT NULL, PRIMARY KEY(a, b))
ENGINE=INNODB;
CREATE TABLE test2(a INT NOT NULL, b CHAR(2) NOT NULL, PRIMARY KEY(a, b))
ENGINE=INNODB;
LOCK TABLES test1 WRITE;
### Locks held ###
SELECT OBJECT_TYPE, OBJECT_NAME, LOCK_TYPE, LOCK_DURATION, LOCK_STATUS
FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'table' AND OBJECT_SCHEMA = 'test' OR
OBJECT_TYPE = 'tablespace'
ORDER BY OBJECT_NAME;
OBJECT_TYPE	OBJECT_NAME	LOCK_TYPE	LOCK_DURATION	LOCK_STATUS
TABLESPACE	test/test1	INTENTION_EXCLUSIVE	TRANSACTION	GRANTED
TABLE	test1	SHARED_NO_READ_WRITE	TRANSACTION	GRANTED
###
ALTER TABLE test1 RENAME test1_tmp;
### Locks held ###
SELECT OBJECT_TYPE, OBJECT_NAME, LOCK_TYPE, LOCK_DURATION, LOCK_STATUS
FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'table' AND OBJECT_SCHEMA = 'test' OR
OBJECT_TYPE = 'tablespace'
ORDER BY OBJECT_NAME;
OBJECT_TYPE	OBJECT_NAME	LOCK_TYPE	LOCK_DURATION	LOCK_STATUS
TABLESPACE	test/test1_tmp	EXCLUSIVE	EXPLICIT	GRANTED
TABLE	test1_tmp	EXCLUSIVE	EXPLICIT	GRANTED
###
ALTER TABLE test2 RENAME test1;
### Locks held ###
SELECT OBJECT_TYPE, OBJECT_NAME, LOCK_TYPE, LOCK_DURATION, LOCK_STATUS
FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'table' AND OBJECT_SCHEMA = 'test' OR
OBJECT_TYPE = 'tablespace'
ORDER BY OBJECT_NAME;
OBJECT_TYPE	OBJECT_NAME	LOCK_TYPE	LOCK_DURATION	LOCK_STATUS
TABLESPACE	test/test1_tmp	EXCLUSIVE	EXPLICIT	GRANTED
TABLE	test1_tmp	EXCLUSIVE	EXPLICIT	GRANTED
###
UNLOCK TABLES;
### Locks held ###
SELECT OBJECT_TYPE, OBJECT_NAME, LOCK_TYPE, LOCK_DURATION, LOCK_STATUS
FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'table' AND OBJECT_SCHEMA = 'test' OR
OBJECT_TYPE = 'tablespace'
ORDER BY OBJECT_NAME;
OBJECT_TYPE	OBJECT_NAME	LOCK_TYPE	LOCK_DURATION	LOCK_STATUS
###
DROP TABLE test1_tmp;
DROP TABLE test1;
# Testing partitioned tables
CREATE TABLE part1(a INT) PARTITION BY HASH (a) PARTITIONS 10;
CREATE TABLE part2(a INT) PARTITION BY HASH (a) PARTITIONS 10;
LOCK TABLES part1 WRITE;
### Locks held ###
SELECT OBJECT_TYPE, REPLACE(OBJECT_NAME, '#p#', '#P#'), LOCK_TYPE,
LOCK_DURATION, LOCK_STATUS
FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'table' AND OBJECT_SCHEMA = 'test' OR
OBJECT_TYPE = 'tablespace'
ORDER BY OBJECT_NAME;
OBJECT_TYPE	REPLACE(OBJECT_NAME, '#p#', '#P#')	LOCK_TYPE	LOCK_DURATION	LOCK_STATUS
TABLE	part1	SHARED_NO_READ_WRITE	TRANSACTION	GRANTED
TABLESPACE	test/part1#P#p0	INTENTION_EXCLUSIVE	TRANSACTION	GRANTED
TABLESPACE	test/part1#P#p1	INTENTION_EXCLUSIVE	TRANSACTION	GRANTED
TABLESPACE	test/part1#P#p2	INTENTION_EXCLUSIVE	TRANSACTION	GRANTED
TABLESPACE	test/part1#P#p3	INTENTION_EXCLUSIVE	TRANSACTION	GRANTED
TABLESPACE	test/part1#P#p4	INTENTION_EXCLUSIVE	TRANSACTION	GRANTED
TABLESPACE	test/part1#P#p5	INTENTION_EXCLUSIVE	TRANSACTION	GRANTED
TABLESPACE	test/part1#P#p6	INTENTION_EXCLUSIVE	TRANSACTION	GRANTED
TABLESPACE	test/part1#P#p7	INTENTION_EXCLUSIVE	TRANSACTION	GRANTED
TABLESPACE	test/part1#P#p8	INTENTION_EXCLUSIVE	TRANSACTION	GRANTED
TABLESPACE	test/part1#P#p9	INTENTION_EXCLUSIVE	TRANSACTION	GRANTED
###
ALTER TABLE part1 RENAME TO part1_tmp;
### Locks held ###
SELECT OBJECT_TYPE, REPLACE(OBJECT_NAME, '#p#', '#P#'), LOCK_TYPE,
LOCK_DURATION, LOCK_STATUS
FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'table' AND OBJECT_SCHEMA = 'test' OR
OBJECT_TYPE = 'tablespace'
ORDER BY OBJECT_NAME;
OBJECT_TYPE	REPLACE(OBJECT_NAME, '#p#', '#P#')	LOCK_TYPE	LOCK_DURATION	LOCK_STATUS
TABLE	part1_tmp	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p0	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p1	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p2	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p3	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p4	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p5	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p6	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p7	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p8	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p9	EXCLUSIVE	EXPLICIT	GRANTED
###
ALTER TABLE part2 RENAME TO part1;
### Locks held ###
SELECT OBJECT_TYPE, REPLACE(OBJECT_NAME, '#p#', '#P#'), LOCK_TYPE,
LOCK_DURATION, LOCK_STATUS
FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'table' AND OBJECT_SCHEMA = 'test' OR
OBJECT_TYPE = 'tablespace'
ORDER BY OBJECT_NAME;
OBJECT_TYPE	REPLACE(OBJECT_NAME, '#p#', '#P#')	LOCK_TYPE	LOCK_DURATION	LOCK_STATUS
TABLE	part1_tmp	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p0	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p1	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p2	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p3	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p4	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p5	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p6	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p7	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p8	EXCLUSIVE	EXPLICIT	GRANTED
TABLESPACE	test/part1_tmp#P#p9	EXCLUSIVE	EXPLICIT	GRANTED
###
UNLOCK TABLES;
### Locks held ###
SELECT OBJECT_TYPE, OBJECT_NAME, LOCK_TYPE, LOCK_DURATION, LOCK_STATUS
FROM performance_schema.metadata_locks
WHERE OBJECT_TYPE = 'table' AND OBJECT_SCHEMA = 'test' OR
OBJECT_TYPE = 'tablespace'
ORDER BY OBJECT_NAME;
OBJECT_TYPE	OBJECT_NAME	LOCK_TYPE	LOCK_DURATION	LOCK_STATUS
###
DROP TABLE part1_tmp;
DROP TABLE part1;
