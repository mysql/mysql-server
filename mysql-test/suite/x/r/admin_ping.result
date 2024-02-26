create user abc@localhost identified by 'abc';

command ok

command ok
Got expected error: 'schema' is not a valid field for ping command (code 5021)
Got expected error: Invalid type of arguments, expected object of arguments (code 5016)
Got expected error: Invalid type of arguments, expected object of arguments (code 5016)
Got expected error: Invalid type of arguments, expected object of arguments (code 5016)
Got expected error: Invalid type of arguments, expected object of arguments (code 5016)
Variable_name	Value
Mysqlx_stmt_execute_mysqlx	7
0 rows affected
create and activate new session abc
expect Mysqlx_stmt_execute_mysqlx=1
connecting...
active session is now 'abc'

command ok
Variable_name	Value
Mysqlx_stmt_execute_mysqlx	1
0 rows affected
closing session abc
Mysqlx.Ok {
  msg: "bye!"
}
ok
SHOW STATUS LIKE 'Mysqlx_stmt_execute_mysqlx';
Variable_name	Value
Mysqlx_stmt_execute_mysqlx	8
DROP USER abc@localhost;
