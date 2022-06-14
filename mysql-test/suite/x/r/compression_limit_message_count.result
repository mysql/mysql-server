CREATE TABLE test_table(data TEXT);
INSERT INTO test_table(data) VALUES
("my first row"),
("my second row"),
("my third row"),
("my fourth row"),
("my fifth row"),
("my sixth row"),
("my seventh row"),
("my eighth row"),
("my ninth row"),
( "my tenth row");

## I. Execute the test using: ZSTD_STREAM + max_combine=1
#


## Execute same query two times, which should return resultset of 10 rows
#   1. Observe that the resultset if its correctly uncompressed
#   2. Observe that the resultset if its correctly encapsulated
#      into compression messages
connecting...
active session is now 'SomeSession'
RUN recvok
Login OK

#
# 1
RUN SELECT * FROM test.test_table
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# 2
RUN SELECT * FROM test.test_table
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
closing session SomeSession
Mysqlx.Ok {
  msg: "bye!"
}
ok

## II. Execute the test using: ZSTD_STREAM + max_combine=2
#


## Execute same query two times, which should return resultset of 10 rows
#   1. Observe that the resultset if its correctly uncompressed
#   2. Observe that the resultset if its correctly encapsulated
#      into compression messages
connecting...
active session is now 'SomeSession'
RUN recvok
Login OK

#
# 1
RUN SELECT * FROM test.test_table
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# 2
RUN SELECT * FROM test.test_table
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
closing session SomeSession
Mysqlx.Ok {
  msg: "bye!"
}
ok

## III. Execute the test using: ZSTD_STREAM + max_combine=3
#


## Execute same query two times, which should return resultset of 10 rows
#   1. Observe that the resultset if its correctly uncompressed
#   2. Observe that the resultset if its correctly encapsulated
#      into compression messages
connecting...
active session is now 'SomeSession'
RUN recvok
Login OK

#
# 1
RUN SELECT * FROM test.test_table
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# 2
RUN SELECT * FROM test.test_table
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
closing session SomeSession
Mysqlx.Ok {
  msg: "bye!"
}
ok

## IV. Execute the test using: ZSTD_STREAM + max_combine=6
#


## Execute same query two times, which should return resultset of 10 rows
#   1. Observe that the resultset if its correctly uncompressed
#   2. Observe that the resultset if its correctly encapsulated
#      into compression messages
connecting...
active session is now 'SomeSession'
RUN recvok
Login OK

#
# 1
RUN SELECT * FROM test.test_table
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# 2
RUN SELECT * FROM test.test_table
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
closing session SomeSession
Mysqlx.Ok {
  msg: "bye!"
}
ok

## V. Execute the test using: ZSTD_STREAM + max_combine=1000000
#


## Execute same query two times, which should return resultset of 10 rows
#   1. Observe that the resultset if its correctly uncompressed
#   2. Observe that the resultset if its correctly encapsulated
#      into compression messages
connecting...
active session is now 'SomeSession'
RUN recvok
Login OK

#
# 1
RUN SELECT * FROM test.test_table
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# 2
RUN SELECT * FROM test.test_table
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
closing session SomeSession
Mysqlx.Ok {
  msg: "bye!"
}
ok

## VI. Execute the test using: ZSTD_STREAM + max_combine=9223372036854775807
#


## Execute same query two times, which should return resultset of 10 rows
#   1. Observe that the resultset if its correctly uncompressed
#   2. Observe that the resultset if its correctly encapsulated
#      into compression messages
connecting...
active session is now 'SomeSession'
RUN recvok
Login OK

#
# 1
RUN SELECT * FROM test.test_table
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# 2
RUN SELECT * FROM test.test_table
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
closing session SomeSession
Mysqlx.Ok {
  msg: "bye!"
}
ok

## I. Execute the test using: LZ4_MESSAGE + max_combine=1
#


## Execute same query two times, which should return resultset of 10 rows
#   1. Observe that the resultset if its correctly uncompressed
#   2. Observe that the resultset if its correctly encapsulated
#      into compression messages
connecting...
active session is now 'SomeSession'
RUN recvok
Login OK

#
# 1
RUN SELECT * FROM test.test_table
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# 2
RUN SELECT * FROM test.test_table
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
closing session SomeSession
Mysqlx.Ok {
  msg: "bye!"
}
ok

## II. Execute the test using: LZ4_MESSAGE + max_combine=2
#


## Execute same query two times, which should return resultset of 10 rows
#   1. Observe that the resultset if its correctly uncompressed
#   2. Observe that the resultset if its correctly encapsulated
#      into compression messages
connecting...
active session is now 'SomeSession'
RUN recvok
Login OK

#
# 1
RUN SELECT * FROM test.test_table
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# 2
RUN SELECT * FROM test.test_table
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
closing session SomeSession
Mysqlx.Ok {
  msg: "bye!"
}
ok

## III. Execute the test using: LZ4_MESSAGE + max_combine=3
#


## Execute same query two times, which should return resultset of 10 rows
#   1. Observe that the resultset if its correctly uncompressed
#   2. Observe that the resultset if its correctly encapsulated
#      into compression messages
connecting...
active session is now 'SomeSession'
RUN recvok
Login OK

#
# 1
RUN SELECT * FROM test.test_table
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# 2
RUN SELECT * FROM test.test_table
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
closing session SomeSession
Mysqlx.Ok {
  msg: "bye!"
}
ok

## IV. Execute the test using: LZ4_MESSAGE + max_combine=6
#


## Execute same query two times, which should return resultset of 10 rows
#   1. Observe that the resultset if its correctly uncompressed
#   2. Observe that the resultset if its correctly encapsulated
#      into compression messages
connecting...
active session is now 'SomeSession'
RUN recvok
Login OK

#
# 1
RUN SELECT * FROM test.test_table
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# 2
RUN SELECT * FROM test.test_table
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
closing session SomeSession
Mysqlx.Ok {
  msg: "bye!"
}
ok

## V. Execute the test using: LZ4_MESSAGE + max_combine=1000000
#


## Execute same query two times, which should return resultset of 10 rows
#   1. Observe that the resultset if its correctly uncompressed
#   2. Observe that the resultset if its correctly encapsulated
#      into compression messages
connecting...
active session is now 'SomeSession'
RUN recvok
Login OK

#
# 1
RUN SELECT * FROM test.test_table
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# 2
RUN SELECT * FROM test.test_table
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
closing session SomeSession
Mysqlx.Ok {
  msg: "bye!"
}
ok

## VI. Execute the test using: LZ4_MESSAGE + max_combine=9223372036854775807
#


## Execute same query two times, which should return resultset of 10 rows
#   1. Observe that the resultset if its correctly uncompressed
#   2. Observe that the resultset if its correctly encapsulated
#      into compression messages
connecting...
active session is now 'SomeSession'
RUN recvok
Login OK

#
# 1
RUN SELECT * FROM test.test_table
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# 2
RUN SELECT * FROM test.test_table
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
closing session SomeSession
Mysqlx.Ok {
  msg: "bye!"
}
ok

## I. Execute the test using: DEFLATE_STREAM + max_combine=1
#


## Execute same query two times, which should return resultset of 10 rows
#   1. Observe that the resultset if its correctly uncompressed
#   2. Observe that the resultset if its correctly encapsulated
#      into compression messages
connecting...
active session is now 'SomeSession'
RUN recvok
Login OK

#
# 1
RUN SELECT * FROM test.test_table
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# 2
RUN SELECT * FROM test.test_table
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
closing session SomeSession
Mysqlx.Ok {
  msg: "bye!"
}
ok

## II. Execute the test using: DEFLATE_STREAM + max_combine=2
#


## Execute same query two times, which should return resultset of 10 rows
#   1. Observe that the resultset if its correctly uncompressed
#   2. Observe that the resultset if its correctly encapsulated
#      into compression messages
connecting...
active session is now 'SomeSession'
RUN recvok
Login OK

#
# 1
RUN SELECT * FROM test.test_table
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# 2
RUN SELECT * FROM test.test_table
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
closing session SomeSession
Mysqlx.Ok {
  msg: "bye!"
}
ok

## III. Execute the test using: DEFLATE_STREAM + max_combine=3
#


## Execute same query two times, which should return resultset of 10 rows
#   1. Observe that the resultset if its correctly uncompressed
#   2. Observe that the resultset if its correctly encapsulated
#      into compression messages
connecting...
active session is now 'SomeSession'
RUN recvok
Login OK

#
# 1
RUN SELECT * FROM test.test_table
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# 2
RUN SELECT * FROM test.test_table
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
closing session SomeSession
Mysqlx.Ok {
  msg: "bye!"
}
ok

## IV. Execute the test using: DEFLATE_STREAM + max_combine=6
#


## Execute same query two times, which should return resultset of 10 rows
#   1. Observe that the resultset if its correctly uncompressed
#   2. Observe that the resultset if its correctly encapsulated
#      into compression messages
connecting...
active session is now 'SomeSession'
RUN recvok
Login OK

#
# 1
RUN SELECT * FROM test.test_table
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# 2
RUN SELECT * FROM test.test_table
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
closing session SomeSession
Mysqlx.Ok {
  msg: "bye!"
}
ok

## V. Execute the test using: DEFLATE_STREAM + max_combine=1000000
#


## Execute same query two times, which should return resultset of 10 rows
#   1. Observe that the resultset if its correctly uncompressed
#   2. Observe that the resultset if its correctly encapsulated
#      into compression messages
connecting...
active session is now 'SomeSession'
RUN recvok
Login OK

#
# 1
RUN SELECT * FROM test.test_table
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# 2
RUN SELECT * FROM test.test_table
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
closing session SomeSession
Mysqlx.Ok {
  msg: "bye!"
}
ok

## VI. Execute the test using: DEFLATE_STREAM + max_combine=9223372036854775807
#


## Execute same query two times, which should return resultset of 10 rows
#   1. Observe that the resultset if its correctly uncompressed
#   2. Observe that the resultset if its correctly encapsulated
#      into compression messages
connecting...
active session is now 'SomeSession'
RUN recvok
Login OK

#
# 1
RUN SELECT * FROM test.test_table
[ASSERT Mysqlx.Resultset.ColumnMetaData]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.Row]
[ASSERT Mysqlx.Resultset.FetchDone]
[ASSERT notice session state change - ROWS_AFFECTED, v_unsigned_int:0]
[ASSERT Mysqlx.Sql.StmtExecuteOk]

#
# 2
RUN SELECT * FROM test.test_table
[ASSERT COMPRESSION]
[ASSERT Mysqlx.Sql.StmtExecuteOk]
closing session SomeSession
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP TABLE test_table;
