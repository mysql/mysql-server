# Restart server with keyring_file plugin
# restart:--early-plugin-load=keyring_file.so --loose-keyring_file_data=MYSQL_TMP_DIR/keyring/perf_schema
CREATE DATABASE WL_11543_PERF_SCHEMA_KEYRING;
USE WL_11543_PERF_SCHEMA_KEYRING;
SELECT count(*) FROM performance_schema.keyring_keys;
count(*)
0
CREATE TABLE t1(c1 INT, c2 char(20)) ENGINE = InnoDB ENCRYPTION="Y";
SELECT count(*) FROM performance_schema.keyring_keys;
count(*)
1
ALTER INSTANCE ROTATE INNODB MASTER KEY;
SELECT count(*) FROM performance_schema.keyring_keys;
count(*)
2
SELECT * FROM performance_schema.keyring_keys;
KEY_ID	KEY_OWNER	BACKEND_KEY_ID
SOME_KEY_ID		
SOME_KEY_ID		
# cleanup
DROP TABLE t1;
UNINSTALL PLUGIN keyring_file;
DROP DATABASE WL_11543_PERF_SCHEMA_KEYRING;
# restart:
