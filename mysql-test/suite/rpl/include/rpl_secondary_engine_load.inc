#####################################################################
#                                                                   #
# The aim of this test is to set up replication between a master    #
# and slave and check that the new ALTER TABLE syntax that has      #
# been introduced in WL#11581 and WL#11581 to support secondary     #
# engines work as expected. A mock engine has been written to test  #
# the statements.                                                   #
#                                                                   #
# Creation Date: 2018-06-21                                         #
# Author: Deepa Dixit                                               #
#                                                                   #
#####################################################################

--source include/master-slave.inc
CALL mtr.add_suppression("\\[ERROR\\] .*MY-\\d+.* \\[Repl\\] Slave SQL for channel '':.*Error 'Unknown storage engine 'MOCK'' on query");
CALL mtr.add_suppression("\\[ERROR\\] .*MY-\\d+.* \\[Repl\\] Slave SQL for channel '':.*Error 'Secondary engine operation failed. No secondary engine defined");
CALL mtr.add_suppression("\\[Warning\\] .*MY-\\d+.* \\[Repl\\] Slave: Unknown storage engine 'MOCK'");
CALL mtr.add_suppression("\\[Warning\\] .*MY-\\d+.* \\[Repl\\] Slave: Secondary engine operation failed. No secondary engine defined");
CALL mtr.add_suppression("\\[Warning\\] .*MY-\\d+.* \\[Server\\] Unsafe statement written to the binary log using statement format");
CALL mtr.add_suppression("The slave coordinator and worker threads are stopped");

--connection master
--echo [ Connection Master ]

# Install plugin on master.
--replace_regex /\.dll/.so/
--eval INSTALL PLUGIN mock SONAME '$MOCK_PLUGIN'
SET @@secondary_engine_cost_threshold = 0;

--connection slave
--echo [ Connection Slave ]

# Install plugin on slave.
--replace_regex /\.dll/.so/
--eval INSTALL PLUGIN mock SONAME '$MOCK_PLUGIN'
SET @@secondary_engine_cost_threshold = 0;

--connection master
--echo [ Connection Master ]

# Create table with SECONDARY_ENGINE MOCK
CREATE TABLE t1 (pk INT PRIMARY KEY AUTO_INCREMENT, vc VARCHAR(10), j JSON, b BLOB, vb VARBINARY(32)) SECONDARY_ENGINE=MOCK;

--source include/sync_slave_sql_with_master.inc
--echo [ Connection Slave ]

# Check if the table is created with the secondary engine.
SHOW CREATE TABLE t1;

# Since the table is not loaded into the MOCK engine, the query is not
# offloaded.
FLUSH STATUS;
SELECT * FROM t1;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';

--connection master
--echo [ Connection Master ]

# Insert rows and offload the table to mock engine.
INSERT INTO t1 VALUES(NULL, 'Poodle', '{"key":"value"}', 'Pug', x'1234');
INSERT INTO t1 VALUES(NULL, 'Labrador', '[{"key":"value"},{"key":"value2"}]', 'Boxer', x'abcd');
INSERT INTO t1 VALUES(NULL, 'Beagle', '[1,2,3]', 'Pitbull', x'abcd1234');
INSERT INTO t1 VALUES(NULL, 'Collie', '["ab","bc","cd"]', 'Huskey', x'1ab2c3d4');
INSERT INTO t1 VALUES(NULL, 'Shiba', '{"1":"one"}', 'Mastiff', x'ffff');

ALTER TABLE t1 SECONDARY_LOAD;

--source include/sync_slave_sql_with_master.inc
--echo [ Connection Slave ]

# Check that SECONDARY_LOAD was not propagated and that the table has not been
# loaded into its secondary engine.
FLUSH STATUS;
SELECT * FROM t1;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';

# Load table also on slave.
ALTER TABLE t1 SECONDARY_LOAD;

# Check that the query is offloaded.
FLUSH STATUS;
SELECT * FROM t1;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';

SET @save_use_secondary_engine = @@use_secondary_engine;

# The query should get executed on the primary engine.
SET use_secondary_engine = OFF;

FLUSH STATUS;
SELECT * FROM t1;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';

# The query should get executed on the secondary engine.
SET use_secondary_engine = FORCED;

FLUSH STATUS;
SELECT * FROM t1;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';

SET use_secondary_engine = @save_use_secondary_engine;

# The query should get executed on secondary engine.
FLUSH STATUS;
SELECT /*+SET_VAR(use_secondary_engine=ON)*/ * FROM t1;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';

# The query should get executed on primary engine.
FLUSH STATUS;
SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ * FROM t1;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';

# The query should get executed on secondary engine.
FLUSH STATUS;
SELECT /*+SET_VAR(use_secondary_engine=FORCED)*/ * FROM t1;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';

--connection master
--echo [ Connection Master ]

# Create a table like t1.
CREATE TABLE t2 LIKE t1;

ANALYZE TABLE t1;
EXPLAIN INSERT INTO t2 SELECT * FROM t1;
EXPLAIN INSERT INTO t2 SELECT * FROM t1 UNION SELECT * FROM t1;
EXPLAIN INSERT INTO t2 SELECT * FROM t1 UNION ALL SELECT * FROM t1;

SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';
CREATE TABLE t3 SELECT * from t1;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';

--disable_warnings
INSERT INTO t2 VALUES(NULL, 'Poodle', '{"key":"value"}', 'Pug', x'1234');
INSERT INTO t2 VALUES(NULL, 'Labrador', '[{"key":"value"},{"key":"value2"}]',
                      'Boxer', x'abcd');
INSERT INTO t2 VALUES(NULL, 'Beagle', '[1,2,3]', 'Pitbull', x'abcd1234');
INSERT INTO t2 VALUES(NULL, 'Collie', '["ab","bc","cd"]', 'Huskey',
                      x'1ab2c3d4');
INSERT INTO t2 VALUES(NULL, 'Shiba', '{"1":"one"}', 'Mastiff', x'ffff');
--enable_warnings

# Offload the table to MOCK engine.
ALTER TABLE t2 SECONDARY_LOAD;

--source include/sync_slave_sql_with_master.inc
--echo [ Connection Slave ]

# Check that t2 is created with the secondary engine.
SHOW CREATE TABLE t2;
SHOW CREATE TABLE t3;

FLUSH STATUS;
SELECT * FROM t2;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';

# Load the table on slave.
ALTER TABLE t2 SECONDARY_LOAD;

FLUSH STATUS;
SELECT * FROM t2;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';

--connection master
--echo [ Connection Master ]

# Query the table on master.
FLUSH STATUS;
SELECT * FROM t2;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';

# Unload the table from the secondary engine.
ALTER TABLE t1 SECONDARY_UNLOAD;
DROP TABLE t3;

--source include/sync_slave_sql_with_master.inc
--echo [ Connection Slave ]

# Check that SECONDARY_UNLOAD was not propagated and that the table is
# still loaded into its secondary engine.
FLUSH STATUS;
SELECT * FROM t1;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';

# Unload table also on slave.
ALTER TABLE t1 SECONDARY_UNLOAD;

# Check that the table is unloaded.
FLUSH STATUS;
SELECT * FROM t1;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';

# Setting use_secondary_engine to FORCED should throw an error since the table is unloaded.
SET use_secondary_engine = FORCED;

FLUSH STATUS;
--error ER_SECONDARY_ENGINE,ER_SECONDARY_ENGINE_PLUGIN
SELECT * FROM t1;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';
# ps-protocol and regular runs give different error codes
--error ER_SECONDARY_ENGINE,ER_SECONDARY_ENGINE_PLUGIN
CREATE TABLE t3 SELECT * from t1;

SET use_secondary_engine = @save_use_secondary_engine;

# Should execute throw an error.
FLUSH STATUS;
--error ER_SECONDARY_ENGINE,ER_SECONDARY_ENGINE_PLUGIN
SELECT /*+SET_VAR(use_secondary_engine=FORCED)*/ * FROM t1;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';

# Set secondary_engine to NULL only on slave.
ALTER TABLE t2 SECONDARY_ENGINE=NULL;

# Stop slave SQL thread.
--source include/stop_slave_sql.inc

--connection master
--echo [ Connection Master ]

# Load the table on master.
ALTER TABLE t2 SECONDARY_LOAD;

--connection slave
--echo [ Connection Slave ]

# Change secondary engine to MOCK
ALTER TABLE t2 SECONDARY_ENGINE=MOCK;

--source include/start_slave_sql.inc
--let $slave_param= Slave_SQL_Running_State
--let $slave_param_value= Replica has read all relay log; waiting for more updates
--source include/wait_for_slave_param.inc

# This should execute on MOCK engine.
FLUSH STATUS;
SELECT * FROM t2;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';

--connection master
--echo [ Connection Master ]

# Change the secondary engine to NULL.
ALTER TABLE t1 SECONDARY_ENGINE=NULL;

--source include/sync_slave_sql_with_master.inc
--echo [ Connection Slave ]

# Check whether the secondary engine has been changed.
SHOW CREATE TABLE t1;
FLUSH STATUS;
SELECT * FROM t1;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';

--connection master
--echo [ Connection Master ]

# Change the secondary engine to something non existent.
ALTER TABLE t1 SECONDARY_ENGINE=HELLO;

--source include/sync_slave_sql_with_master.inc
--echo [ Connection Slave ]

# Check the secondary engine of the table.
SHOW CREATE TABLE t1;

# Check whether the ALTER statements work only on the slave.
ALTER TABLE t1 SECONDARY_ENGINE=NULL;
ALTER TABLE t1 SECONDARY_ENGINE=NADA;
SHOW CREATE TABLE t1;

ALTER TABLE t1 SECONDARY_ENGINE=NULL;
ALTER TABLE t1 SECONDARY_ENGINE=MOCK;
SHOW CREATE TABLE t1;
ALTER TABLE t1 SECONDARY_LOAD;
ALTER TABLE t2 SECONDARY_LOAD;
FLUSH STATUS;
SELECT * FROM t1;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';
ALTER TABLE t1 SECONDARY_UNLOAD;
FLUSH STATUS;
SELECT * FROM t1;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';

ALTER TABLE t2 SECONDARY_UNLOAD;

--connection master
--echo [ Connection Master ]

ALTER TABLE t1 SECONDARY_ENGINE=NULL;
ALTER TABLE t1 SECONDARY_ENGINE=MOCK;

--source include/sync_slave_sql_with_master.inc
--echo [ Connection Slave ]

# Uninstall the plugin on slave.
UNINSTALL PLUGIN mock;

# Stop slave SQL thread.
--source include/stop_slave_sql.inc

--connection master
--echo [ Connection Master ]

# Check if table gets loaded on master even if there is no plugin installed on
# slave.
ALTER TABLE t1 SECONDARY_LOAD;

--connection slave
--echo [ Connection Slave ]

# Install the plugin on slave again
--replace_regex /\.dll/.so/
--eval INSTALL PLUGIN mock SONAME '$MOCK_PLUGIN'

--sleep 3
--source include/start_slave_sql.inc
--let $slave_param= Slave_SQL_Running_State
--let $slave_param_value= Replica has read all relay log; waiting for more updates
--source include/wait_for_slave_param.inc

FLUSH STATUS;
SELECT * FROM t1;
SHOW SESSION STATUS LIKE 'Secondary_engine_execution_count';

--connection master
--echo [ Connection Master ]

# Clean up
DROP TABLE t1;
DROP TABLE t2;
UNINSTALL PLUGIN mock;
SET @@secondary_engine_cost_threshold = DEFAULT;

--source include/sync_slave_sql_with_master.inc

# Uninstall plugin on slave too.
--sleep 3
UNINSTALL PLUGIN mock;
SET @@secondary_engine_cost_threshold = DEFAULT;

--source include/rpl_end.inc
