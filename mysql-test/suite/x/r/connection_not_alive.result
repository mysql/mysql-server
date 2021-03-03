CREATE TABLE test.table (field INT);
INSERT INTO test.table VALUES (1),(2),(3);

I. Aborting of sleeping session
===============================
connecting...
active session is now 'sleep_session'
aborting session sleep_session
switched to session default

II. Aborting of updating locked table session
=============================================
connecting...
active session is now 'lock_table'

0 rows affected
connecting...
active session is now 'try_update_lock_table'
aborting session try_update_lock_table
switched to session default

0 rows affected
closing session lock_table
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default

III. Aborting of updating table session used by transaction
===========================================================
connecting...
active session is now 'transaction'

0 rows affected

3 rows affected
Rows matched: 3  Changed: 3  Warnings: 0
connecting...
active session is now 'try_update_table_in_trans'
aborting session try_update_table_in_trans
switched to session default

0 rows affected
closing session transaction
Mysqlx.Ok {
  msg: "bye!"
}
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP TABLE test.table;
