#
# Assert the user exist from the start
#
include/assert.inc [session_user should exist before X plugin install]
#
# Start X plugin and check all is OK
#
include/assert.inc [session_user should exist after X plugin install]
#
# Delete the user
#
DROP USER "mysql.session"@"localhost";
call mtr.add_suppression("Plugin mysqlx reported: 'Unable to switch context to user mysql.session'");
#
# Recreate the user with an upgrade and see that X plugin works
#
Run mysql_upgrade
# restart:--upgrade=FORCE
include/assert.inc [session_user should exist after upgrade]
RUN SELECT CURRENT_USER()

CURRENT_USER()
x_root@localhost
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
#
# Uninstall plugin and verify that user is still there
#
include/assert.inc [session_user should exist after install]
#
# Cleaning
#
call mtr.add_suppression("Unable to use user mysql.session context *.*");
call mtr.add_suppression("On plugin shutdown it was not possible to reset the server read mode settings. Try to reset it manually.");
