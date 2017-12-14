RUN DROP SCHEMA IF EXISTS xtest

0 rows affected
RUN CREATE SCHEMA xtest

1 rows affected
RUN USE xtest

0 rows affected
RUN DROP PROCEDURE IF EXISTS xtest.uninstall_plugin

0 rows affected
RUN DROP PROCEDURE IF EXISTS xtest.install_plugin

0 rows affected
RUN DROP PROCEDURE IF EXISTS xtest.shutdown_server

0 rows affected
RUN CREATE PROCEDURE xtest.uninstall_plugin()
BEGIN
UNINSTALL PLUGIN `mysqlx`;
END


0 rows affected
RUN CREATE PROCEDURE xtest.install_plugin()
BEGIN
INSTALL PLUGIN `mysqlx` SONAME "$MYSQLXPLUGIN";
END


0 rows affected
RUN CREATE PROCEDURE xtest.shutdown_server()
BEGIN
SHUTDOWN;
END


0 rows affected
RUN UNINSTALL PLUGIN `mysqlx`
While executing UNINSTALL PLUGIN `mysqlx`:
Got expected error: Command not supported by pluggable protocols (code 3130)
RUN INSTALL PLUGIN `mysqlx` SONAME "$MYSQLXPLUGIN"
While executing INSTALL PLUGIN `mysqlx` SONAME "$MYSQLXPLUGIN":
Got expected error: Command not supported by pluggable protocols (code 3130)
RUN SHUTDOWN
While executing SHUTDOWN:
Got expected error: Command not supported by pluggable protocols (code 3130)
RUN CALL xtest.uninstall_plugin()
While executing CALL xtest.uninstall_plugin():
Got expected error: Command not supported by pluggable protocols (code 3130)
RUN CALL xtest.install_plugin()
While executing CALL xtest.install_plugin():
Got expected error: Command not supported by pluggable protocols (code 3130)
RUN CALL xtest.shutdown_server()
While executing CALL xtest.shutdown_server():
Got expected error: Command not supported by pluggable protocols (code 3130)
RUN DROP SCHEMA IF EXISTS xtest

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
