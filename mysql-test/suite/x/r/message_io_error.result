SET GLOBAL mysqlx_connect_timeout = 300;
SET GLOBAL mysqlx_wait_timeout = 300;
## 1. Trigger disconnection in middle of X Message
#
## Send the message, without MSG_BODY_FIELD2
#
Sending 11 bytes raw data...
## Drop the connection in middle of the message
#
ok
## 2. Verify that no StmtExecute was handled
#
#
# Wait until the session is dropped
#
# After that verify that non of StmtExecutes was processed
#
# Syncing with status [ 'Mysqlx_stmt_execute_sql' == 0 ]
# Syncing with status [ 'Mysqlx_stmt_execute_mysqlx' == 0 ]
# Syncing with status [ 'Mysqlx_stmt_execute_xplugin' == 0 ]
SET GLOBAL mysqlx_connect_timeout= DEFAULT;
SET GLOBAL mysqlx_wait_timeout = DEFAULT;
