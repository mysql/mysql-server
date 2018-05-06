RUN drop schema if exists xtest

0 rows affected
RUN create schema xtest

1 rows affected
RUN use xtest

0 rows affected
RUN create table mytable (pk int primary key auto_increment, data varchar(100))

0 rows affected
RUN create procedure result_and_error ()
begin
  select 1;
  select * from invalid_table;
end

0 rows affected
send Mysqlx.Sql.StmtExecute {
  stmt: "insert into mytable values (DEFAULT, \'one\')"
}


1 rows affected
last insert id: 1
send Mysqlx.Sql.StmtExecute {
  stmt: "insert into mytable values (DEFAULT, \'two\')"
}


1 rows affected
last insert id: 2
send Mysqlx.Sql.StmtExecute {
  stmt: "insert into mytable values (DEFAULT, \'three\')"
}


1 rows affected
last insert id: 3
send Mysqlx.Sql.StmtExecute {
  stmt: "select * from mytable"
}

Mysqlx.Resultset.ColumnMetaData {
  type: SINT
  name: "pk"
  original_name: "pk"
  table: "mytable"
  original_table: "mytable"
  schema: "xtest"
  catalog: "def"
  length: 11
  flags: 304
}

Mysqlx.Resultset.ColumnMetaData {
  type: BYTES
  name: "data"
  original_name: "data"
  table: "mytable"
  original_table: "mytable"
  schema: "xtest"
  catalog: "def"
  collation: 255
  length: 400
}

Mysqlx.Resultset.Row {
  field: "\002"
  field: "one\000"
}

Mysqlx.Resultset.Row {
  field: "\004"
  field: "two\000"
}

Mysqlx.Resultset.Row {
  field: "\006"
  field: "three\000"
}

Mysqlx.Resultset.FetchDone {
}

Mysqlx.Notice.Frame {
  type: 3
  scope: LOCAL
  payload: "Mysqlx.Notice.SessionStateChanged { param: ROWS_AFFECTED\nvalue {\n  type: V_UINT\n  v_unsigned_int: 0\n}\n }"
}

Mysqlx.Sql.StmtExecuteOk {
}

RUN drop schema if exists xtest

1 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
