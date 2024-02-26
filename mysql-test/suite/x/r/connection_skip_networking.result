create user nonblank@localhost identified by 'nonblank';
Test starts here
# Verify that UNIX socket connection still works
RUN select user(),current_user(),@@bind_address
user()	current_user()	@@bind_address
x_root@localhost	x_root@localhost	*
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
# Verify that TCP is disabled
Application terminated with expected ERROR: Connection refused connecting to localhost:PORT (code 2002)
ok
# Try login using mysqlx protocol when server started with skip-networking options and mysqlx loaded with non super user
Application terminated with expected ERROR: Connection refused connecting to localhost:PORT (code 2002)
ok
call mtr.add_suppression("Plugin mysqlx reported: .Preparation of I/O interfaces failed, X Protocol won't be accessible");
call mtr.add_suppression("Plugin mysqlx reported: .X Plugin failed to setup .+ .., with:");
call mtr.add_suppression("Plugin mysqlx reported: .+ path is empt");
call mtr.add_suppression("Plugin mysqlx reported: .+ is set to empty text");
# restart: --skip-networking --loose-mysqlx-socket=
# Verify that TCP is disabled
# Verify that UNIX socket is disabled
drop user nonblank@localhost;
