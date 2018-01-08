RUN SHOW SCHEMAS

Database
information_schema
mtr
mysql
performance_schema
sys
test
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
# Syncing with status [ 'Mysqlx_connections_closed' == 1 ]
SHOW GLOBAL STATUS LIKE 'mysqlx%connections%';
Variable_name	Value
Mysqlx_connections_accepted	1
Mysqlx_connections_closed	1
Mysqlx_connections_rejected	0
INSTALL COMPONENT "file://component_mysqlx_global_reset";
SELECT mysqlx_reset_global_status_variables();
mysqlx_reset_global_status_variables()
1
SHOW GLOBAL STATUS LIKE 'mysqlx%connections%';
Variable_name	Value
Mysqlx_connections_accepted	0
Mysqlx_connections_closed	0
Mysqlx_connections_rejected	0
RUN SELECT mysqlx_reset_global_status_variables()

mysqlx_reset_global_status_variables()
0
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
UNINSTALL COMPONENT "file://component_mysqlx_global_reset";
