flush status;
TRUNCATE TABLE performance_schema.host_cache;
flush user_resources;
flush privileges;
select @@global.debug;
@@global.debug

select @@global.max_connect_errors;
@@global.max_connect_errors
100
select @@global.max_user_connections;
@@global.max_user_connections
0
select @@global.max_connections;
@@global.max_connections
151
select `User`, `Host` from mysql.`user` where `host` like '%\\%%';
User	Host
select `User`, `Host` from mysql.`user` where `user` like '192.%';
User	Host
select `User`, `Host` from mysql.`user` where `user` like '2001:%';
User	Host
select `User`, `Host` from mysql.`user` where `user` like 'santa.claus.%';
User	Host
show global status like "connection_errors_%";
Variable_name	Value
Connection_errors_accept	0
Connection_errors_internal	0
Connection_errors_max_connections	0
Connection_errors_peer_address	0
Connection_errors_select	0
Connection_errors_tcpwrap	0
select "Con1 is alive";
Con1 is alive
Con1 is alive
select current_user();
current_user()
root@localhost
set global debug= "+d,vio_peer_addr_error";
ERROR HY000: Can't get hostname for your address
show global status like "connection_errors_%";
Variable_name	Value
Connection_errors_accept	0
Connection_errors_internal	0
Connection_errors_max_connections	0
Connection_errors_peer_address	1
Connection_errors_select	0
Connection_errors_tcpwrap	0
"Dumping performance_schema.host_cache"
ERROR HY000: Can't get hostname for your address
show global status like "connection_errors_%";
Variable_name	Value
Connection_errors_accept	0
Connection_errors_internal	0
Connection_errors_max_connections	0
Connection_errors_peer_address	2
Connection_errors_select	0
Connection_errors_tcpwrap	0
"Dumping performance_schema.host_cache"
set global debug= default;
flush status;
show global status like "connection_errors_%";
Variable_name	Value
Connection_errors_accept	0
Connection_errors_internal	0
Connection_errors_max_connections	0
Connection_errors_peer_address	0
Connection_errors_select	0
Connection_errors_tcpwrap	0
