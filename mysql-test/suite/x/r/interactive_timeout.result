Preamble
SET GLOBAL mysqlx_interactive_timeout = 5;
SET GLOBAL mysqlx_wait_timeout = 10;
CREATE USER temp_user@localhost
IDENTIFIED WITH 'mysql_native_password' BY 'auth_string';
GRANT ALL ON *.* TO temp_user@localhost;
#
# Run the test without SSL
#
Verify [Session variable "mysqlx_wait_timeout" needs to have a value of 5]
Verify [Global variable "mysqlx_wait_timeout" needs to have a value of 10]
RUN SHOW SESSION VARIABLES LIKE 'mysqlx_wait_timeout'
Variable_name	Value
mysqlx_wait_timeout	5
0 rows affected
RUN SELECT CONNECTION_TYPE from performance_schema.threads where processlist_command='Query'
CONNECTION_TYPE
TCP/IP
0 rows affected
RUN SELECT USER()
USER()
temp_user@localhost
0 rows affected
RUN SHOW STATUS LIKE 'Mysqlx_ssl_active'
Variable_name	Value
Mysqlx_ssl_active	OFF
0 rows affected
# Hanging 'receive' command will cause interactive_timeout
Mysqlx.Notice.Frame {
  type: 1
  payload: "Mysqlx.Notice.Warning { level: ERROR\ncode: 1810\nmsg: \"IO Read error: read_timeout exceeded\"\n }"
}

# We do no expect any message to be received here,
# verification is done by comparing with 'result' file.
Server disconnected
ok
#
# Run the test with SSL
#
Verify [Session variable "mysqlx_wait_timeout" needs to have a value of 5]
Verify [Global variable "mysqlx_wait_timeout" needs to have a value of 10]
RUN SHOW SESSION VARIABLES LIKE 'mysqlx_wait_timeout'
Variable_name	Value
mysqlx_wait_timeout	5
0 rows affected
RUN SELECT CONNECTION_TYPE from performance_schema.threads where processlist_command='Query'
CONNECTION_TYPE
SSL/TLS
0 rows affected
RUN SELECT USER()
USER()
temp_user@localhost
0 rows affected
RUN SHOW STATUS LIKE 'Mysqlx_ssl_active'
Variable_name	Value
Mysqlx_ssl_active	ON
0 rows affected
# Hanging 'receive' command will cause interactive_timeout
Mysqlx.Notice.Frame {
  type: 1
  payload: "Mysqlx.Notice.Warning { level: ERROR\ncode: 1810\nmsg: \"IO Read error: read_timeout exceeded\"\n }"
}

# We do no expect any message to be received here,
# verification is done by comparing with 'result' file.
Server disconnected
ok
SET GLOBAL mysqlx_interactive_timeout = DEFAULT;
SET GLOBAL mysqlx_wait_timeout = DEFAULT;
DROP USER temp_user@localhost;
