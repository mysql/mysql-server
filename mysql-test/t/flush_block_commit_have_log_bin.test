# Let's see if FLUSH TABLES WITH READ LOCK blocks COMMIT of existing
# transactions.
# We verify that we did not introduce a deadlock.
# This is intended to mimick how mysqldump and innobackup work.

--source include/have_log_bin.inc

--echo # Save the initial number of concurrent sessions
--source include/count_sessions.inc


--echo # Establish connection con1 (user=root)
connect (con1,localhost,root,,);
--echo # Establish connection con2 (user=root)
connect (con2,localhost,root,,);

# FLUSH TABLES WITH READ LOCK should block writes to binlog too
--echo # Switch to connection con1
connection con1;
CREATE TABLE t1 (a INT) ENGINE=innodb;
RESET BINARY LOGS AND GTIDS;
SET AUTOCOMMIT=0;
SELECT 1;
--echo # Switch to connection con2
connection con2;
FLUSH TABLES WITH READ LOCK;
--source include/rpl/deprecated/show_binlog_events.inc
--echo # Switch to connection con1
connection con1;
send INSERT INTO t1 VALUES (1);
--echo # Switch to connection con2
connection con2;
sleep 1;
--source include/rpl/deprecated/show_binlog_events.inc
UNLOCK TABLES;
--echo # Switch to connection con1
connection con1;
reap;
DROP TABLE t1;
SET AUTOCOMMIT=1;

# GLR blocks new transactions
create table t1 (a int) engine=innodb;
connection con1;
flush tables with read lock;
connection con2;
begin;
--send insert into t1 values (1);
connection con1;
let $wait_condition=
  select count(*) = 1 from information_schema.processlist
  where state = "Waiting for global read lock" and
        info = "insert into t1 values (1)";
--source include/wait_condition.inc
unlock tables;
connection con2;
--reap
commit;
drop table t1;

--echo # Switch to connection default and close connections con1 and con2
connection default;
disconnect con1;
disconnect con2;

--echo # Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc

