include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection master]
DROP TABLE IF EXISTS t1;
include/sync_slave_sql_with_master.inc
call mtr.add_suppression("The slave coordinator and worker threads are stopped");
call mtr.add_suppression("Query caused different errors on master and slave");
SET @@global.debug="+d,simulate_error_in_ddl";
[connection master]
CREATE TABLE t1(a int);
[connection slave]
include/wait_for_slave_sql_error.inc [errno=13113]
DROP TABLE t1;
SET @@global.debug="-d,simulate_error_in_ddl";
include/start_slave_sql.inc
[connection master]
DROP TABLE t1;
include/rpl_end.inc
