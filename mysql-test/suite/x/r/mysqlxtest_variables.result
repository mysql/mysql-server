CREATE DATABASE xtest;
CREATE USER 'user1'@'127.0.0.1' IDENTIFIED BY 'user1_pass';
CREATE USER 'user2'@'127.0.0.1' IDENTIFIED BY 'user2_pass';
GRANT SELECT ON xtest.* TO user1@'127.0.0.1';
GRANT SELECT ON xtest.* TO user2@'127.0.0.1';

############################################################
## Verify mysqlxtests variables at first connection
##
#
# Command line options must be unchange, on unauthenticated connection
# on every session or authenticated account
#
# Authenticating on existing connection
Login OK
# * SOCKET_ID must be the same as before
# * CLIENT_ID must have a expected value
#
# Command line options must be unchange, on authenticated connection
# on every session or authenticated account
#
# New connection:
connecting...
active session is now 'second_session'
#
# Command line options must be unchange, on second session
# on every session or authenticated account
# * SOCKET_ID must have a new value
# * CLIENT_ID must have a new value
ok

############################################################
## Verify mysqlxtests variables at second connection
##
#
# Command line options must be unchange, on unauthenticated connection
# on every session or authenticated account
#
# Authenticating on existing connection
Login OK
# * SOCKET_ID must be the same as before
# * CLIENT_ID must have a expected value
#
# Command line options must be unchange, on authenticated connection
# on every session or authenticated account
#
# New connection:
connecting...
active session is now 'second_session'
#
# Command line options must be unchange, on second session
# on every session or authenticated account
# * SOCKET_ID must have a new value
# * CLIENT_ID must have a new value
ok
DROP USER 'user1'@'127.0.0.1';
DROP USER 'user2'@'127.0.0.1';
