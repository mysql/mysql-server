
Basic reset scenario
--------------------
send Mysqlx.Session.Reset {
  keep_open: true
}

RUN recvok

Check the pipelining
--------------------
RUN select 2.1 as value
RUN select 2.2 as value
send Mysqlx.Session.Reset {
  keep_open: true
}

value
2.1
0 rows affected
value
2.2
0 rows affected
RUN recvok

Reset session variable
----------------------
RUN set lc_messages= pl_PL

0 rows affected
send Mysqlx.Session.Reset {
  keep_open: true
}

RUN recvok
RUN SELECT IF(@@lc_messages NOT LIKE 'pl_PL', 'OK', 'FAIL') AS result;;
Verify ["SELECT IF(@@lc_messages NOT LIKE 'pl_PL', 'OK', 'FAIL') AS result;" returns in "result" column, following value OK]

Reset user variable
-------------------
RUN set @secret = 12345

0 rows affected
send Mysqlx.Session.Reset {
  keep_open: true
}

RUN recvok
RUN SELECT IF(@secret IS NULL, 'OK', 'FAIL') AS result;;
Verify ["SELECT IF(@secret IS NULL, 'OK', 'FAIL') AS result;" returns in "result" column, following value OK]

Reset prepare statments
-----------------------
send Mysqlx.Prepare.Prepare {
  stmt_id: 123
  stmt {
    type: STMT
    stmt_execute {
      stmt: "select 2.3"
    }
  }
}

RUN recvok
RUN SHOW STATUS LIKE 'Prepared_stmt_count';
Verify [Status variable "Prepared_stmt_count" needs to have a value of 1]
send Mysqlx.Session.Reset {
  keep_open: true
}

RUN recvok
send Mysqlx.Prepare.Execute {
  stmt_id: 123
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5110
  msg: "Statement with ID=123 was not prepared"
  sql_state: "HY000"
}

RUN SHOW STATUS LIKE 'Prepared_stmt_count';
Verify [Status variable "Prepared_stmt_count" needs to have a value of 0]

Reset table lock
----------------
RUN CREATE TABLE test.example1(value INT)

0 rows affected
RUN CREATE TABLE test.example2(value INT)

0 rows affected
RUN LOCK TABLES test.example2 READ

0 rows affected
RUN SELECT COUNT(*) FROM test.example2
COUNT(*)
0
0 rows affected
RUN SELECT COUNT(*) FROM test.example1;
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 1100
  msg: "Table \'example1\' was not locked with LOCK TABLES"
  sql_state: "HY000"
}

send Mysqlx.Session.Reset {
  keep_open: true
}

RUN recvok
RUN SELECT COUNT(*) FROM test.example1
COUNT(*)
0
0 rows affected
RUN SELECT COUNT(*) FROM test.example2
COUNT(*)
0
0 rows affected
RUN DROP TABLE test.example1

0 rows affected
RUN DROP TABLE test.example2

0 rows affected

Expect block is not reset
-------------------------
send Mysqlx.Expect.Open {
  cond {
    condition_key: 1
  }
}

RUN recvok
send Mysqlx.Session.Reset {
  keep_open: true
}

RUN recvok
send Mysqlx.Expect.Close {
}

RUN recvok

Reset global variable
---------------------
RUN set global max_connections = 1234

0 rows affected
send Mysqlx.Session.Reset {
  keep_open: true
}

RUN recvok
RUN SELECT IF(@@global.max_connections = 1234, 'OK', 'FAIL') AS result;;
Verify ["SELECT IF(@@global.max_connections = 1234, 'OK', 'FAIL') AS result;" returns in "result" column, following value OK]
Mysqlx.Ok {
  msg: "bye!"
}
ok
set @@global.max_connections = default;
