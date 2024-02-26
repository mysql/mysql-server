Preamble
# restart
include/assert.inc [Mysqlx_connection_accept_errors should be 0]
include/assert.inc [Mysqlx_connection_errors should be 0]
include/assert.inc [Mysqlx_connections_accepted should be 0]
include/assert.inc [Mysqlx_connections_closed should be 0]
include/assert.inc [Mysqlx_connections_rejected should be 0]
SET GLOBAL mysqlx_connect_timeout = 4;
include/assert.inc [ should be 4]

## Disconnect user on idle connection after timeout
#
# 1. Wait until user is disconnected
ok

## Disconnect user without session after timeout
#
# 1. Connect to X Plugin
connecting...
active session is now 'idle_session'
# 2. Show that its valid X Protocol connection
send Mysqlx.Connection.CapabilitiesGet {
}

# 3. Generate long message flow, ignore all disconnect errors
#
# lets wait 30 seconds, for the disconnection
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send/receive
Got expected error (one of: 0 2006 )
Got expected error (one of: 0 2006 )
send Mysqlx.Connection.CapabilitiesGet {
}

Got expected error: MySQL server has gone away (code 2006)
# 4. Show that user is/was disconnected
Got expected error: MySQL server has gone away (code 2006)
aborting session idle_session
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok

## Inactivity timer disabled after session setup (commands executed after 1s should be processed)
#
Mysqlx.Ok {
  msg: "bye!"
}
ok
include/assert.inc [Mysqlx_connection_accept_errors should be 0]
include/assert.inc [Mysqlx_connection_errors should be 2]
include/assert.inc [Mysqlx_connections_accepted should be 4]
include/assert.inc [Mysqlx_connections_closed should be 4]
include/assert.inc [Mysqlx_connections_rejected should be 0]
SET GLOBAL mysqlx_connect_timeout = 2;
include/assert.inc [ should be 2]

## Disconnect user on idle connection after new timeout
#
# 1. Wait until user is disconnected
ok
SET GLOBAL mysqlx_connect_timeout = 4;
include/assert.inc [ should be 4]

## Inactivity timer restarted with Session.Reset message
#
# 1. Connect to X Plugin and show that its a valid connection
Afert the Session_reset connection isn't considered a target for release

command ok
# 2. Trigger reset
send Mysqlx.Session.Reset {
  keep_open: false
}

RUN recvok
# 3. Wait longer than Mysqlx_connection_timeout
# 4. Show that connection was not released, by authenticated and executing query
Login OK

command ok
Mysqlx.Ok {
  msg: "bye!"
}
ok
include/assert.inc [Mysqlx_connection_accept_errors should be 0]
include/assert.inc [Mysqlx_connection_errors should be 3]
include/assert.inc [Mysqlx_connections_accepted should be 6]
include/assert.inc [Mysqlx_connections_closed should be 6]
include/assert.inc [Mysqlx_connections_rejected should be 0]
SET GLOBAL mysqlx_connect_timeout = DEFAULT;
