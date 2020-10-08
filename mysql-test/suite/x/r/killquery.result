## I. Kill query that executes for a long time(contains SQL sleep). It must be killed from another
#     XSession.
# 1. Sleep is execute inside "WHERE" clause (of SELECT statement)
# 2. Sleep is execute inside "select_expr" of SELECT statement, and must return multiple rows

CREATE TABLE data(b INT);
INSERT INTO data(b) VALUES (1),(2),(3);

#
# I.1
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

#
# I.2
connecting...
active session is now 'third'
RUN SELECT connection_id() AS Value;
RUN select sleep(20),b from data;
switched to session default
RUN KILL QUERY $ID

0 rows affected
switched to session third
sleep(20)	b
1	1
Got expected error: Query execution was interrupted (code 1317)
RUN SELECT '"third" session must work' AS state
state
"third" session must work
0 rows affected
closing session third
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP TABLE data;
