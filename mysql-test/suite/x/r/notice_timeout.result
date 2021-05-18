## I. Validate that X Plugin sends IO Read notice on read timeout for authenticted connections
#
## II. Validate that X Plugin sends only ServerHello notice on read timeout for not authenticted connections
#

SET GLOBAL mysqlx_read_timeout=5;

#
# I

#
# Creating sessions
connecting...
active session is now 'session_0'
Sending 4 bytes raw data...
connecting...
active session is now 'session_1'
Sending 4 bytes raw data...
connecting...
active session is now 'session_2'
Sending 4 bytes raw data...
connecting...
active session is now 'session_3'
Sending 4 bytes raw data...
connecting...
active session is now 'session_4'
Sending 4 bytes raw data...
connecting...
active session is now 'session_5'
Sending 4 bytes raw data...
connecting...
active session is now 'session_6'
Sending 4 bytes raw data...
connecting...
active session is now 'session_7'
Sending 4 bytes raw data...
connecting...
active session is now 'session_8'
Sending 4 bytes raw data...
connecting...
active session is now 'session_9'
Sending 4 bytes raw data...

#
# Reciving close notices
switched to session session_0
[ASSERT notice warning - ERROR, 1810]
Server disconnected
closing session session_0
switched to session default
switched to session session_1
[ASSERT notice warning - ERROR, 1810]
Server disconnected
closing session session_1
switched to session default
switched to session session_2
[ASSERT notice warning - ERROR, 1810]
Server disconnected
closing session session_2
switched to session default
switched to session session_3
[ASSERT notice warning - ERROR, 1810]
Server disconnected
closing session session_3
switched to session default
switched to session session_4
[ASSERT notice warning - ERROR, 1810]
Server disconnected
closing session session_4
switched to session default
switched to session session_5
[ASSERT notice warning - ERROR, 1810]
Server disconnected
closing session session_5
switched to session default
switched to session session_6
[ASSERT notice warning - ERROR, 1810]
Server disconnected
closing session session_6
switched to session default
switched to session session_7
[ASSERT notice warning - ERROR, 1810]
Server disconnected
closing session session_7
switched to session default
switched to session session_8
[ASSERT notice warning - ERROR, 1810]
Server disconnected
closing session session_8
switched to session default
switched to session session_9
[ASSERT notice warning - ERROR, 1810]
Server disconnected
closing session session_9
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok

#
# II

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

#
# Closing connection
switched to session session_0
Server disconnected
closing session session_0
switched to session default
switched to session session_1
Server disconnected
closing session session_1
switched to session default
switched to session session_2
Server disconnected
closing session session_2
switched to session default
switched to session session_3
Server disconnected
closing session session_3
switched to session default
switched to session session_4
Server disconnected
closing session session_4
switched to session default
switched to session session_5
Server disconnected
closing session session_5
switched to session default
switched to session session_6
Server disconnected
closing session session_6
switched to session default
switched to session session_7
Server disconnected
closing session session_7
switched to session default
switched to session session_8
Server disconnected
closing session session_8
switched to session default
switched to session session_9
Server disconnected
closing session session_9
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_read_timeout= DEFAULT;
