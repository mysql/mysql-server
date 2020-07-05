SET GLOBAL mysqlx_connect_timeout = 300;
SET GLOBAL mysqlx_wait_timeout = 300;

command ok
connecting...
active session is now 'test_different_messages1'
Try to send number of objects less then X Protocol limit, 50 (2 * 25)
doc
command ok
Try to send number of objects equal to the X Protocol limit, 100 (2 * 50)
doc
command ok
Try to send number of object more than the X Protocol limit 101 (2 * 51)
Received error message with code: ER_X_BAD_MESSAGE
closing session test_different_messages1
switched to session default
Mysqlx.Ok {
  msg: "bye!"
}
ok

command ok
Try to send number of object more than the X Protocol limit 101 (2 * 51)
Received error message with code: ER_X_BAD_MESSAGE
ok
SET GLOBAL mysqlx_connect_timeout= DEFAULT;
SET GLOBAL mysqlx_wait_timeout = DEFAULT;
DROP TABLE IF EXISTS coll;
