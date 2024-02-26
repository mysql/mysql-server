###################### t/debug_sync.test ###############################
#                                                                      #
# Testing of the Debug Sync Facility.                                  #
#                                                                      #
# There is important documentation within sql/debug_sync.cc            #
#                                                                      #
# Used objects in this test case:                                      #
# p0 - synchronization point 0. Non-existent dummy sync point.         #
# s1 - signal 1.                                                       #
# s2 - signal 2.                                                       #
#                                                                      #
# Creation:                                                            #
# 2008-02-18 istruewing                                                #
#                                                                      #
########################################################################

#
# We need the Debug Sync Facility.
#
--source include/have_debug_sync.inc

--source include/force_myisam_default.inc
--source include/have_myisam.inc

#
# Test.
CREATE TABLE t1 (c1 INT) ENGINE=myisam;
INSERT INTO t1 VALUES (1);
SELECT GET_LOCK('mysqltest_lock', 100);

--echo connection con1
connect (con1,localhost,root,,);
--echo # Sending:
--send UPDATE t1 SET c1=GET_LOCK('mysqltest_lock', 100);

--echo connection con2
connect (con2,localhost,root,,);
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "User lock" and
        info = "UPDATE t1 SET c1=GET_LOCK('mysqltest_lock', 100)";
--source include/wait_condition.inc

# Retain action after use. First used by general_log.
SET DEBUG_SYNC= 'wait_for_lock SIGNAL locked EXECUTE 2';
send INSERT INTO t1 VALUES (1);

--echo connection default
connection default;
# Wait until INSERT waits for lock.
SET DEBUG_SYNC= 'now WAIT_FOR locked';
# let UPDATE continue.
SELECT RELEASE_LOCK('mysqltest_lock');
--echo connection con1
connection con1;
--echo # Reaping UPDATE
reap;
SELECT RELEASE_LOCK('mysqltest_lock');

--echo connection con2
connection con2;
--echo retrieve INSERT result.
reap;
disconnect con1;
disconnect con2;
--echo connection default
connection default;
DROP TABLE t1;

