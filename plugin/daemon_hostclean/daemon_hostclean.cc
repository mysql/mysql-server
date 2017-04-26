#include <my_global.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <mysql_version.h>
#include <mysql/plugin.h>
#include <my_dir.h>
#include "my_thread.h"
#include "sql_plugin.h"                         // st_plugin_int
#include "hostname.h"
#include "violite.h"

my_thread_handle hostclean_thread;

void *mysql_hostclean(void *p)
{
  Host_entry *first, *current;
  uint size;
  uint index;

  while(1)
  {
    sleep(10);

    hostname_cache_lock();
    index = 0;
    int old_entries = 0;
    size = hostname_cache_size();
    first = hostname_cache_first();
    current = first;

    while ((current != NULL) && (index < size))
    {
      char hostname_buffer[NI_MAXHOST];
      struct sockaddr_in sa;
      inet_pton(AF_INET, current->ip_key, &sa.sin_addr);
      sa.sin_family = AF_INET;
      int r;
      if ((r = vio_getnameinfo((struct sockaddr*) &sa, hostname_buffer,
        NI_MAXHOST, NULL, 0, NI_NAMEREQD)) == 0) {
        if (current->m_hostname_length > 0){
          if (strcmp(hostname_buffer, current->m_hostname) != 0) {
              old_entries++;
          }
        }
      }
      index++;
      current= current->next();
    }
    hostname_cache_unlock();

    if (old_entries > 0) {
      hostname_cache_refresh();
    }
  }
}

static int daemon_hostclean_plugin_init(void *p)
{
  DBUG_ENTER("daemon_hostclean_plugin_init");

  my_thread_attr_t attr;

  my_thread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  if (my_thread_create(&hostclean_thread, &attr, mysql_hostclean, 0) != 0)
  {
    DBUG_PRINT("error", ("Could not create hostclean thread"));
    exit(0);
  }

  DBUG_RETURN(0);
}


static int daemon_hostclean_plugin_deinit(void *p)
{
  DBUG_ENTER("daemon_hostclean_plugin_deinit");

  my_thread_cancel(&hostclean_thread);

  DBUG_RETURN(0);
}


struct st_mysql_daemon daemon_hostclean_plugin=
{ MYSQL_DAEMON_INTERFACE_VERSION  };


mysql_declare_plugin(daemon_hostclean)
{
  MYSQL_DAEMON_PLUGIN,
  &daemon_hostclean_plugin,
  "daemon_hostclean",
  "Daniël van Eeden",             /* Author            */
  "Daemon which periodically checks the data in the host_cache",
  PLUGIN_LICENSE_GPL,             /* License           */
  daemon_hostclean_plugin_init,   /* Plugin Init       */
  daemon_hostclean_plugin_deinit, /* Plugin Deinit     */
  0x0001,                         /* 0.1               */
  NULL,                           /* status variables  */
  NULL,                           /* system variables  */
  NULL,                           /* config options    */
  0,                              /* flags             */
}
mysql_declare_plugin_end;
