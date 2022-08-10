include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
create temporary table t1 (n int);
insert into t1 values(1);
include/sync_slave_sql_with_master.inc
show status like 'Replica_open_temp_tables';
Variable_name	Value
Replica_open_temp_tables	1
delete from t1;
include/sync_slave_sql_with_master.inc
show status like 'Replica_open_temp_tables';
Variable_name	Value
Replica_open_temp_tables	1
truncate t1;
include/sync_slave_sql_with_master.inc
show status like 'Replica_open_temp_tables';
Variable_name	Value
Replica_open_temp_tables	1
show status like 'Replica_open_temp_tables';
Variable_name	Value
Replica_open_temp_tables	0
include/rpl_end.inc
