/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "gcs_plugin.h"
#include "observer_server_state.h"
#include "observer_trans.h"
#include <sql_class.h>                          // THD
#include <log.h>
#include <gcs_replication.h>
#include <gcs_protocol.h>
#include <gcs_protocol_factory.h>
#include <pthread.h>

using std::string;


static MYSQL_PLUGIN plugin_info_ptr;

/* configuration related: */
ulong gcs_protocol_opt;
const char *gcs_protocol_names[]= {"COROSYNC", NullS};
TYPELIB gcs_protocol_typelib=
{ array_elements(gcs_protocol_names) - 1, "", gcs_protocol_names, NULL };

char gcs_replication_group[UUID_LENGTH+1];
char gcs_replication_boot;
/* end of conf */

char *gcs_group_pointer=NULL;

class Mutex_autolock
{
public:
  Mutex_autolock(pthread_mutex_t *arg) : ptr_mutex(arg)
  {
    DBUG_ENTER("Mutex_autolock::Mutex_autolock");

    DBUG_ASSERT(arg != NULL);

    pthread_mutex_lock(ptr_mutex);
    DBUG_VOID_RETURN;
  }
  ~Mutex_autolock()
  {
      pthread_mutex_unlock(ptr_mutex);
  }

private:
  pthread_mutex_t *ptr_mutex;
  Mutex_autolock(Mutex_autolock const&); // no copies permitted
  void operator=(Mutex_autolock const&);
};

static pthread_mutex_t gcs_running_mutex;
static bool gcs_running;

rpl_sidno gcs_cluster_sidno;

static GCS::Protocol *gcs_instance= NULL; // Specific/conf-ed GCS protocol

namespace GCS
{

/* (unit) testing / experimental  declarations */

void handle_view_change(View& view, Member_set& totl,
                        Member_set& left, Member_set& joined, bool quorate);
void handle_message_delivery(Message *msg, const View& view);
Event_handlers default_event_handlers=
{
  handle_view_change,
  handle_message_delivery
};

}

/*
  Internal auxiliary functions signatures.
*/
static int check_group_name_string(const char *str);

static bool init_cluster_sidno();


/*
  Plugin interface.
*/
struct st_mysql_gcs_rpl gcs_rpl_descriptor =
{
  MYSQL_GCS_REPLICATION_INTERFACE_VERSION,
  gcs_rpl_start,
  gcs_rpl_stop
};

int gcs_rpl_start()
{
  Mutex_autolock a(&gcs_running_mutex);

  DBUG_ENTER("gcs_rpl_start");

  if (gcs_running)
    DBUG_RETURN(2);
  if (check_group_name_string(gcs_group_pointer))
    DBUG_RETURN(1);
  if (init_cluster_sidno())
    DBUG_RETURN(1);


  // TODO: Pedro's applier is to replace the session by its own.

  gcs_instance->open_session(&GCS::default_event_handlers);
  if (!gcs_instance->join(string(gcs_group_pointer)))
    gcs_running= true;
  else
    gcs_instance->close_session();

  /* Protocol_corosync::test_me (a part of unit testing) */
  if (gcs_running && !strcmp(gcs_group_pointer, "00000000-0000-0000-0000-000000000000"))
    gcs_instance->test_me();
  DBUG_RETURN(!gcs_running);
}

int gcs_rpl_stop()
{
  Mutex_autolock a(&gcs_running_mutex);

  DBUG_ENTER("gcs_rpl_stop");

  if (!gcs_running)
    DBUG_RETURN(0);

  /* first leave all joined groups (currently one) */
  gcs_instance->leave(string(gcs_group_pointer));
  gcs_instance->close_session();
  gcs_running= false;

  DBUG_RETURN(0);
}

int gcs_replication_init(MYSQL_PLUGIN plugin_info)
{
  pthread_mutex_init(&gcs_running_mutex, NULL);
  plugin_info_ptr= plugin_info;
  if (init_gcs_rpl())
    return 1;

  if (register_server_state_observer(&server_state_observer, (void *)plugin_info_ptr))
  {
    sql_print_error("Failure in GCS cluster during registering the server state observers");
    return 1;
  }

  if (register_trans_observer(&trans_observer, (void *)plugin_info_ptr))
  {
    sql_print_error("Failure in GCS cluster during registering the transactions state observers");
    return 1;
  }

  if (!(gcs_instance= GCS::Protocol_factory::create_protocol((GCS::Protocol_type)
                                                             gcs_protocol_opt, NULL)))
  {
    sql_print_error("Failure in GCS protocol initialization");
    return 1;
  };

  if (gcs_replication_boot && start_gcs_rpl())
    return 1;

  return 0;
}

int gcs_replication_deinit(void *p)
{
  pthread_mutex_destroy(&gcs_running_mutex);
  if (cleanup_gcs_rpl())
    return 1;

  if (unregister_server_state_observer(&server_state_observer, p))
  {
    sql_print_error("Failure in GCS cluster during unregistering the server state observers");
    return 1;
  }

  if (unregister_trans_observer(&trans_observer, p))
  {
    sql_print_error("Failure in GCS cluster during unregistering the transactions state observers");
    return 1;
  }

  sql_print_information("The observers in GCS cluster have been successfully unregistered");
  return 0;
}

static void update_boot(MYSQL_THD thd, SYS_VAR *var, void *ptr, const void *val)
{
  DBUG_ENTER("update_boot");

  *(char *)ptr= *(char *)val;
  gcs_replication_boot= *((char *) ptr);

  DBUG_VOID_RETURN;
}

static bool init_cluster_sidno()
{
  DBUG_ENTER("init_cluster_sid");
  rpl_sid cluster_sid;

  if (cluster_sid.parse(gcs_group_pointer) != RETURN_STATUS_OK)
    DBUG_RETURN(true);

  gcs_cluster_sidno = get_sidno_from_global_sid_map(cluster_sid);
  if (gcs_cluster_sidno <= 0)
    DBUG_RETURN(true);

  DBUG_RETURN(false);
}

static int check_group_name_string(const char *str)
{
  DBUG_ENTER("check_group_name_string");

  if (!str)
  {
    sql_print_error("The group name option is mandatory");
    DBUG_RETURN(1);
  }
  if (!Uuid::is_valid(str))
  {
    sql_print_error("The group name '%s' is not a valid UUID", str);
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}

static int check_group_name(MYSQL_THD thd, SYS_VAR *var, void* prt,
                            struct st_mysql_value *value)
{
  DBUG_ENTER("check_group_name");

  char buff[NAME_CHAR_LEN];
  const char *str;

  //safe_mutex_assert_owner(&gcs_running_mutex);

  if (gcs_running)
  {
    sql_print_error("The group name cannot be changed when cluster is running");
    DBUG_RETURN(1);
  }

  int length= sizeof(buff);
  str= value->val_str(value, buff, &length);
  if (check_group_name_string(str))
    DBUG_RETURN(1);

  *(const char**)prt= str;
  DBUG_RETURN(0);
}

static void update_group_name(MYSQL_THD thd, SYS_VAR *var, void *ptr, const
                              void *val)
{
  DBUG_ENTER("update_group_name");

  const char *newGroup= *(const char**)val;
  strncpy(gcs_replication_group, newGroup, UUID_LENGTH);
  gcs_group_pointer= &gcs_replication_group[0];

  DBUG_VOID_RETURN;
}

static MYSQL_SYSVAR_BOOL(start_on_boot, gcs_replication_boot,
  PLUGIN_VAR_OPCMDARG,
  "Whether this server should start the group or not during bootstrap.",
  NULL,
  update_boot,
  0);

static MYSQL_SYSVAR_STR(group_name, gcs_group_pointer,
  PLUGIN_VAR_OPCMDARG,
  "The cluster name this server has joined.",
  check_group_name,
  update_group_name,
  NULL);

static MYSQL_SYSVAR_ENUM(gcs_protocol, gcs_protocol_opt,
  PLUGIN_VAR_OPCMDARG,
  "The name of GCS protocol to us.",
  NULL,
  NULL,
  GCS::PROTO_COROSYNC,
  &gcs_protocol_typelib);

static SYS_VAR* gcs_system_vars[]= {
  MYSQL_SYSVAR(group_name),
  MYSQL_SYSVAR(start_on_boot),
  MYSQL_SYSVAR(gcs_protocol),
  NULL,
};

mysql_declare_plugin(gcs_repl_plugin)
{
  MYSQL_GCS_RPL_PLUGIN,
  &gcs_rpl_descriptor,
  "gcs_replication_plugin",
  "ORACLE",
  "GCS replication plugin",
  PLUGIN_LICENSE_GPL,
  gcs_replication_init,   /* Plugin Init */
  gcs_replication_deinit, /* Plugin Deinit */
  0x0100,                 /* 1.0 Plugin version*/
  NULL,                   /* status variables */
  gcs_system_vars,        /* system variables */
  NULL,                   /* config options */
  0,                      /* flags */
}
mysql_declare_plugin_end;


/*******************************************************************
  Testing and experimenting compartment to be replaced
  by actual GCS::Protocol::Event_handlers.

  This section contains template defininitions of @c Event_handlers
  and currently should be used only at testing.

  TODO: remove it at some stage.
********************************************************************/

static ulong received_messages= 0;

namespace GCS
{

/*
  The function is called at View change and
  receives three set of members as arguments:
  one for the being installed view,
  one for left members and the third for joined ones.

  Using that info this function implements
  a prototype of Node manager that checks quorate condition
  and terminates this intance membership when it does not hold.

  This definition is only for testing.
*/
void handle_view_change(View& view, Member_set& totl,
                        Member_set& left, Member_set& joined, bool quorate)
{
  if (!strcmp(gcs_group_pointer, "00000000-0000-0000-0000-000000000000"))
    sql_print_warning("GCS dummy_test_cluster: received View change. "
                    "Current # of members %d, Left %d, Joined %d",
                      (int) totl.size(), (int) left.size(), (int) joined.size());
  if (!quorate)
    gcs_rpl_stop();
}

/*
  The function is invoked whenever a message is delivered from a group.

  @param msg     pointer to Message object
  @param ptr_v   pointer to the View in which delivery is happening
*/
void handle_message_delivery(Message *msg, const View& view)
{
  if (!strcmp(gcs_group_pointer, "00000000-0000-0000-0000-000000000000"))
  {
    // report each 100th message
    if (++received_messages % 100 == 0)
      sql_print_warning("GCS dummy_test_cluster: received %lu:th message",
                        received_messages);
  }
};

/*
  Protocol non-specific GCS event handler vector to be associated
  with a protocol session.
*/
} // namespace



