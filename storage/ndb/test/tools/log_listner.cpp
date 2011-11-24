/*
   Copyright (c) 2007 MySQL AB
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <mgmapi.h>
#include <ndb_global.h>
#include <ndb_opts.h>
#include <NDBT.hpp>

NDB_STD_OPTS_VARS;

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_logevent_listen"),
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};
static void usage()
{
  char desc[] = 
    "tabname\n"\
    "This program list all properties of table(s) in NDB Cluster.\n"\
    "  ex: desc T1 T2 T4\n";
  ndb_std_print_version();
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}

int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_BACKUP,
		 15, NDB_MGM_EVENT_CATEGORY_CONNECTION,
		 15, NDB_MGM_EVENT_CATEGORY_NODE_RESTART,
		 15, NDB_MGM_EVENT_CATEGORY_STARTUP,
		 15, NDB_MGM_EVENT_CATEGORY_SHUTDOWN,
		 15, NDB_MGM_EVENT_CATEGORY_STATISTIC,
		 15, NDB_MGM_EVENT_CATEGORY_ERROR,
		 15, NDB_MGM_EVENT_CATEGORY_CHECKPOINT,
		 15, NDB_MGM_EVENT_CATEGORY_CONGESTION,
		 0 };

int 
main(int argc, char** argv)
{
  NDB_INIT(argv[0]);
  const char *load_default_groups[]= { "mysql_cluster",0 };
  load_defaults("my",load_default_groups,&argc,&argv);
  int ho_error;
#ifndef DBUG_OFF
  opt_debug= "d:t:O,/tmp/ndb_desc.trace";
#endif
  if ((ho_error=handle_options(&argc, &argv, my_long_options, 
			       ndb_std_get_one_option)))
    return NDBT_ProgramExit(NDBT_WRONGARGS);

  NdbMgmHandle handle= ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(handle, opt_connect_str);
  
  while (true)
  {
    if (ndb_mgm_connect(handle,0,0,0) == -1)
    {
      ndbout_c("Failed to connect");
      exit(0);
    }
    
    NdbLogEventHandle le = ndb_mgm_create_logevent_handle(handle, filter);
    if (le == 0)
    {
      ndbout_c("Failed to create logevent handle");
      exit(0);
    }
    
    struct ndb_logevent event;
    while (true)
    {
      int r= ndb_logevent_get_next(le, &event,5000);
      if (r < 0)
      {
	ndbout_c("Error while getting next event");
	break;
      }
      if (r == 0)
      {
	continue;
      }
      ndbout_c("Got event: %d", event.type);
    }
    
    ndb_mgm_destroy_logevent_handle(&le);
    ndb_mgm_disconnect(handle);
  }

  return 0;
}
