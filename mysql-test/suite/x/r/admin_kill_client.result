create user unpriv@localhost;
set password for unpriv@localhost = 'x';
client_id	user	host	sql_session
1	x_root	localhost	$SESSION
command ok
Got expected error: 'what' is not a valid field for list_clients command (code 5021)
connecting...
active session is now 'aux'
client_id	user	host	sql_session
1	x_root	localhost	$SESSION
2	x_root	localhost	$SESSION
command ok
switched to session default
client_id	user	host	sql_session
1	x_root	localhost	$SESSION
2	x_root	localhost	$SESSION
command ok
Got expected error: Invalid number of arguments, expected value for 'id' (code 5015)

command ok
Mysqlx.Notice.Frame {
  type: 1
  payload: "Mysqlx.Notice.Warning { level: ERROR\ncode: 3169\nmsg: \"Session was killed\"\n }"
}

Server disconnected
closing session aux
switched to session default
client_id	user	host	sql_session
1	x_root	localhost	$SESSION
command ok
connecting...
active session is now 'unpriv'
Should list unprivs session only
client_id	user	host	sql_session
3	unpriv	localhost	$SESSION
command ok
Got expected error: You are not owner of thread (code 1095)
switched to session default
client_id	user	host	sql_session
1	x_root	localhost	$SESSION
3	unpriv	localhost	$SESSION
command ok

command ok
Mysqlx.Notice.Frame {
  type: 1
  payload: "Mysqlx.Notice.Warning { level: ERROR\ncode: 3169\nmsg: \"Session was killed\"\n }"
}

Server disconnected
closing session unpriv
switched to session default
client_id	user	host	sql_session
1	x_root	localhost	$SESSION
command ok
Mysqlx.Ok {
  msg: "bye!"
}
ok
drop user unpriv@localhost;
