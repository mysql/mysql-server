#include "plugin.h"
#include "com_data.h"
struct st_mysql_sqlshim
{
  int interface_version;
  my_bool (*shim_function)(MYSQL_THD, union COM_DATA*, enum enum_server_command, union COM_DATA*, enum enum_server_command*);
};
