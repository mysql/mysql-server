#
## Preamble
#
#
## Testcase prepare
#
call mtr.add_suppression("Plugin mysqlx reported: '.*: Unsuccessful login attempt");
CREATE DATABASE xdb;
CREATE DATABASE ydb;
CREATE USER 'user_with_access_to_xdb'@'%';
CREATE USER 'user_with_access_to_ydb'@'%';
GRANT SELECT ON xdb.* TO 'user_with_access_to_xdb'@'%';
GRANT SELECT ON ydb.* TO 'user_with_access_to_ydb'@'%';
#
## Testcase execute
#
#
## Test group 1.
##
## Authenticate using plain X Protocol flows to schema that:
##
## * account has permissions
## * account has not permissions
## * account has permissions still db-doesn't exist
#
connecting...
active session is now 'non_root_user'
Login OK
closing session non_root_user
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default
connecting...
active session is now 'non_root_user'
Got expected error: Access denied for user 'user_with_access_to_ydb'@'%' to database 'xdb' (code 1044)
aborting session non_root_user
switched to session default
connecting...
active session is now 'root_user'
Got expected error: Unknown database 'non_existing_schema' (code 1049)
aborting session root_user
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
#
## Test group 2.
##
## Authenticate using an account which has permissions
## to selected schema.
#

# Using MySQL client
# Checking if requested schema was selected.
IF(DATABASE()='xdb','Success','Failure')
Success

# Using mysqlxtest
# Checking if requested schema was selected.
RUN SELECT IF(DATABASE()='xdb','Success','Failure')
IF(DATABASE()='xdb','Success','Failure')
Success
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
#
## Test group 3.
##
## Authenticate using an account which has not permissions
## to selected schema.
#

# Using MySQL client
# Checking if requested schema was selected.
EXPECTED_ERROR_CODE(42000): Access denied for user 'user_with_access_to_xdb'@'%' to database 'ydb'

# Using mysqlxtest
# Checking if requested schema was selected.
in main, line 0:ERROR: Access denied for user 'user_with_access_to_xdb'@'%' to database 'ydb' (EXPECTED_ERROR_CODE)
not ok
#
## Test group 4.
##
## Authenticate using an valid account to non existing
## schema.
#

# Using MySQL client
# Checking if requested schema was selected.
EXPECTED_ERROR_CODE(42000): Unknown database 'non_existing_schema'

# Using mysqlxtest
# Checking if requested schema was selected.
in main, line 0:ERROR: Unknown database 'non_existing_schema' (EXPECTED_ERROR_CODE)
not ok
#
## Cleanup
#
DROP USER 'user_with_access_to_xdb'@'%';
DROP USER 'user_with_access_to_ydb'@'%';
DROP DATABASE xdb;
DROP DATABASE ydb;
