# Routine for checking
# - connect, run some statement, disconnect
# - kill connection, kill query of some connection
# when either parallel connections or the affected connection itself (kill experiments)
# are in some interesting state (high load, waiting for lock, inactive, user sleep etc.)
# Therefore this test is a complement to the existing tests for kill, connect, disconnect
# within the main testsuite.
#
# The variable $connections_to_add must be set before calling this routine.
# Recommended values:
#    - @@global.thread_pool_size in case thread pooling functionality is loaded and
#      thread_pool_size > 1, @@global.thread_pool_size = 5 is recommended
#    - something like 5 in case thread pooling functionality is not available
#
# IMHO the weakest point within this routine is the statement $high_load.
# It must have a runtime of clear more than ~30 seconds.
#

--echo # Check and generate prerequisites used in various tests.
--disable_warnings
DROP TABLE IF EXISTS t1;
DROP TABLE IF EXISTS t2;
--enable_warnings
# Attention: This is no test for the some storage engine.
#            MEMORY is used because it is a fast engine.
CREATE TABLE t1 (connection_name VARCHAR(20), id BIGINT, PRIMARY KEY(connection_name))
ENGINE = MEMORY;
CREATE TABLE t2 (col1 BIGINT) ENGINE = MEMORY;
INSERT INTO t2 VALUES(0),(1),(2),(3),(4),(5),(6),(7),(8),(9),(11);
let $select_sleep= SELECT SLEEP(30);
# The execution of the next statement must require >= ~30 seconds even on a box without
# parallel load.
# The WHERE condition should ensure that the optimizer has no chance to make the statement fast
# and it should cause some work by the server.
# ~  47s, 8 table join with  43046721 rows result, 10 rows per table, tmpfs, one core 100%
# ~ 110s, 8 table join with 100000000 rows result, 11 rows per table, tmpfs, one core 100%
# There is no dramatic increase in performance when placing vardir on tmpfs instead of disc
# based filesystem and the space consumption in vardir is rather low (> 100 MB).
#
let $high_load=
SELECT COUNT(*)
FROM t2 C1, t2 C2, t2 C3, t2 C4, t2 C5 , t2 C6 , t2 C7 , t2 C8
WHERE C1.col1 <> 1 AND C2.col1 <> 2 AND C3.col1 <> 3 AND C4.col1 <> 4 AND C5.col1 <> 5
  AND C6.col1 <> 6 AND C7.col1 <> 7 AND C8.col1 <> 8 ;


# 1. Ensure that KILL <CONNECTION> works
# 1.1 KILL of connection by other connection
--echo #--------------------------------------
--echo # 1.1.1 KILL other inactive connection.
--echo #--------------------------------------
let $connection= con_to_kill1;
--echo # Establish connection $connection
--connect ($connection, localhost, root,,)
let $id_list= `SELECT CONNECTION_ID()`;

--echo # Switch to connection default
--connection default
--echo # Poll till the just started session is visible within the processlist
--echo # and command is "sleep".
let $wait_condition= SELECT COUNT(*) = 1 FROM information_schema.processlist
WHERE id = $id_list AND command = 'sleep' AND info IS NULL;
--source include/wait_condition.inc
if(!$success)
{
   --echo # ERROR The just started session with id = $id_list did not reach the state with
   --echo #       command = 'sleep' AND info IS NULL
   eval SELECT * FROM information_schema.processlist WHERE id = $id_list;
   --sleep 30
   eval SELECT * FROM information_schema.processlist WHERE id = $id_list;
   --echo # abort
   exit;
}
#
--replace_result $id_list <target_id>
eval KILL $id_list;
#
--echo # Poll till the just killed session is no more visible within the processlist.
--source ../inc/wait_until_disappeared.inc


let $statement= $select_sleep;
--echo #--------------------------------------------------
--echo # 1.1.2 KILL other active connection which executes
--echo #       $statement
--echo #       and just does the "sleep" from the SELECT.
--echo #--------------------------------------------------
let $connection= con_to_kill2;
--echo # Establish connection $connection
--connect ($connection, localhost, root,,)
let $id_list= `SELECT CONNECTION_ID()`;
--echo # Send statement to the server, don't wait for results
--send
eval $statement;
--echo # Switch to connection default
--connection default
--echo # Poll till the just started session and its SELECT is visible within the processlist
--echo # and the state contains the string 'sleep'.
let $wait_condition= SELECT COUNT(*) = 1 FROM information_schema.processlist
WHERE id = $id_list AND state LIKE '%sleep%' AND info = '$statement';
--source include/wait_condition.inc
if(!$success)
{
   --echo # ERROR The just started session with id = $id_list did not reach the state with
   --echo #       state LIKE '%sleep%' AND info = '$statement'
   eval SELECT * FROM information_schema.processlist WHERE id = $id_list;
   --sleep 30
   eval SELECT * FROM information_schema.processlist WHERE id = $id_list;
   --echo # abort
   exit;
}
#
--replace_result $id_list <target_id>
eval KILL $id_list;
#
--echo # Poll till the just killed session is no more visible within the processlist.
--source ../inc/wait_until_disappeared.inc


LOCK TABLE t1 WRITE;
let $statement= SELECT * FROM t1;
--echo #--------------------------------------------------
--echo # 1.1.3 KILL other active connection which executes
--echo #       $statement
--echo #       and just waits for getting a lock.
--echo #--------------------------------------------------
let $connection= con_to_kill3;
--echo # Establish connection $connection
--connect ($connection, localhost, root,,)
let $id_list= `SELECT CONNECTION_ID()`;
--echo # Send statement to the server, don't wait for results
--send
eval $statement;
--echo # Switch to connection default
--connection default
--echo # Poll till the just started session and its SELECT is visible within the processlist
--echo # and the state contains the string 'lock'.
let $wait_condition= SELECT COUNT(*) = 1 FROM information_schema.processlist
WHERE id = $id_list AND state LIKE '%lock%' AND info = "$statement";
--source include/wait_condition.inc
if(!$success)
{
   --echo # ERROR The just started session with id = $id_list did not reach the state with
   --echo #       state LIKE '%lock%' AND info = "$statement"
   eval SELECT * FROM information_schema.processlist WHERE id = $id_list;
   --sleep 30
   eval SELECT * FROM information_schema.processlist WHERE id = $id_list;
   --echo # abort
   exit;
}
#
--replace_result $id_list <target_id>
eval KILL $id_list;
#
--echo # Poll till the just killed session is no more visible within the processlist.
--source ../inc/wait_until_disappeared.inc
UNLOCK TABLES;


let $statement= $high_load;
--echo #--------------------------------------------------------------
--echo # 1.1.4 KILL other active connection which executes a huge join
--echo #       and just causes a high load.
--echo #--------------------------------------------------------------
let $connection= con_to_kill4;
--echo # Establish connection $connection
--connect ($connection, localhost, root,,)
let $id_list= `SELECT CONNECTION_ID()`;
--echo # Send statement to the server, don't wait for results
--send
eval $statement;
--echo # Switch to connection default
--connection default
--echo # Poll till the just started session and its SELECT is visible within the processlist
--echo # and time > 0.
let $wait_condition= SELECT COUNT(*) = 1 FROM information_schema.processlist
WHERE id = $id_list AND time > 0 AND info = "$statement";
--source include/wait_condition.inc
if(!$success)
{
   --echo # ERROR The just started session with id = $id_list did not reach the state with
   --echo #       time > 0 AND info = "$statement"
   eval SELECT * FROM information_schema.processlist WHERE id = $id_list;
   --sleep 30
   eval SELECT * FROM information_schema.processlist WHERE id = $id_list;
   --echo # abort
   exit;
}
#
--replace_result $id_list <target_id>
eval KILL $id_list;
#
--echo # Poll till the just killed session is no more visible within the processlist.
--source ../inc/wait_until_disappeared.inc


--echo #--------------------------------
--echo # 1.2 KILL own active connection.
--echo #--------------------------------
let $connection= con_to_kill5;
--echo # Establish connection $connection
--connect ($connection, localhost, root,,)
let $id_list= `SELECT CONNECTION_ID()`;
#
--error ER_QUERY_INTERRUPTED
KILL CONNECTION_ID();
#
--echo # Switch to connection default
--connection default
--echo # Poll till the just killed session is no more visible within the processlist.
--source ../inc/wait_until_disappeared.inc


# 2. Ensure that KILL QUERY <CONNECTION> works.
let $statement= $select_sleep;
--echo #------------------------------------------------------------
--echo # 2.1 KILL ongoing query of another connection which executes
--echo #     $statement
--echo #     and just does the "sleep" from the SELECT.
--echo #------------------------------------------------------------
let $connection= con_to_kill6;
--echo # Establish connection $connection
--connect ($connection, localhost, root,,)
let $id_list= `SELECT CONNECTION_ID()`;
--echo # Send statement to the server, reap result later
--send
eval $statement;
--echo # Switch to connection default
--connection default
--echo # Poll till execution of '$statement' has started
let $wait_condition= SELECT COUNT(*) = 1 FROM information_schema.processlist
WHERE id = $id_list AND state LIKE '%sleep%' AND info = "$statement";
--source include/wait_condition.inc
if(!$success)
{
   --echo # ERROR The just started session with id = $id_list did not reach the state with
   --echo #       state LIKE '%sleep%' AND info = "$statement"
   eval SELECT * FROM information_schema.processlist WHERE id = $id_list;
   --sleep 30
   eval SELECT * FROM information_schema.processlist WHERE id = $id_list;
   --echo # abort
   exit;
}
#
--replace_result $id_list <target_id>
eval KILL QUERY $id_list;
#
--echo # Switch to connection $connection
--connection $connection
#    The session should exist and be not damaged.
#    -> Reap should report that the query was interrupted.
--echo # Reap result
# I am unsure if it is correct that the server response is not ER_QUERY_INTERRUPTED
# --error ER_QUERY_INTERRUPTED
--reap
--echo # Switch to connection default
--connection default

LOCK TABLE t1 WRITE;
let $statement= SELECT * FROM t1;
--echo #------------------------------------------------------------
--echo # 2.2 KILL ongoing query of another connection which executes
--echo #     $statement
--echo #     and just waits for getting a lock.
--echo #------------------------------------------------------------
--echo # Switch to connection $connection
--connection $connection
let $id_list= `SELECT CONNECTION_ID()`;
--echo # Send statement to the server, reap result later
--send
eval $statement;
--echo # Switch to connection default
--connection default
--echo # Poll till execution of '$statement' has started
--echo # and state contains the string 'lock'
let $wait_condition= SELECT COUNT(*) = 1 FROM information_schema.processlist
WHERE id = $id_list AND state LIKE '%lock%' AND info = "$statement";
--source include/wait_condition.inc
if(!$success)
{
   --echo # ERROR The session with id = $id_list did not reach the state with
   --echo #       state LIKE '%lock%' AND info = "$statement"
   eval SELECT * FROM information_schema.processlist WHERE id = $id_list;
   --sleep 30
   eval SELECT * FROM information_schema.processlist WHERE id = $id_list;
   --echo # abort
   exit;
}
#
--replace_result $id_list <target_id>
eval KILL QUERY $id_list;
#
--echo # Switch to connection $connection
--connection $connection
#    The session should exist and be not damaged.
#    -> Reap should report that the query was interrupted.
--echo # Reap result
--error ER_QUERY_INTERRUPTED
--reap
--connection default
UNLOCK TABLES;

let $statement= $high_load;
--echo #------------------------------------------------------------
--echo # 2.3 KILL ongoing query of another connection which executes
--echo #     a huge join and just causes a high load.
--echo #------------------------------------------------------------
--echo # Switch to connection $connection
--connection $connection
let $id_list= `SELECT CONNECTION_ID()`;
--echo # Send statement to the server, reap result later
--send
eval $statement;
--echo # Switch to connection default
--connection default
--echo # Poll till the execution of '$statement' has started
--echo # and time > 0.
let $wait_condition= SELECT COUNT(*) = 1 FROM information_schema.processlist
WHERE id = $id_list AND time > 0 AND info = "$statement";
--source include/wait_condition.inc
if(!$success)
{
   --echo # ERROR The session with id = $id_list did not reach the state with
   --echo #       time > 0 AND info = "$statement"
   eval SELECT * FROM information_schema.processlist WHERE id = $id_list;
   --sleep 30
   eval SELECT * FROM information_schema.processlist WHERE id = $id_list;
   --echo # abort
   exit;
}
#
--replace_result $id_list <target_id>
eval KILL QUERY $id_list;
#
--echo # Switch to connection $connection
--connection $connection
#    The session should exist and be not damaged.
#    -> Reap should report that the query was interrupted.
--echo # Reap result
--error ER_QUERY_INTERRUPTED
--reap

--echo #-------------------------------------
--echo # 2.4 KILL own query (pathologic case)
--echo #-------------------------------------
--echo # Switch to connection $connection
--connection $connection
#
--error ER_QUERY_INTERRUPTED
KILL QUERY CONNECTION_ID();
#
--echo # Switch to connection default
--connection default
--echo # Poll till it becomes visible that the connection $connection is
--echo # inactive.
let $wait_condition= SELECT COUNT(*) = 1 FROM information_schema.processlist
WHERE id = $id_list AND command = 'sleep' AND info IS NULL;
--source include/wait_condition.inc
if(!$success)
{
   --echo # ERROR The session with id = $id_list did not reach the state with
   --echo #       command = 'sleep' AND info IS NULL
   eval SELECT * FROM information_schema.processlist WHERE id = $id_list;
   --sleep 30
   eval SELECT * FROM information_schema.processlist WHERE id = $id_list;
   --echo # abort
   exit;
}


--echo #---------------------------------
--echo # 3. Disconnect current connection
--echo #---------------------------------
--echo # Switch to connection $connection
--connection $connection
#
--echo # Disconnect connection $connection
--disconnect $connection
#
--connection default
--echo # Poll till the connection $connection disappeared from the processlist.
--source ../inc/wait_until_disappeared.inc


--echo #--------------------------------------------------------------
--echo # 4. Thread pool group related tests
--echo #    In case these tests are applied to a server without thread
--echo #    pooling than the comments are partially no more valid.
--echo #--------------------------------------------------------------
# Ensure that some connection A (we have one per thread group) executing some statement and just
# being in the state
# - "user sleep"
# - wait for some lock
# - cause a high load
# does not prevent that
# 1. some new connection "con_extra" to the thread group containing A could be established, run
#    some statement and disconnect
# 2. some existing connection C being in whatever thread group could kill the query executed by all
#    connections A in every thread group
--echo # Establish connections_to_add (= thread_pool_size if possible) additional connections
--echo # so that every thread group contains one of these additional connections.
let $run= $connections_to_add;
--disable_query_log
while($run)
{
   --connect (con$run, localhost, root,,)
   eval INSERT INTO t1 SET connection_name = 'con$run', id = CONNECTION_ID();
   dec $run;
}
--connection default
--enable_query_log
--echo # Switch to connection default
--connection default
--replace_result $connections_to_add <connections_to_add>
eval SELECT COUNT(*) = $connections_to_add AS "Expect 1" FROM t1;


let $statement= $select_sleep;
--echo #-------------------------------------------------------------------------
--echo # 4.1 Have within every thread group a connection A executing
--echo #        $statement
--echo #     and just doing the "sleep" from the SELECT.
--echo #     Show that
--echo #     - con_extra: connect, run some statement, disconnect
--echo #     - default:   run SELECT ... and KILL QUERY <A>
--echo #     works.
--echo #-------------------------------------------------------------------------
let $run= $connections_to_add;
--disable_query_log
while($run)
{
   --connection con$run
   --send
   eval $statement;
   dec $run;
}
--enable_query_log
--echo # Switch to connection default
--connection default
--echo # Poll till connections_to_add connections are busy with their statement.
let $wait_condition= SELECT COUNT(*) = $connections_to_add FROM information_schema.processlist
                     WHERE state LIKE '%sleep%' AND info = '$statement';
--source include/wait_condition.inc
if(!$success)
{
   --echo # ERROR The $connections_to_add sessions did not reach the state with
   --echo #       state LIKE '%sleep%' AND info = '$statement'
   SELECT * FROM information_schema.processlist;
   --sleep 30
   SELECT * FROM information_schema.processlist;
   --echo # abort
   exit;
}

let $connection= con_extra;
--echo # Establish connection $connection
--connect ($connection, localhost, root,,)
let $id_list= `SELECT CONNECTION_ID()`;
--replace_result $connections_to_add <connections_to_add>
eval SET @connections_to_add = $connections_to_add;
eval SELECT COUNT(*) = @connections_to_add AS "Expect 1" FROM information_schema.processlist
WHERE info = '$statement';
--echo # Switch to connection default
--connection default
--echo # Disconnect the additional session $connection
--disconnect $connection
--echo # Poll till the connection $connection disappeared from the processlist.
--source ../inc/wait_until_disappeared.inc

--echo # KILL the queries of all additional sessions (printing is suppressed).
let $run= $connections_to_add;
--disable_query_log
while($run)
{
   eval SELECT id INTO @target_id FROM t1 WHERE connection_name = 'con$run';
   KILL QUERY @target_id;
   dec $run;
}
--enable_query_log
--echo # Let all additional sessions reap their results (printing is suppressed).
--disable_result_log
let $run= $connections_to_add;
while($run)
{
   --connection con$run
   --reap
   dec $run;
}
--enable_result_log
--echo # Switch to connection default
--connection default


LOCK TABLE t1 WRITE;
let $statement= SELECT * FROM t1;
--echo #-------------------------------------------------------------------------
--echo # 4.2 Have within every thread group a connection A executing
--echo #        $statement
--echo #     and just waiting for a lock.
--echo #     Show that
--echo #     - con_extra: connect, run some statement, disconnect
--echo #     - default:   run SELECT ... and KILL QUERY <A>
--echo #     works.
--echo #-------------------------------------------------------------------------
let $run= $connections_to_add;
--disable_query_log
while($run)
{
   --connection con$run
   --send
   eval $statement;
   dec $run;
}
--enable_query_log
--echo # Switch to connection default
--connection default
--echo # Poll till connections_to_add connections are busy with their statement.
let $wait_condition= SELECT COUNT(*) = $connections_to_add FROM information_schema.processlist
                     WHERE state LIKE '%lock%' AND info = '$statement';
--source include/wait_condition.inc
if(!$success)
{
   --echo # ERROR The $connections_to_add sessions did not reach the state with
   --echo #       state LIKE '%lock%' AND info = '$statement'
   SELECT * FROM information_schema.processlist;
   --sleep 30
   SELECT * FROM information_schema.processlist;
   --echo # abort
   exit;
}

let $connection= con_extra;
--echo # Establish connection $connection
--connect ($connection, localhost, root,,)
let $id_list= `SELECT CONNECTION_ID()`;
--replace_result $connections_to_add <connections_to_add>
eval SET @connections_to_add = $connections_to_add;
eval SELECT COUNT(*) = @connections_to_add AS "Expect 1" FROM information_schema.processlist
WHERE info = '$statement';
--echo # Switch to connection default
--connection default
--echo # Disconnect the additional session $connection
--disconnect $connection
--echo # Poll till the connection $connection disappeared from the processlist.
--source ../inc/wait_until_disappeared.inc

--echo # KILL the queries of all additional sessions (printing is suppressed).
let $run= $connections_to_add;
--disable_query_log
while($run)
{
   eval SELECT id INTO @target_id FROM t1 WHERE connection_name = 'con$run';
   KILL QUERY @target_id;
   dec $run;
}
--enable_query_log

--echo # Let all additional sessions reap their results (printing is suppressed).
--disable_result_log
let $run= $connections_to_add;
while($run)
{
   --connection con$run
   --error ER_QUERY_INTERRUPTED
   --reap
   dec $run;
}
--enable_result_log
--echo # Switch to connection default
--connection default
UNLOCK TABLES;


let $statement= $high_load;
--echo #-------------------------------------------------------------------------
--echo # 4.3 Have within every thread group a connection A executing a huge join
--echo #     and just causing high load.
--echo #     Show that
--echo #     - con_extra: connect, run some statement, disconnect
--echo #     works.
--echo #-------------------------------------------------------------------------
let $run= $connections_to_add;
--disable_query_log
while($run)
{
   --connection con$run
   --send
   eval $statement;
   dec $run;
}
--enable_query_log
--echo # Switch to connection default
--connection default
--echo # Poll till connections_to_add connections are busy with their statement.
let $wait_condition= SELECT COUNT(*) = $connections_to_add FROM information_schema.processlist
                     WHERE time > 0 AND info = '$statement';
--source include/wait_condition.inc
if(!$success)
{
   --echo # ERROR The $connections_to_add sessions did not reach the state with
   --echo #       time > 0 AND info = '$statement'
   SELECT * FROM information_schema.processlist;
   --sleep 30
   SELECT * FROM information_schema.processlist;
   --echo # abort
}

let $connection= con_extra;
--echo # Establish connection $connection
--connect ($connection, localhost, root,,)
--replace_result $connections_to_add <connections_to_add>
eval SET @connections_to_add = $connections_to_add;
eval SELECT COUNT(*) = @connections_to_add AS "Expect 1" FROM information_schema.processlist
WHERE info = '$statement';
let $id_list= `SELECT CONNECTION_ID()`;
--echo # Switch to connection default
--connection default
--echo # Disconnect the additional session $connection
--disconnect $connection
--echo # Poll till the connection $connection disappeared from the processlist.
--source ../inc/wait_until_disappeared.inc
--enable_query_log


--echo #--------------------------------------------------------------------------------------
--echo # 4.4 Reveal that some session having another session causing high load within the same
--echo #     thread group is able to KILL this other session.
--echo #--------------------------------------------------------------------------------------
--echo # KILL the queries of all additional sessions (printing is suppressed).
let $run= $connections_to_add;
--disable_query_log
while($run)
{
   eval SELECT id INTO @target_id FROM t1 WHERE connection_name = 'con$run';
   KILL @target_id;
   dec $run;
}
--enable_query_log
--echo # Poll till there is no connection running which has an id occurring in t1
let $wait_condition= SELECT COUNT(*) = 0 FROM information_schema.processlist
                     WHERE id IN (SELECT id FROM t1);
--source include/wait_condition.inc
if(!$success)
{
   --echo # ERROR Not all sessions disappeared from processlist
   SELECT * FROM information_schema.processlist WHERE id IN (SELECT id FROM t1);
   --sleep 30
   SELECT * FROM information_schema.processlist WHERE id IN (SELECT id FROM t1);
   --echo # abort
   exit;
}

# Cleanup
DROP TABLE t1;
DROP TABLE t2;

