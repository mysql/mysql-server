CREATE SCHEMA xtest;
RUN USE xtest

0 rows affected
RUN CREATE TABLE test_float (f FLOAT, d DOUBLE)

0 rows affected
RUN INSERT INTO test_float VALUES (1.0/81, 1.0/81)

1 rows affected
RUN INSERT INTO test_float VALUES (1234567890.1234567890,1234567890.1234567890)

1 rows affected
RUN SELECT * FROM test_float
f	d
0.0123457	0.012345679
1234570000	1234567890.1234567
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
RUN USE xtest

0 rows affected
RUN CREATE TABLE test_unsigned (i INT UNSIGNED, b BIGINT UNSIGNED, t TINYINT UNSIGNED, s SMALLINT UNSIGNED, f FLOAT UNSIGNED, d DOUBLE UNSIGNED, de DECIMAL(10,2) UNSIGNED, dt DATETIME, ts TIMESTAMP)

0 rows affected
send Mysqlx.Sql.StmtExecute {
  stmt: "SELECT * FROM test_unsigned"
}

Mysqlx.Resultset.ColumnMetaData {
  type: UINT
  name: "i"
  original_name: "i"
  table: "test_unsigned"
  original_table: "test_unsigned"
  schema: "xtest"
  catalog: "def"
  length: 10
}

Mysqlx.Resultset.ColumnMetaData {
  type: UINT
  name: "b"
  original_name: "b"
  table: "test_unsigned"
  original_table: "test_unsigned"
  schema: "xtest"
  catalog: "def"
  length: 20
}

Mysqlx.Resultset.ColumnMetaData {
  type: UINT
  name: "t"
  original_name: "t"
  table: "test_unsigned"
  original_table: "test_unsigned"
  schema: "xtest"
  catalog: "def"
  length: 3
}

Mysqlx.Resultset.ColumnMetaData {
  type: UINT
  name: "s"
  original_name: "s"
  table: "test_unsigned"
  original_table: "test_unsigned"
  schema: "xtest"
  catalog: "def"
  length: 5
}

Mysqlx.Resultset.ColumnMetaData {
  type: FLOAT
  name: "f"
  original_name: "f"
  table: "test_unsigned"
  original_table: "test_unsigned"
  schema: "xtest"
  catalog: "def"
  fractional_digits: 31
  length: 12
  flags: 1
}

Mysqlx.Resultset.ColumnMetaData {
  type: DOUBLE
  name: "d"
  original_name: "d"
  table: "test_unsigned"
  original_table: "test_unsigned"
  schema: "xtest"
  catalog: "def"
  fractional_digits: 31
  length: 22
  flags: 1
}

Mysqlx.Resultset.ColumnMetaData {
  type: DECIMAL
  name: "de"
  original_name: "de"
  table: "test_unsigned"
  original_table: "test_unsigned"
  schema: "xtest"
  catalog: "def"
  fractional_digits: 2
  length: 11
  flags: 1
}

Mysqlx.Resultset.ColumnMetaData {
  type: DATETIME
  name: "dt"
  original_name: "dt"
  table: "test_unsigned"
  original_table: "test_unsigned"
  schema: "xtest"
  catalog: "def"
  length: 19
  content_type: 2
}

Mysqlx.Resultset.ColumnMetaData {
  type: DATETIME
  name: "ts"
  original_name: "ts"
  table: "test_unsigned"
  original_table: "test_unsigned"
  schema: "xtest"
  catalog: "def"
  length: 19
  flags: 1
  content_type: 2
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

Mysqlx.Ok {
  msg: "bye!"
}
ok
RUN USE xtest

0 rows affected
RUN CREATE TABLE test_set (s SET('one','two','three','four','five','six','seven','eight','nine','ten','eleven','twelve','thirteen','fourteen','fifteen','sixteen','seventeen','eightteen','nineteen','twenty','thirty','forty','fifty','sixty','seventy','eightty','ninety'))

0 rows affected
RUN INSERT INTO test_set VALUES ('one,two')

1 rows affected
RUN INSERT INTO test_set VALUES ('one,two,three,four,five,six,seven,eight,nine,ten,eleven,twelve,thirteen,fourteen,fifteen,sixteen,seventeen,eightteen,nineteen')

1 rows affected
RUN INSERT INTO test_set VALUES ('one,two,three,four,five,six,seven,eight,nine,ten,eleven,twelve,thirteen,fourteen,fifteen,sixteen,seventeen,eightteen,nineteen,twenty,thirty,forty,fifty,sixty,seventy,eightty,ninety')

1 rows affected
RUN SELECT * FROM test_set
s
one,two
eight,eightteen,eleven,fifteen,five,four,fourteen,nine,nineteen,one,seven,seventeen,six,sixteen,ten,thirteen,three,twelve,two
eight,eightteen,eightty,eleven,fifteen,fifty,five,forty,four,fourteen,nine,nineteen,ninety,one,seven,seventeen,seventy,six,sixteen,sixty,ten,thirteen,thirty,three,twelve,twenty,two
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
