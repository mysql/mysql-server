#include <my_global.h>
#include <my_sys.h>
#include <mysql.h>

/*
  Currently we cannot use libmysqlclient directly becouse of the linking
  issues. Here we provide needed libmysqlclient functions.
  TODO: to think how to use libmysqlclient code instead of copy&paste.
  The other possible solution is to use simple_command directly.
*/

const char * STDCALL
mysql_get_server_info(MYSQL *mysql)
{
  return((char*) mysql->server_version);
}

int STDCALL
mysql_ping(MYSQL *mysql)
{
  DBUG_ENTER("mysql_ping");
  DBUG_RETURN(simple_command(mysql,COM_PING,0,0,0));
}

int STDCALL
mysql_shutdown(MYSQL *mysql, enum mysql_enum_shutdown_level shutdown_level)
{
  uchar level[1];
  DBUG_ENTER("mysql_shutdown");
  level[0]= (uchar) shutdown_level;
  DBUG_RETURN(simple_command(mysql, COM_SHUTDOWN, (char *)level, 1, 0));
}
