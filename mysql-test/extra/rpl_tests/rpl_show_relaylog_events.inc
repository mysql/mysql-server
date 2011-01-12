-- connection master
-- source include/rpl_reset.inc

-- connection master

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1);
INSERT INTO t1 VALUES (2);
INSERT INTO t1 VALUES (3);
INSERT INTO t1 VALUES (4);
INSERT INTO t1 VALUES (5);
INSERT INTO t1 VALUES (6);

-- echo [MASTER] ********* SHOW BINLOG EVENTS IN ...  *********
let $binlog_file= query_get_value(SHOW MASTER STATUS, File, 1);
-- source include/show_binlog_events.inc

-- echo [MASTER] ********* SHOW BINLOG EVENTS         *********
let $binlog_file= ;
-- source include/show_binlog_events.inc

-- echo [MASTER] ********* SHOW BINLOG EVENTS ... LIMIT rows  *********
let $binlog_limit= 3;
-- source include/show_binlog_events.inc

-- echo [MASTER] ********* SHOW BINLOG EVENTS ... LIMIT offset,rows  *********
let $binlog_limit= 1,4;
-- source include/show_binlog_events.inc

# clear show_binlog_event/show_relaylog_events parameters
let $binlog_limit= ;

-- sync_slave_with_master

-- echo [SLAVE] ********* SHOW BINLOG EVENTS IN ...   *********
let $binlog_file= query_get_value(SHOW MASTER STATUS, File, 1);
-- source include/show_binlog_events.inc

-- echo [SLAVE] ********* SHOW BINLOG EVENTS          *********
let $binlog_file= ;
-- source include/show_binlog_events.inc

-- echo [SLAVE] ********* SHOW BINLOG EVENTS ... LIMIT rows  *********
let $binlog_limit= 3;
-- source include/show_binlog_events.inc

-- echo [SLAVE] ********* SHOW BINLOG EVENTS ... LIMIT offset,rows  *********
let $binlog_limit= 1,4;
-- source include/show_binlog_events.inc

# clear show_binlog_event/show_relaylog_events parameters
let $binlog_limit= ;

-- echo [SLAVE] ********* SHOW RELAYLOG EVENTS IN ... *********
let $binlog_file= query_get_value(SHOW SLAVE STATUS, Relay_Log_File, 1);
-- source include/show_relaylog_events.inc

-- echo [SLAVE] ********* SHOW RELAYLOG EVENTS        *********
let $binlog_file= ;
-- source include/show_relaylog_events.inc

-- echo [MASTER] ********* SHOW RELAYLOG EVENTS ... LIMIT rows  *********
let $binlog_file= query_get_value(SHOW SLAVE STATUS, Relay_Log_File, 1);
let $binlog_limit= 3;
-- source include/show_relaylog_events.inc

-- echo [MASTER] ********* SHOW RELAYLOG EVENTS ... LIMIT offset,rows  *********
let $binlog_limit= 1,3;
-- source include/show_relaylog_events.inc

FLUSH LOGS;
let $binlog_file= query_get_value(SHOW SLAVE STATUS, Relay_Log_File, 1);

-- connection master
FLUSH LOGS;
DROP TABLE t1;

# clear show_binlog_event/show_relaylog_events parameters
let $binlog_file= ;
let $binlog_limit= ;

-- echo [MASTER] ********* SHOW BINLOG EVENTS IN ...  *********
let $binlog_file= query_get_value(SHOW MASTER STATUS, File, 1);
-- source include/show_binlog_events.inc

-- echo [MASTER] ********* SHOW BINLOG EVENTS         *********
let $binlog_file= ;
-- source include/show_binlog_events.inc

-- sync_slave_with_master

-- echo [SLAVE] ********* SHOW BINLOG EVENTS IN ...   *********
let $binlog_file= query_get_value(SHOW MASTER STATUS, File, 1);
-- source include/show_binlog_events.inc

-- echo [SLAVE] ********* SHOW BINLOG EVENTS          *********
let $binlog_file= ;
-- source include/show_binlog_events.inc

-- echo [SLAVE] ********* SHOW RELAYLOG EVENTS IN ... *********
let $binlog_file= query_get_value(SHOW SLAVE STATUS, Relay_Log_File, 1);
-- source include/show_relaylog_events.inc

-- echo [SLAVE] ********* SHOW RELAYLOG EVENTS        *********
let $binlog_file= ;
-- source include/show_relaylog_events.inc

# clear show_binlog_event/show_relaylog_events parameters
let $binlog_file= ;
let $binlog_limit= ;
