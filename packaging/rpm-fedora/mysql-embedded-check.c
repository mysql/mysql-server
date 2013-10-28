/* simple test program to see if we can link the embedded server library */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "mysql.h"

MYSQL *mysql;

static char *server_options[] = \
       { "mysql_test", "--defaults-file=my.cnf", NULL };
int num_elements = (sizeof(server_options) / sizeof(char *)) - 1;

static char *server_groups[] = { "libmysqld_server", 
                                 "libmysqld_client", NULL };

int main(int argc, char **argv)
{
   mysql_library_init(num_elements, server_options, server_groups);
   mysql = mysql_init(NULL);
   mysql_close(mysql);
   mysql_library_end();

   return 0;
}
