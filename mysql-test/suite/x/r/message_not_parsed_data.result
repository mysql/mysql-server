SET GLOBAL mysqlx_connect_timeout = 300;
SET GLOBAL mysqlx_wait_timeout = 300;


## I. Handling of unparsable 
#
# 1. Send message with not-parsable data and verify that parsable part was properly handled
# 2. Send message normal query to verify that data from previous call was properly discarded 

#
# I.1
# Protobuf uses tags to mark its message fields,
# in general \x00 is not a valid tag, still in this test its
# used as end-of-message tag:
#
#          ===================PROPER MESSAGE================>EOM==>not-parsed=>
#           MSG_LENGTH_FIVE_BYTES_LONGER MSG_TYPE MSG_BODY   \x00  MSG_LENGTH
#
# Where last MSG_LENGTH is not parsed garbage.

# Sending message with not-parsable data, and receive an error:
Sending 20 bytes raw data...
Got expected error:
Mysqlx.Error {
  severity: FATAL
  code: 5000
  msg: "Invalid message-frame."
  sql_state: "HY000"
}

# Wait until server disconnects because of the error
ok
SET GLOBAL mysqlx_connect_timeout= DEFAULT;
SET GLOBAL mysqlx_wait_timeout = DEFAULT;
