#
# Bug#28624707: MYSQL TRYING TO OFFLOAD QUERIES TO SECONDARY ENGINE
#               AFTER PLUGIN UNINSTALL
#
SELECT PLUGIN_STATUS FROM INFORMATION_SCHEMA.PLUGINS WHERE PLUGIN_NAME='MOCK';
PLUGIN_STATUS
ACTIVE
CREATE TABLE t(x INT) SECONDARY_ENGINE MOCK;
INSERT INTO t VALUES (1);
ALTER TABLE t SECONDARY_LOAD;
# Should use the secondary engine.
FLUSH STATUS;
SELECT * FROM t;
x
SHOW STATUS LIKE 'Secondary_engine_execution_count';
Variable_name	Value
Secondary_engine_execution_count	1
# The plugin cannot be unloaded because there are still references
# to it (from the table cache). Will give a warning about that.
UNINSTALL PLUGIN mock;
Warnings:
Warning	1620	Plugin is busy and will be uninstalled on shutdown
SELECT PLUGIN_STATUS FROM INFORMATION_SCHEMA.PLUGINS WHERE PLUGIN_NAME='MOCK';
PLUGIN_STATUS
DELETED
# Should not use the secondary engine.
FLUSH STATUS;
SELECT * FROM t;
x
1
SHOW STATUS LIKE 'Secondary_engine_execution_count';
Variable_name	Value
Secondary_engine_execution_count	0
DROP TABLE t;
SELECT PLUGIN_STATUS FROM INFORMATION_SCHEMA.PLUGINS WHERE PLUGIN_NAME='MOCK';
PLUGIN_STATUS
