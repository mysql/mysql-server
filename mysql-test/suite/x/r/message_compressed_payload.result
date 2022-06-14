CREATE SCHEMA xtest;
SET GLOBAL mysqlx_connect_timeout = 300;
SET GLOBAL mysqlx_wait_timeout = 300;
SET GLOBAL mysqlx_max_allowed_packet = 1000;

## A. Execute the test with deflate
#

## 1. Send compressed message with invalid payload
#  a. Send compressed-message with too long-uncompressed payload size
#  b. Send compressed-message with invalid compressed-payload
## 2. Send compressed message with invalid message
#  a. Send compressed-message with StmtExecute and missing fields (not initialized error)
#  b. Send compressed-message with not know message, and verify that its dispatched
#  c. Send compressed-message with message that exceeds protobufs nested
#     messages limit, and verify that its rejected with proper error
;
# 1.a.
connecting...
active session is now 'SESS_SINGLE1'
RUN recvok
Login OK
send Mysqlx.Connection.Compression {
  uncompressed_size: 268435456
  payload: "BINARY"
}

closing session SESS_SINGLE1
switched to session default
;
# 1.b.
connecting...
active session is now 'SESS_SINGLE2'
RUN recvok
Login OK
send Mysqlx.Connection.Compression {
  payload: "BINARY"
}

Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5171
  msg: "Payload decompression failed"
  sql_state: "HY000"
}

closing session SESS_SINGLE2
switched to session default
;
# 2.a.
connecting...
active session is now 'SESS_SINGLE3'
RUN recvok
Login OK
send Mysqlx.Connection.Compression {
  payload: "BINARY"
}

Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5000
  msg: "Parse error unserializing protobuf message"
  sql_state: "HY000"
}

closing session SESS_SINGLE3
switched to session default
;
# 2.b.
connecting...
active session is now 'SESS_SINGLE4'
RUN recvok
Login OK
send Mysqlx.Expect.Open {
  cond {
    condition_key: 1
  }
}

RUN recvok
send Mysqlx.Connection.Compression {
  payload: "BINARY"
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 1047
  msg: "Unexpected message received"
  sql_state: "HY000"
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5159
  msg: "Expectation failed: no_error"
  sql_state: "HY000"
}

send Mysqlx.Expect.Close {
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5159
  msg: "Expectation failed: no_error"
  sql_state: "HY000"
}


command ok
;
# 2.c.
connecting...
active session is now 'SESS_SINGLE5'
RUN recvok
Login OK

command ok
Try to send number of object more than the X Protocol limit 101 (2 * 51)
Received error message with code: ER_X_BAD_MESSAGE
closing session SESS_SINGLE5
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok

## B. Execute the test with lz4
#

## 1. Send compressed message with invalid payload
#  a. Send compressed-message with too long-uncompressed payload size
#  b. Send compressed-message with invalid compressed-payload
## 2. Send compressed message with invalid message
#  a. Send compressed-message with StmtExecute and missing fields (not initialized error)
#  b. Send compressed-message with not know message, and verify that its dispatched
#  c. Send compressed-message with message that exceeds protobufs nested
#     messages limit, and verify that its rejected with proper error
;
# 1.a.
connecting...
active session is now 'SESS_SINGLE1'
RUN recvok
Login OK
send Mysqlx.Connection.Compression {
  uncompressed_size: 268435456
  payload: "BINARY"
}

closing session SESS_SINGLE1
switched to session default
;
# 1.b.
connecting...
active session is now 'SESS_SINGLE2'
RUN recvok
Login OK
send Mysqlx.Connection.Compression {
  payload: "BINARY"
}

Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5171
  msg: "Payload decompression failed"
  sql_state: "HY000"
}

closing session SESS_SINGLE2
switched to session default
;
# 2.a.
connecting...
active session is now 'SESS_SINGLE3'
RUN recvok
Login OK
send Mysqlx.Connection.Compression {
  payload: "BINARY"
}

Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5000
  msg: "Parse error unserializing protobuf message"
  sql_state: "HY000"
}

closing session SESS_SINGLE3
switched to session default
;
# 2.b.
connecting...
active session is now 'SESS_SINGLE4'
RUN recvok
Login OK
send Mysqlx.Expect.Open {
  cond {
    condition_key: 1
  }
}

RUN recvok
send Mysqlx.Connection.Compression {
  payload: "BINARY"
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 1047
  msg: "Unexpected message received"
  sql_state: "HY000"
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5159
  msg: "Expectation failed: no_error"
  sql_state: "HY000"
}

send Mysqlx.Expect.Close {
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5159
  msg: "Expectation failed: no_error"
  sql_state: "HY000"
}


command ok
;
# 2.c.
connecting...
active session is now 'SESS_SINGLE5'
RUN recvok
Login OK

command ok
Try to send number of object more than the X Protocol limit 101 (2 * 51)
Received error message with code: ER_X_BAD_MESSAGE
closing session SESS_SINGLE5
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok

## C. Execute the test with zstd
#

## 1. Send compressed message with invalid payload
#  a. Send compressed-message with too long-uncompressed payload size
#  b. Send compressed-message with invalid compressed-payload
## 2. Send compressed message with invalid message
#  a. Send compressed-message with StmtExecute and missing fields (not initialized error)
#  b. Send compressed-message with not know message, and verify that its dispatched
#  c. Send compressed-message with message that exceeds protobufs nested
#     messages limit, and verify that its rejected with proper error
;
# 1.a.
connecting...
active session is now 'SESS_SINGLE1'
RUN recvok
Login OK
send Mysqlx.Connection.Compression {
  uncompressed_size: 268435456
  payload: "BINARY"
}

closing session SESS_SINGLE1
switched to session default
;
# 1.b.
connecting...
active session is now 'SESS_SINGLE2'
RUN recvok
Login OK
send Mysqlx.Connection.Compression {
  payload: "BINARY"
}

Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5171
  msg: "Payload decompression failed"
  sql_state: "HY000"
}

closing session SESS_SINGLE2
switched to session default
;
# 2.a.
connecting...
active session is now 'SESS_SINGLE3'
RUN recvok
Login OK
send Mysqlx.Connection.Compression {
  payload: "BINARY"
}

Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5000
  msg: "Parse error unserializing protobuf message"
  sql_state: "HY000"
}

closing session SESS_SINGLE3
switched to session default
;
# 2.b.
connecting...
active session is now 'SESS_SINGLE4'
RUN recvok
Login OK
send Mysqlx.Expect.Open {
  cond {
    condition_key: 1
  }
}

RUN recvok
send Mysqlx.Connection.Compression {
  payload: "BINARY"
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 1047
  msg: "Unexpected message received"
  sql_state: "HY000"
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5159
  msg: "Expectation failed: no_error"
  sql_state: "HY000"
}

send Mysqlx.Expect.Close {
}

Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 5159
  msg: "Expectation failed: no_error"
  sql_state: "HY000"
}


command ok
;
# 2.c.
connecting...
active session is now 'SESS_SINGLE5'
RUN recvok
Login OK

command ok
Try to send number of object more than the X Protocol limit 101 (2 * 51)
Received error message with code: ER_X_BAD_MESSAGE
closing session SESS_SINGLE5
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_connect_timeout= DEFAULT;
SET GLOBAL mysqlx_wait_timeout = DEFAULT;
SET GLOBAL mysqlx_max_allowed_packet = DEFAULT;
