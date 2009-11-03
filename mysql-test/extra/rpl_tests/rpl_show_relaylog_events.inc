-- connection master

CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1);
INSERT INTO t1 VALUES (2);
INSERT INTO t1 VALUES (3);
INSERT INTO t1 VALUES (4);
INSERT INTO t1 VALUES (5);
INSERT INTO t1 VALUES (6);

-- echo [MASTER] ********* SOW BINLOG EVENTS IN ...  *********
let $binary_log_file= master-bin.000001;
-- source include/show_binlog_events.inc

-- echo [MASTER] ********* SOW BINLOG EVENTS         *********
let $binary_log_file= ;
-- source include/show_binlog_events.inc

-- echo [MASTER] ********* SOW BINLOG EVENTS ... LIMIT rows  *********
let $binary_log_file= ;
let $binary_log_limit_row= 3;
-- source include/show_binlog_events.inc

-- echo [MASTER] ********* SOW BINLOG EVENTS ... LIMIT offset,rows  *********
let $binary_log_file= ;
let $binary_log_limit_row= 4;
let $binary_log_limit_offset= 1;
-- source include/show_binlog_events.inc

# clear show_binlog_event/show_relaylog_events parameters
let $binary_log_file= ;
let $binary_log_limit_row= ;
let $binary_log_limit_offset= ;

-- sync_slave_with_master

-- echo [SLAVE] ********* SOW BINLOG EVENTS IN ...   *********
let $binary_log_file= slave-bin.000001;
-- source include/show_binlog_events.inc

-- echo [SLAVE] ********* SOW BINLOG EVENTS          *********
let $binary_log_file= ;
-- source include/show_binlog_events.inc

-- echo [SLAVE] ********* SOW BINLOG EVENTS ... LIMIT rows  *********
let $binary_log_file= ;
let $binary_log_limit_row= 3;
-- source include/show_binlog_events.inc

-- echo [SLAVE] ********* SOW BINLOG EVENTS ... LIMIT offset,rows  *********
let $binary_log_file= ;
let $binary_log_limit_row= 4;
let $binary_log_limit_offset= 1;
-- source include/show_binlog_events.inc

# clear show_binlog_event/show_relaylog_events parameters
let $binary_log_file= ;
let $binary_log_limit_row= ;
let $binary_log_limit_offset= ;

-- echo [SLAVE] ********* SOW RELAYLOG EVENTS IN ... *********
let $binary_log_file= slave-relay-bin.000003;
-- source include/show_relaylog_events.inc

-- echo [SLAVE] ********* SOW RELAYLOG EVENTS        *********
let $binary_log_file= ;
-- source include/show_relaylog_events.inc

-- echo [MASTER] ********* SOW RELAYLOG EVENTS ... LIMIT rows  *********
let $binary_log_file= slave-relay-bin.000003;
let $binary_log_limit_row= 3;
let $binary_log_limit_offset= ;
-- source include/show_relaylog_events.inc

-- echo [MASTER] ********* SOW RELAYLOG EVENTS ... LIMIT offset,rows  *********
let $binary_log_file= slave-relay-bin.000003;
let $binary_log_limit_offset= 1;
let $binary_log_limit_row= 3;
-- source include/show_relaylog_events.inc

FLUSH LOGS;

-- connection master
FLUSH LOGS;
DROP TABLE t1;

# clear show_binlog_event/show_relaylog_events parameters
let $binary_log_file= ;
let $binary_log_limit_row= ;
let $binary_log_limit_offset= ;

-- echo [MASTER] ********* SOW BINLOG EVENTS IN ...  *********
let $binary_log_file= master-bin.000002;
-- source include/show_binlog_events.inc

-- echo [MASTER] ********* SOW BINLOG EVENTS         *********
let $binary_log_file= ;
-- source include/show_binlog_events.inc

-- sync_slave_with_master

-- echo [SLAVE] ********* SOW BINLOG EVENTS IN ...   *********
let $binary_log_file= slave-bin.000002;
-- source include/show_binlog_events.inc

-- echo [SLAVE] ********* SOW BINLOG EVENTS          *********
let $binary_log_file= ;
-- source include/show_binlog_events.inc

-- echo [SLAVE] ********* SOW RELAYLOG EVENTS IN ... *********
let $binary_log_file= slave-relay-bin.000005;
-- source include/show_relaylog_events.inc

-- echo [SLAVE] ********* SOW RELAYLOG EVENTS        *********
let $binary_log_file= ;
-- source include/show_relaylog_events.inc

# clear show_binlog_event/show_relaylog_events parameters
let $binary_log_name= ;
let $binary_log_limit_row= ;
let $binary_log_limit_offset= ;
