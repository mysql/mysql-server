connecting...
active session is now 'second'
RUN SELECT connection_id() AS Value;
RUN SELECT * FROM mysql.user WHERE SLEEP(30);
switched to session default
RUN KILL QUERY $ID

0 rows affected
switched to session second
Got expected error: Query execution was interrupted (code 1317)
RUN SELECT '"Second" session must work' AS state
state
"Second" session must work
0 rows affected
closing session second
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default
RUN SELECT 'Default session must work' AS state
state
Default session must work
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
