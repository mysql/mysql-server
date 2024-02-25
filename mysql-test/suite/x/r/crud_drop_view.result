CREATE SCHEMA xtest;
CREATE TABLE xtest.xtable (first INT, second JSON);
CREATE VIEW xtest.xview AS SELECT first FROM xtest.xtable;
SHOW CREATE VIEW xtest.xview;
View	Create View	character_set_client	collation_connection
xview	CREATE ALGORITHM=UNDEFINED DEFINER=`root`@`localhost` SQL SECURITY DEFINER VIEW `xtest`.`xview` AS select `xtest`.`xtable`.`first` AS `first` from `xtest`.`xtable`	utf8mb4	utf8mb4_0900_ai_ci

Drop view (success)
-------------------
Mysqlx.Ok {
}


Drop view (error: view removed)
-------------------------------
Mysqlx.Error {
  severity: ERROR
  code: 1051
  msg: "Unknown table \'xtest.xview\'"
  sql_state: "42S02"
}


Drop view (if exists)
---------------------
Mysqlx.Notice.Frame {
  type: 1
  scope: LOCAL
  payload: "Mysqlx.Notice.Warning { level: NOTE\ncode: 1051\nmsg: \"Unknown table \\\'xtest.xview\\\'\"\n }"
}

Mysqlx.Ok {
}

Variable_name	Value
Mysqlx_crud_drop_view	3
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
include/assert.inc [Global status of 'Mysqlx_crud_drop_view']
