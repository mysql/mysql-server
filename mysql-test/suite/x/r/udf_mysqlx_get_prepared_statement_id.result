include/assert.inc [Validation of MYSQLX_GET_PREPARED_STATEMENT_ID(1)]
include/assert.inc [Validation of MYSQLX_GET_PREPARED_STATEMENT_ID(2)]
RUN recvok
RUN recvok
RUN recvok
RUN recvok
RUN recvok
RUN recvok
Verify ["SELECT MYSQLX_GET_PREPARED_STATEMENT_ID(3) as Value" returns in "Value" column, following value null]
Verify ["SELECT MYSQLX_GET_PREPARED_STATEMENT_ID(4) as Value" returns in "Value" column, following value 1]

0 rows affected
Got expected error: No database selected (code 1046)

0 rows affected
Got expected error: PROCEDURE test.MYSQLX_GET_PREPARED_STATEMENT_ID does not exist (code 1305)
Got expected error: Can't initialize function 'mysqlx_get_prepared_statement_id'; Function expect only one numeric argument (code 1123)
Got expected error: Can't initialize function 'mysqlx_get_prepared_statement_id'; Function expect only one numeric argument (code 1123)
Got expected error: Can't initialize function 'mysqlx_get_prepared_statement_id'; Function expect only one numeric argument (code 1123)
Got expected error: Can't initialize function 'mysqlx_get_prepared_statement_id'; Function expect only one numeric argument (code 1123)

0 rows affected
begin
begin
MYSQLX_GET_PREPARED_STATEMENT_ID(id)
6
end
end

0 rows affected

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
