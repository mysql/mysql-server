# xplugin_create_user.inc - script
#
#  $xplugin_cache_x_root -
#    Cache the X Plugin test user, that it might be used with
#    unencrpted connections.
#  $xplugin_create_user_cache -
#    Define the autnetication should be done, to be successful,
#    and fill the user cache (default SSL). If this parameter
#    is set to USOCKET, then unix socket is going to be used.
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

CREATE USER 'x_root' @'localhost' IDENTIFIED WITH 'caching_sha2_password';
GRANT ALL ON *.* TO 'x_root' @'localhost' WITH GRANT OPTION;

if ($create_user_disable_binlog)
{
  SET SESSION sql_log_bin = 1;
}

if ($xplugin_cache_x_root)
{
  if ($xplugin_create_user_cache==USOCKET)
  {
    --exec $MYSQLXTEST -ux_root --socket=$MASTER_X_MYSOCK -e ';' >/dev/null 2>&1
  }
  if ($xplugin_create_user_cache!=USOCKET)
  {
    --exec $MYSQLXTEST -ux_root --ssl-mode=REQUIRED -e ";" >/dev/null 2>&1
  }
  source xplugin_reset_global_status.inc;
}

--let xplugin_create_user=1

--enable_query_log
