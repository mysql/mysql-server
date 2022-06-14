Mysqlx.Ok {
  msg: "bye!"
}
ok
Test lock file issue
call mtr.add_suppression("Plugin mysqlx reported: .Setup of socket: '..+/mysqlx.custom' failed, lock file is empty");
# restart: --loose-mysqlx-socket=SOCKET
Application terminated with expected error: No such file or directory, while connecting to SOCKET (code 2002)
ok
call mtr.add_suppression("Plugin mysqlx reported: .Setup of socket: '..+/mysqlx.dir.socket' failed, can't read lock file");
call mtr.add_suppression("Plugin mysqlx reported: .* wasn't allocated by X Plugin ..+/mysqlx.dir.socket.lock");
# restart: --loose-mysqlx-socket=SOCKET
Application terminated with expected error: No such file or directory, while connecting to SOCKET (code 2002)
ok
call mtr.add_suppression("Plugin mysqlx reported: .Setup of socket: '..+/mysqlx.custom' failed, invalid PID in UNIX socket lock file ");
# restart: --loose-mysqlx-socket=SOCKET
Application terminated with expected error: No such file or directory, while connecting to SOCKET (code 2002)
ok
# restart:
