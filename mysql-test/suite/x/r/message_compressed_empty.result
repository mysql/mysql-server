SET GLOBAL mysqlx_connect_timeout = 300;
SET GLOBAL mysqlx_wait_timeout = 300;
SET GLOBAL mysqlx_max_allowed_packet = 1000;

## A. Execute the test using "deflate" algorithm
#

## 1. Send empty compressed frame
## 2. Send empty compressed payload
## 3. Send empty single-compressed frame and check its processed
## 4. Send empty compressed frame with invalid uncompressed-size field
## 5. Send empty compressed frame with invalid client_messages field
## 6. Send empty compressed frame with unknown field of group type
## 7. Send empty compressed frame with invalid payload field
## 8. Send empty compressed frame with unknown field
#

# 1.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 5 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default
;
# 2
030000002e2200
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 7 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default
;
# 3.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
RUN SHOW STATUS LIKE 'Mysqlx_expect_open';
Verify [Status variable "Mysqlx_expect_open" needs to have a value of 0]
send Mysqlx.Connection.Compression {
  uncompressed_size: 1000
  payload: "BINARY"
}

RUN recvok
RUN SHOW STATUS LIKE 'Mysqlx_expect_open';
Verify [Status variable "Mysqlx_expect_open" needs to have a value of 1]
closing session SOME_SESSION
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

# 4.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 6 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default

# 5.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 6 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default

# 6.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 6 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default

# 7.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 6 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default

# 8.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 7 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok

## B. Execute the test using "lz4" algorithm
#

## 1. Send empty compressed frame
## 2. Send empty compressed payload
## 3. Send empty single-compressed frame and check its processed
## 4. Send empty compressed frame with invalid uncompressed-size field
## 5. Send empty compressed frame with invalid client_messages field
## 6. Send empty compressed frame with unknown field of group type
## 7. Send empty compressed frame with invalid payload field
## 8. Send empty compressed frame with unknown field
#

# 1.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 5 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default
;
# 2
030000002e2200
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 7 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default
;
# 3.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
RUN SHOW STATUS LIKE 'Mysqlx_expect_open';
Verify [Status variable "Mysqlx_expect_open" needs to have a value of 0]
send Mysqlx.Connection.Compression {
  uncompressed_size: 1000
  payload: "BINARY"
}

RUN recvok
RUN SHOW STATUS LIKE 'Mysqlx_expect_open';
Verify [Status variable "Mysqlx_expect_open" needs to have a value of 1]
closing session SOME_SESSION
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

# 4.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 6 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default

# 5.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 6 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default

# 6.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 6 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default

# 7.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 6 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default

# 8.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 7 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok

## C. Execute the test using "zstd" algorithm
#

## 1. Send empty compressed frame
## 2. Send empty compressed payload
## 3. Send empty single-compressed frame and check its processed
## 4. Send empty compressed frame with invalid uncompressed-size field
## 5. Send empty compressed frame with invalid client_messages field
## 6. Send empty compressed frame with unknown field of group type
## 7. Send empty compressed frame with invalid payload field
## 8. Send empty compressed frame with unknown field
#

# 1.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 5 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default
;
# 2
030000002e2200
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 7 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default
;
# 3.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
RUN SHOW STATUS LIKE 'Mysqlx_expect_open';
Verify [Status variable "Mysqlx_expect_open" needs to have a value of 0]
send Mysqlx.Connection.Compression {
  uncompressed_size: 1000
  payload: "BINARY"
}

RUN recvok
RUN SHOW STATUS LIKE 'Mysqlx_expect_open';
Verify [Status variable "Mysqlx_expect_open" needs to have a value of 1]
closing session SOME_SESSION
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

# 4.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 6 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default

# 5.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 6 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default

# 6.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 6 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default

# 7.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 6 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default

# 8.
connecting...
active session is now 'SOME_SESSION'
RUN recvok
Login OK
Sending 7 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5174
  msg: "Invalid compressed frame."
  sql_state: "HY000"
}

## Waiting for disconnection
closing session SOME_SESSION
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_connect_timeout= DEFAULT;
SET GLOBAL mysqlx_wait_timeout = DEFAULT;
SET GLOBAL mysqlx_max_allowed_packet = DEFAULT;
