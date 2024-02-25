## Script test_session_reset.inc
#
# Extracted queries form 'session_reset.test' that
# it can be executed multiple times, with different
# mysqlxtest configuration.
#
# Usage:
#
# [--let $param_mysqlxtest="cmd line args"|]
# --source test_session_reset.inc
#


## Test starts here
--write_file $MYSQL_TMP_DIR/session_reset.xpl
-->import mysqlx_session.macro

-->title =Session reset (%SESSION_CLOSE_SCENARIO%)

#####Basic scenario:

-->stmtadmin list_clients
-->recvtovar %EXPECT_SESSION_ID% sql_session
-->stmtadmin list_clients
-->recvtovar %EXPECT_CLIENT_ID% client_id


#1) Reset session
-->callmacro %SESSION_CLOSE_SCENARIO%
-->recvok


#2) Log back
-->login x_root

#3) Execute query
Mysqlx.Sql.StmtExecute {
    stmt: "select 1.1"
}
-->recvresult

#4) Check sql session id change
-->stmtadmin list_clients
-->recvtovar %CURRENT_SESSION_ID% sql_session
-->assert_ne %EXPECT_SESSION_ID%	%CURRENT_SESSION_ID%

#5) Check client id not change
-->stmtadmin list_clients
-->recvtovar %CURRENT_CLIENT_ID% client_id
-->assert_eq %EXPECT_CLIENT_ID%	%CURRENT_CLIENT_ID%

##### Check the pipelining:
#1) Pipeline some queries
Mysqlx.Sql.StmtExecute {
  stmt: "select 2.1"
}

Mysqlx.Sql.StmtExecute {
    stmt: "select 2.2"
}


#2) Reset session
-->callmacro %SESSION_CLOSE_SCENARIO%


#3) Receive messages from queries preceeding Reset
-->recvresult
-->recvresult
-->recvok

-->login x_root


######## Expect error and server disconnect trying to execute query after the session reset
-->callmacro %SESSION_CLOSE_SCENARIO%
-->recvok

Mysqlx.Sql.StmtExecute {
    stmt: "select 3.1"
}
-->recv


##### Ensure that a session after reset is not the same session as from before the reset
-->newsession foo

-->sql
set @secret = 12345;
-->endsql

-->callmacro %SESSION_CLOSE_SCENARIO%
-->recvok

-->login x_root

-->sql
select IF(@secret is NULL, 'OK', 'FAIL');
-->endsql
-->closesession


##### Ensure that a global variable after reset is the same as before the reset
-->newsession foobar

-->sql
set global max_connections = 1234
-->endsql

-->callmacro %SESSION_CLOSE_SCENARIO%
-->recvok

-->login x_root

-->sql
select IF(@@global.max_connections = 1234, 'OK', 'FAIL');
set @@global.max_connections = default;
-->endsql
-->closesession

### Ensure that a session expect block is reset
#TODO: this is not expected behavior; need to be fix

-->newsession bar
Mysqlx.Expect.Open {
    cond {
        condition_key: 1
    }
}
-->recvok

-->callmacro %SESSION_CLOSE_SCENARIO%
-->recvok

-->login x_root
Mysqlx.Expect.Close {
}
-->recverror ER_X_EXPECT_NOT_OPEN

EOF

--write_file $MYSQL_TMP_DIR/session_reset_single_frame.xpl
-->macro Session_reset_empty
-->binparse %RST% Mysqlx.Session.Reset {
}
-->endmacro

-->macro Session_reset
-->binparse %RST% Mysqlx.Session.Reset {
  keep_open:false
}
-->endmacro

-->macro Session_close
-->binparse %RST% Mysqlx.Session.Close {
}
-->endmacro

-->title =Session reset single frame (%SESSION_CLOSE_SCENARIO%)

-->binparse %SEL1% Mysqlx.Sql.StmtExecute {
  stmt: "select 2.1"
}

-->binparse %SEL2% Mysqlx.Sql.StmtExecute {
    stmt: "select 2.2"
}

-->callmacro %SESSION_CLOSE_SCENARIO%

-->binsend %SEL1%%RST%%SEL2%

-->recvresult
-->recv
-->recv
-->closesession abort
EOF


--write_file $MYSQL_TMP_DIR/session_reset_prepared_stmt.xpl
-->title =Session reset prepared stmt (%SESSION_CLOSE_SCENARIO%)
-->import assert_variable.macro
-->import mysqlx_session.macro


## WL#9270 - ensure that session reset deallocates MySQL Servers
##           prepared statements allocated by Mysqlx.Prepare messages

Mysqlx.Prepare.Prepare {
  stmt_id: 2
  stmt: {
    type: STMT
    stmt_execute: {
      stmt: "select 2"
    }
  }
}
-->recvok

Mysqlx.Cursor.Open {
  cursor_id: 3
  stmt: {
    type: PREPARE_EXECUTE
    prepare_execute {
      stmt_id: 2
    }
  }
}
-->recvresult

-->callmacro Assert_status_variable	Prepared_stmt_count	1

-->callmacro %SESSION_CLOSE_SCENARIO%
-->recvok

-->login x_root

-->callmacro Assert_status_variable	Prepared_stmt_count	0

Mysqlx.Prepare.Execute {
  stmt_id: 2
}
-->expecterror ER_X_BAD_STATEMENT_ID
-->recvresult

Mysqlx.Cursor.Fetch {
  cursor_id: 3
}
-->expecterror ER_X_BAD_CURSOR_ID
-->recvresult

EOF

echo ## 1. Execute Reset using basic scenarios;
echo #    a. verify that reset without args removes current session;
echo #    b. verify that reset with keep_open=false removes current session;
echo #    c. verify that close removes current session;
echo ## 2. Execute Reset with pipelined stmt-execute;
echo #    a. verify that reset without args removes current session;
echo #    b. verify that reset with keep_open=false removes current session;
echo #    c. verify that close removes current session;
echo ## 3. Execute Reset and check that prepared-stmts are removed;
echo #    a. verify that reset without args removes current session and prepared-statemtents;
echo #    b. verify that reset with keep_open=false removes current session and prepared-statemtents;
echo #    c. verify that close removes current session and prepared-statemtents;



--echo
--echo ## 1.a
--echo #
exec $MYSQLXTEST $PARAM $param_mysqlxtest
  -ux_root --password='' -v%SESSION_CLOSE_SCENARIO%=Session_reset_empty
  --file=$MYSQL_TMP_DIR/session_reset.xpl 2>&1;
--echo
--echo ## 1.b
--echo #
exec $MYSQLXTEST $param_mysqlxtest
  -ux_root --password='' -v%SESSION_CLOSE_SCENARIO%=Session_reset
  --file=$MYSQL_TMP_DIR/session_reset.xpl 2>&1;
--echo
--echo ## 1.c
--echo #
exec $MYSQLXTEST $param_mysqlxtest
  -ux_root --password='' -v%SESSION_CLOSE_SCENARIO%=Session_close
  --file=$MYSQL_TMP_DIR/session_reset.xpl 2>&1;


--echo
--echo ## 2.a
--echo #
exec $MYSQLXTEST $param_mysqlxtest
  -ux_root --password='' -v%SESSION_CLOSE_SCENARIO%=Session_reset_empty
  --file=$MYSQL_TMP_DIR/session_reset_single_frame.xpl 2>&1;
--echo
--echo ## 2.b
--echo #
exec $MYSQLXTEST $param_mysqlxtest
  -ux_root --password='' -v%SESSION_CLOSE_SCENARIO%=Session_reset
  --file=$MYSQL_TMP_DIR/session_reset_single_frame.xpl 2>&1;
--echo
--echo ## 2.c
--echo #
exec $MYSQLXTEST $param_mysqlxtest
  -ux_root --password='' -v%SESSION_CLOSE_SCENARIO%=Session_close
  --file=$MYSQL_TMP_DIR/session_reset_single_frame.xpl 2>&1;


--echo
--echo ## 3.a
--echo #
exec $MYSQLXTEST $param_mysqlxtest
  -ux_root --password='' -v%SESSION_CLOSE_SCENARIO%=Session_reset_empty
  --file=$MYSQL_TMP_DIR/session_reset_prepared_stmt.xpl;
--echo
--echo ## 3.b
--echo #
exec $MYSQLXTEST $param_mysqlxtest
  -ux_root --password='' -v%SESSION_CLOSE_SCENARIO%=Session_reset
  --file=$MYSQL_TMP_DIR/session_reset_prepared_stmt.xpl;
--echo
--echo ## 3.c
--echo #
exec $MYSQLXTEST $param_mysqlxtest
  -ux_root --password='' -v%SESSION_CLOSE_SCENARIO%=Session_close
  --file=$MYSQL_TMP_DIR/session_reset_prepared_stmt.xpl;

## Cleanup
--remove_file $MYSQL_TMP_DIR/session_reset.xpl
--remove_file $MYSQL_TMP_DIR/session_reset_single_frame.xpl
--remove_file $MYSQL_TMP_DIR/session_reset_prepared_stmt.xpl
