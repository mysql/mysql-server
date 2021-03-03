## I. Validate that X Plugin sends shutdown notice on IDLE connections
#
# 1. Connect multiple clients, make the IDLE, and trigger restart, validate the notice
# 2. Assert status variables after server restart (they should be set at 0)
# 3. Assert MYSQLXTEST message counters that it saw the warning notice
#
## II. Validate that X Plugin sends shutdown notice on connections that process a query
#
# 1. A connection is waiting for lock
# 2. A connection is sleeping "SELECT SLEEP(A);"
#
## III. Validate that X Plugin not sends shutdown notice on not authotized connections
#


#
# I.1
connecting...
active session is now 'session_0'
connecting...
active session is now 'session_1'
connecting...
active session is now 'session_2'
connecting...
active session is now 'session_3'
connecting...
active session is now 'session_4'
connecting...
active session is now 'session_5'
connecting...
active session is now 'session_6'
connecting...
active session is now 'session_7'
connecting...
active session is now 'session_8'
connecting...
active session is now 'session_9'

#
# Shutting down MySQL Server
session_0
switched to session session_0
[ASSERT notice warning - ERROR, 1053]
Server disconnected
switched to session default
[ASSERT notice warning - ERROR, 1053]
Server disconnected
# Reconnecting default session

#
# I.2
RUN SHOW STATUS LIKE 'Mysqlx_notice_warning_sent';
Verify [Status variable "Mysqlx_notice_warning_sent" needs to have a value of 0]
RUN SHOW GLOBAL STATUS LIKE 'Mysqlx_notice_warning_sent';
Verify [Status variable "Mysqlx_notice_warning_sent" needs to have a value of 0]

#
# I.3
Verify [Client handled Mysqlx.Notice.Warning message count is "==" 1]
Mysqlx.Ok {
  msg: "bye!"
}
ok

#
# II. Prepare
RUN CREATE TABLE test.some_table(some_column int)

0 rows affected
RUN CREATE PROCEDURE test.loop_in_sp()
BEGIN
  REPEAT
    SET @A=1;
  UNTIL FALSE END REPEAT;
END


0 rows affected
connecting...
active session is now 'session_WRITE_LOCK'
RUN LOCK TABLE test.some_table WRITE

0 rows affected
connecting...
active session is now 'session_READ_LOCK'
RUN SELECT * FROM test.some_table
connecting...
active session is now 'session_SLEEP'
RUN SELECT SLEEP(10000)
connecting...
active session is now 'session_SP'
RUN CALL test.loop_in_sp()
switched to session default

#
# Shutting down MySQL Server
switched to session default

#
# II.1
switched to session session_WRITE_LOCK
[ASSERT notice warning - ERROR, 1053]
Server disconnected

#
# II.1
switched to session session_READ_LOCK
Got expected error (one of: 0 1317 )
Mysqlx.Notice.Frame {
  type: 1
  payload: "Mysqlx.Notice.Warning { level: ERROR\ncode: 1053\nmsg: \"Server shutdown in progress\"\n }"
}

Server disconnected

#
# II.2
switched to session session_SLEEP
SLEEP(10000)
1
0 rows affected
[ASSERT notice warning - ERROR, 1053]
Server disconnected

#
# II.3
switched to session session_SP
Mysqlx.Error(code) = 1053
[ASSERT notice warning - ERROR, 1053]
Server disconnected
closing session session_SP
Mysqlx.Ok {
  msg: "bye!"
}
ok

#
# III

#
# Creating sessions
connecting...
active session is now 'session_0'
Mysqlx.Notice.Frame(type) = 5
connecting...
active session is now 'session_1'
Mysqlx.Notice.Frame(type) = 5
connecting...
active session is now 'session_2'
Mysqlx.Notice.Frame(type) = 5
connecting...
active session is now 'session_3'
Mysqlx.Notice.Frame(type) = 5
connecting...
active session is now 'session_4'
Mysqlx.Notice.Frame(type) = 5
connecting...
active session is now 'session_5'
Mysqlx.Notice.Frame(type) = 5
connecting...
active session is now 'session_6'
Mysqlx.Notice.Frame(type) = 5
connecting...
active session is now 'session_7'
Mysqlx.Notice.Frame(type) = 5
connecting...
active session is now 'session_8'
Mysqlx.Notice.Frame(type) = 5
connecting...
active session is now 'session_9'
Mysqlx.Notice.Frame(type) = 5

#echo # Shutting down MySQL Server
switched to session default

#
# Closing connection and reconect
switched to session session_0
# `recvunitldisc` command must report disconnection without any other messages
Server disconnected
switched to session session_1
# `recvunitldisc` command must report disconnection without any other messages
Server disconnected
switched to session session_2
# `recvunitldisc` command must report disconnection without any other messages
Server disconnected
switched to session session_3
# `recvunitldisc` command must report disconnection without any other messages
Server disconnected
switched to session session_4
# `recvunitldisc` command must report disconnection without any other messages
Server disconnected
switched to session session_5
# `recvunitldisc` command must report disconnection without any other messages
Server disconnected
switched to session session_6
# `recvunitldisc` command must report disconnection without any other messages
Server disconnected
switched to session session_7
# `recvunitldisc` command must report disconnection without any other messages
Server disconnected
switched to session session_8
# `recvunitldisc` command must report disconnection without any other messages
Server disconnected
switched to session session_9
# `recvunitldisc` command must report disconnection without any other messages
Server disconnected
switched to session default
[ASSERT notice warning - ERROR, 1053]
Server disconnected
# Reconnecting default session
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP TABLE test.some_table;
DROP PROCEDURE test.loop_in_sp;
