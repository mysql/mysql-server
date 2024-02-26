create user unpriv@localhost;
set password for unpriv@localhost = 'x';
grant connection_admin on *.* to unpriv@localhost;
create role sys_user_role;
grant system_user on *.* to sys_user_role;
grant sys_user_role to unpriv@localhost;
client_id	user	host	sql_session
1	x_root	localhost	$SESSION
command ok
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
connecting...
active session is now 'unpriv'
Should list unprivs session only
client_id	user	host	sql_session
3	unpriv	localhost	$SESSION
command ok
Try to kill a x_root session from regular session
Got expected error:
Mysqlx.Error {
  severity: ERROR
  code: 1095
  msg: "You are not owner of thread"
  sql_state: "HY000"
}

promote regular session to power session
RUN set role sys_user_role

0 rows affected
Try to kill a x_root session from unpriv

command ok
switched to session default
Should two sessions
client_id	user	host	sql_session
1	x_root	localhost	$SESSION
3	unpriv	localhost	$SESSION
command ok
Now kill one power_session from another power_session

command ok
Should list only x_root session
client_id	user	host	sql_session
1	x_root	localhost	$SESSION
command ok
RUN drop schema if exists xtest

0 rows affected
RUN drop user unpriv@localhost

0 rows affected
RUN drop role sys_user_role

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
