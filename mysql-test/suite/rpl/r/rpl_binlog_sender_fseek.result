include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
include/stop_dump_threads.inc
CREATE TABLE t1 (c1 TEXT(8192));
Inserting a reference transaction to calculate the content size
Filling binary log up to the border of the 8192 block
include/assert.inc [The binary log is at the expected border of a 8192 block]
Filling the binary log 100 transactions
TRUNCATE performance_schema.file_summary_by_event_name;
[connection slave]
include/start_slave_io.inc
[connection master]
include/sync_slave_io_with_master.inc
[connection master]
include/assert.inc [COUNT_MISC for "wait/io/file/sql/binlog" should be minimal]
[connection slave]
include/start_slave_sql.inc
[connection master]
DROP TABLE t1;
include/rpl_end.inc
