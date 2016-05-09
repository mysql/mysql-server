include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
# Initial setup
[connection master]
CREATE TABLE t1 (c1 INT) ENGINE=InnoDB;
CREATE TEMPORARY TABLE temp_t1 (c1 INT) ENGINE=InnoDB;
# Insert the data in the master
BEGIN;
INSERT INTO temp_t1 VALUES (1);
COMMIT;
BEGIN;
INSERT INTO t1 VALUES ((SELECT COUNT(*) FROM temp_t1));
COMMIT;
include/sync_slave_sql_with_master.inc
[connection slave]
# Stop the IO thread
include/stop_slave_io.inc
Warnings:
Warning	3022	This operation may not be safe when the slave has temporary tables. The tables will be kept open until the server restarts or until the tables are deleted by any replicated DROP statement. Suggest to wait until slave_open_temp_tables = 0.
# Restart IO thread
include/start_slave_io.inc
# Do one more insert on master and then sync slave with master
[connection master]
BEGIN;
INSERT INTO t1 VALUES ((SELECT COUNT(*) FROM temp_t1));
COMMIT;
include/sync_slave_sql_with_master.inc
# Now compare master and slave's t1 table data
[connection master]
include/diff_tables.inc [master:t1, slave:t1]
# Cleanup
[connection master]
DROP TABLE t1, temp_t1;
include/rpl_end.inc
