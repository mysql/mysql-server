include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
set global max_connections=151;
include/stop_slave.inc
include/start_slave.inc
include/sync_slave_sql_with_master.inc
show variables like 'init_replica';
Variable_name	Value
init_replica	set global max_connections=500
show variables like 'max_connections';
Variable_name	Value
max_connections	500
reset master;
show variables like 'init_replica';
Variable_name	Value
init_replica	
show variables like 'max_connections';
Variable_name	Value
max_connections	151
include/sync_slave_sql_with_master.inc
set @my_global_init_connect= @@global.init_connect;
set global init_connect="set @c=1";
show variables like 'init_connect';
Variable_name	Value
init_connect	set @c=1
include/sync_slave_sql_with_master.inc
set global init_connect= @my_global_init_connect;
set global max_connections= default;
include/rpl_end.inc
