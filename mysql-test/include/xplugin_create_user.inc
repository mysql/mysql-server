#==== Usage ====
#
#[--let $create_user_disable_binlog = 1]
#-- source include/xplugin_create_user.inc
#

--disable_query_log

if ($create_user_disable_binlog)
{
  SET SESSION sql_log_bin = 0;
}

CREATE USER 'x_root' @'localhost' IDENTIFIED WITH 'mysql_native_password';
GRANT ALL ON *.*TO 'x_root' @'localhost' WITH GRANT OPTION;

if ($create_user_disable_binlog)
{
  SET SESSION sql_log_bin = 1;
}

--enable_query_log
