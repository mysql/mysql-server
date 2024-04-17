#
# Test KILL and KILL QUERY statements.
#
-- source include/have_debug_sync.inc
-- source include/not_threadpool.inc

--disable_warnings
SET DEBUG_SYNC = 'RESET';
DROP TABLE IF EXISTS t1, t2, t3;
DROP FUNCTION IF EXISTS MY_KILL;
--enable_warnings

delimiter |;
# Helper function used to repeatedly kill a session.
CREATE FUNCTION MY_KILL(tid INT) RETURNS INT
BEGIN
  DECLARE CONTINUE HANDLER FOR SQLEXCEPTION BEGIN END;
  KILL tid;
  RETURN (SELECT COUNT(*) = 0 FROM INFORMATION_SCHEMA.PROCESSLIST WHERE ID = tid);
END|
delimiter ;|

connect (con1, localhost, root,,);
connect (con2, localhost, root,,);

# Save id of con1
connection con1;
let $ID= `SELECT @id := CONNECTION_ID()`;
connection con2;
let $ignore= `SELECT @id := $ID`;
connection con1;
# Signal when this connection is terminating.
SET DEBUG_SYNC= 'thread_end SIGNAL con1_end';
# See if we can kill read().
# Run into read() immediately after hitting 'before_do_command_net_read'.
SET DEBUG_SYNC= 'before_do_command_net_read SIGNAL con1_read';

# Kill con1
connection con2;
SET DEBUG_SYNC='now WAIT_FOR con1_read';
# At this point we have no way to figure out, when con1 is blocked in
# reading from the socket. Sending KILL to early would not terminate
# con1. So we repeat KILL until con1 terminates.
let $wait_condition= SELECT MY_KILL(@id);
--source include/wait_condition.inc
# If KILL missed the read(), sync point wait will time out.
SET DEBUG_SYNC= 'now WAIT_FOR con1_end';
SET DEBUG_SYNC = 'RESET';

connection con1;
--error 1053,2006,2013
SELECT 1;

connect;
# this should work, and we should have a new connection_id()
SELECT 1;
let $ignore= `SELECT @id := $ID`;
SELECT @id != CONNECTION_ID();

#make sure the server is still alive
connection con2;
SELECT 4;
connection default;

--error ER_NOT_SUPPORTED_YET
KILL (SELECT COUNT(*) FROM mysql.user);

connection con1;
let $ID= `SELECT @id := CONNECTION_ID()`;
connection con2;
let $ignore= `SELECT @id := $ID`;
connection con1;
# Signal when this connection is terminating.
SET DEBUG_SYNC= 'thread_end SIGNAL con1_end';
# See if we can kill the sync point itself.
# Wait in 'before_do_command_net_read' until killed.
# It doesn't wait for a signal 'kill' but for to be killed.
# The signal name doesn't matter here.
SET DEBUG_SYNC= 'before_do_command_net_read SIGNAL con1_read WAIT_FOR kill';
connection con2;
SET DEBUG_SYNC= 'now WAIT_FOR con1_read';
# Repeat KILL until con1 terminates.
let $wait_condition= SELECT MY_KILL(@id);
--source include/wait_condition.inc
SET DEBUG_SYNC= 'now WAIT_FOR con1_end';
SET DEBUG_SYNC = 'RESET';

connection con1;
--error 1053,2006,2013
SELECT 1;
connect;
SELECT 1;
let $ignore= `SELECT @id := $ID`;
SELECT @id != CONNECTION_ID();
connection con2;
SELECT 4;
connection default;

#
# BUG#14851: killing long running subquery processed via a temporary table.
#

CREATE TABLE t1 (id INT PRIMARY KEY AUTO_INCREMENT);
CREATE TABLE t2 (id INT UNSIGNED NOT NULL);

INSERT INTO t1 VALUES
(0),(0),(0),(0),(0),(0),(0),(0), (0),(0),(0),(0),(0),(0),(0),(0),
(0),(0),(0),(0),(0),(0),(0),(0), (0),(0),(0),(0),(0),(0),(0),(0),
(0),(0),(0),(0),(0),(0),(0),(0), (0),(0),(0),(0),(0),(0),(0),(0),
(0),(0),(0),(0),(0),(0),(0),(0), (0),(0),(0),(0),(0),(0),(0),(0);
INSERT t1 SELECT 0 FROM t1 AS a1, t1 AS a2 LIMIT 4032;

INSERT INTO t2 SELECT id FROM t1;

connection con1;
let $ID= `SELECT @id := CONNECTION_ID()`;
connection con2;
let $ignore= `SELECT @id := $ID`;

connection con1;
SET DEBUG_SYNC= 'thread_end SIGNAL con1_end';
SET DEBUG_SYNC= 'before_acos_function SIGNAL in_sync';
--source include/turn_off_only_full_group_by.inc
# This is a very long running query. If this test start failing,
# it may be necessary to change to an even longer query.
# Hash join will make the query run too long before hitting the DEBUG_SYNC-point
# 'before_acos_function', because the hash join iterator will materialize a lot
# of data before we get so far.
send SELECT id FROM t1 WHERE id IN
       (SELECT /*+ NO_BNL(a,b,c,d) */ DISTINCT a.id FROM t2 a, t2 b, t2 c, t2 d
          GROUP BY ACOS(1/a.id), b.id, c.id, d.id
          HAVING a.id BETWEEN 10 AND 20);

connection con2;
SET DEBUG_SYNC= 'now WAIT_FOR in_sync';
KILL @id;
SET DEBUG_SYNC= 'now WAIT_FOR con1_end';

connection con1;
--error 1317,1053,2006,2013
reap;
connect;
SELECT 1;

connection default;
SET DEBUG_SYNC = 'RESET';
DROP TABLE t1, t2;

#
# Test of blocking of sending ERROR after OK or EOF
#
connection con1;
let $ID= `SELECT @id := CONNECTION_ID()`;
connection con2;
let $ignore= `SELECT @id := $ID`;
connection con1;
SET DEBUG_SYNC= 'before_acos_function SIGNAL in_sync WAIT_FOR kill';
send SELECT ACOS(0);
connection con2;
SET DEBUG_SYNC= 'now WAIT_FOR in_sync';
KILL QUERY @id;
connection con1;
reap;
SELECT 1;
SELECT @id = CONNECTION_ID();
connection default;
SET DEBUG_SYNC = 'RESET';

#
# Bug#28598: mysqld crash when killing a long-running explain query.
#
connection con1;
let $ID= `SELECT @id := CONNECTION_ID()`;
connection con2;
let $ignore= `SELECT @id := $ID`;
connection con1;
--disable_query_log
let $tab_count= 40;

let $i= $tab_count;
while ($i)
{
  eval CREATE TABLE t$i (a$i INT, KEY(a$i));
  eval INSERT INTO t$i VALUES (1),(2),(3),(4),(5),(6),(7);
  dec $i ;
}
SET SESSION optimizer_search_depth=0;

let $i=$tab_count;
while ($i)
{
  let $a= a$i;
  let $t= t$i;
  dec $i;
  if ($i)
  {
    let $comma=,;
    let $from=$comma$t$from;
    let $where=a$i=$a $and $where;
  }
  if (!$i)
  {
    let $from=FROM $t$from;
    let $where=WHERE $where;
  }
  let $and=AND;
}

--enable_query_log
SET DEBUG_SYNC= 'before_join_optimize SIGNAL in_sync WAIT_FOR continue';
eval PREPARE stmt FROM 'EXPLAIN SELECT * $from $where';
send EXECUTE stmt;

connection con2;
SET DEBUG_SYNC= 'now WAIT_FOR in_sync';
KILL QUERY @id;
SET DEBUG_SYNC= 'now SIGNAL continue';
connection con1;
--error 1317
reap;
--disable_query_log
let $i= $tab_count;
while ($i)
{
  eval DROP TABLE t$i;
  dec $i ;
}
--enable_query_log
connection default;
SET DEBUG_SYNC = 'RESET';

--echo #
--echo # Bug#19723: kill of active connection yields different error code
--echo # depending on platform.
--echo #

--echo
--echo # Connection: con1.
--connection con1
let $ID= `SELECT @id := CONNECTION_ID()`;
SET DEBUG_SYNC= 'thread_end SIGNAL con1_end';
--error ER_QUERY_INTERRUPTED
KILL @id;

connection con2;
SET DEBUG_SYNC= 'now WAIT_FOR con1_end';
connection con1;
--echo # ER_SERVER_SHUTDOWN, CR_SERVER_GONE_ERROR, CR_SERVER_LOST,
--echo # depending on the timing of close of the connection socket
--error 1053,2006,2013
SELECT 1;
connect;
SELECT 1;
let $ignore= `SELECT @id := $ID`;
SELECT @id != CONNECTION_ID();
connection default;
disconnect con1;
SET DEBUG_SYNC = 'RESET';

###########################################################################

SET DEBUG_SYNC = 'RESET';
DROP FUNCTION MY_KILL;


--echo #
--echo # Bug#12661349 assert in protocol::end_statement
--echo #

--echo # Note: This test case should be run with --ps-protocol

CREATE TABLE t1 (col1 INT);
--enable_connect_log

--connect(con1, localhost, root)
--let $con1_id= `SELECT CONNECTION_ID()`
SET DEBUG_SYNC= 'before_execute_sql_command SIGNAL waiting WAIT_FOR killed';
--send SELECT * FROM t1

--connection default
SET DEBUG_SYNC= 'now WAIT_FOR waiting';
--replace_result $con1_id <con1_id>
--eval KILL QUERY $con1_id
SET DEBUG_SYNC= 'now SIGNAL killed';

--connection con1
# Here the server asserts when running with "--ps-protocol"
--error ER_QUERY_INTERRUPTED
--reap
--disconnect con1
--source include/wait_until_disconnected.inc

--connection default
DROP TABLE t1;
--disable_connect_log
SET DEBUG_SYNC= 'RESET';
