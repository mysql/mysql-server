RUN SELECT CONNECTION_TYPE from performance_schema.threads where processlist_command='Query';
CONNECTION_TYPE
Socket
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
SET GLOBAL mysqlx_socket=null;
ERROR HY000: Variable 'mysqlx_socket' is a read only variable
SET @@global.mysqlx_socket='invalid';
ERROR HY000: Variable 'mysqlx_socket' is a read only variable
SET GLOBAL mysqlx_socket=$MYSQL_TMP_DIR/socket_mysqlx.sock;
ERROR HY000: Variable 'mysqlx_socket' is a read only variable
call mtr.add_suppression("Plugin mysqlx reported: .Setup of socket: '' failed, the socket file path is empty");
# restart: --loose-mysqlx-socket=
Application terminated with expected error: No such file or directory, while connecting to  SOCKET (code 2002)
ok
call mtr.add_suppression("Plugin mysqlx reported: .Setup of socket: '\(.+\)' failed, can't create lock file /dev/null/mysqlx\.sock\.lock");
# restart: --loose-mysqlx-socket=/dev/null/mysqlx.sock
Application terminated with expected error: No such file or directory, while connecting to  SOCKET (code 2002)
ok
Application terminated with expected error: Not a directory, while connecting to  SOCKET (code 2002)
ok
call mtr.add_suppression("Plugin mysqlx reported: 'X Plugins UNIX socket must use different file than MySQL server. X Plugin won't be accessible through UNIX socket");
# restart: --loose-mysqlx-socket= SOCKET
# restart: --loose-mysqlx-socket= SOCKET
RUN SELECT CONNECTION_TYPE from performance_schema.threads where processlist_command='Query';
CONNECTION_TYPE
Socket
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
call mtr.add_suppression("Plugin mysqlx reported: .Setup of socket: '\(.+\)' failed, .bind... on UNIX socket failed with error");
# restart: --loose-mysqlx-socket= SOCKET
Application terminated with expected error: Connection refused, while connecting to  SOCKET (code 2002)
ok
call mtr.add_suppression("Plugin mysqlx reported: .Setup of socket: '\(.+\)' failed, can't create lock file .+\.lock");
# restart: --loose-mysqlx-socket= SOCKET
Application terminated with expected error: Permission denied, while connecting to  SOCKET (code 2002)
ok
call mtr.add_suppression("Plugin mysqlx reported: .Setup of socket: '\(.+\)' failed, can't create lock file .*ne/x.sock.lock");
# restart: --loose-mysqlx-socket= SOCKET
Application terminated with expected error: No such file or directory, while connecting to  SOCKET (code 2002)
ok
call mtr.add_suppression("Plugin mysqlx reported: .Setup of socket: '\(.+\)' failed, the socket file path is too long");
# restart: --loose-mysqlx-socket= SOCKET
Application terminated with expected error: UNIX Socket file name too long, size should be less or equal LENGTH (code 2005)
ok
# restart:
