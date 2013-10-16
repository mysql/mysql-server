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
#include <gcs_replication.h>
#include <gcs_protocol.h>
#include <gcs_protocol_factory.h>
#include <pthread.h>
#include "gcs_event_handlers.h"


using std::string;

static MYSQL_PLUGIN plugin_info_ptr;

/* configuration related: */
ulong gcs_protocol_opt;
const char *gcs_protocol_names[]= {"COROSYNC", NullS};
TYPELIB gcs_protocol_typelib=
{ array_elements(gcs_protocol_names) - 1, "", gcs_protocol_names, NULL };

char gcs_replication_group[UUID_LENGTH+1];
char gcs_replication_boot;
ulong handler_pipeline_type;
bool wait_on_engine_initialization= false;
ulong gcs_applier_thread_timeout= LONG_TIMEOUT;
rpl_sidno gcs_cluster_sidno;
/* end of conf */

//The plugin running flag and lock
static pthread_mutex_t gcs_running_mutex;
static bool gcs_running;

//The plugin applier
Applier_module *applier= NULL;

char *gcs_group_pointer=NULL;

// Specific/configured GCS protocol
static GCS::Protocol *gcs_instance= NULL;

GCS::Event_handlers gcs_plugin_event_handlers=
{
  handle_view_change,
  handle_message_delivery
};

/*
  Internal auxiliary functions signatures.
*/
static int check_group_name_string(const char *str);

static bool init_cluster_sidno();

static bool server_engine_initialized();

/*
  Auxiliary public functions.
*/
bool is_gcs_rpl_running()
{
  return gcs_running;
}

int log_message(enum plugin_log_level level, const char *format, ...)
{
  va_list args;
  char buff[1024];

  va_start(args, format);
  my_vsnprintf(buff, sizeof(buff), format, args);
  va_end(args);
  return my_plugin_log_message(&plugin_info_ptr, level, buff);
}

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

  if (is_gcs_rpl_running())
    DBUG_RETURN(GCS_ALREADY_RUNNING);
  if (check_group_name_string(gcs_group_pointer))
    DBUG_RETURN(GCS_CONFIGURATION_ERROR);
  if (init_cluster_sidno())
    DBUG_RETURN(GCS_CONFIGURATION_ERROR);
  if (server_engine_initialized())
  {
    //we can only start the applier if the log has been initialized
    if (configure_and_start_applier())
      DBUG_RETURN(GCS_REPLICATION_APPLIER_INIT_ERROR);
  }
  else
  {
    wait_on_engine_initialization= true;
    DBUG_RETURN(0); //leave the decision for later
  }

  int error= 0;
  if ((error= configure_and_start_gcs()))
  {
    //terminate the before created pipeline
    log_message(MY_ERROR_LEVEL,
                "Error on gcs initialization methods, killing the applier");
    applier->terminate_applier_thread();
    DBUG_RETURN(error);
  }

  gcs_running= true;
  DBUG_RETURN(0); //All is OK
}

int gcs_rpl_stop()
{
  Mutex_autolock a(&gcs_running_mutex);
  DBUG_ENTER("gcs_rpl_stop");

  if (!is_gcs_rpl_running())
    DBUG_RETURN(0);

  int error= 0;
  if (applier != NULL)
  {
    if (!applier->terminate_applier_thread()) //all goes fine
    {
      delete applier;
      applier= NULL;
    }
    else
    {
      /*
        Let gcs_running be false as the applier thread can terminate in the
        meanwhile.
      */
      error= ER_STOP_GCS_APPLIER_THREAD_TIMEOUT;
    }
  }

  /* first leave all joined groups (currently one) */
  gcs_instance->leave(string(gcs_group_pointer));
  gcs_instance->close_session();
  gcs_running= false;

  DBUG_RETURN(error);
}

int gcs_replication_init(MYSQL_PLUGIN plugin_info)
{
  pthread_mutex_init(&gcs_running_mutex, NULL);
  plugin_info_ptr= plugin_info;
  if (init_gcs_rpl())
    return 1;

  if (register_server_state_observer(&server_state_observer, (void *)plugin_info_ptr))
  {
    log_message(MY_ERROR_LEVEL,
                "Failure in GCS cluster during registering the server state observers");
    return 1;
  }

  if (register_trans_observer(&trans_observer, (void *)plugin_info_ptr))
  {
    log_message(MY_ERROR_LEVEL,
                "Failure in GCS cluster during registering the transactions state observers");
    return 1;
  }

  if (!(gcs_instance= GCS::Protocol_factory::create_protocol((GCS::Protocol_type)
                                                             gcs_protocol_opt, NULL)))
  {
    log_message(MY_ERROR_LEVEL, "Failure in GCS protocol initialization");
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
    log_message(MY_ERROR_LEVEL,
                "Failure in GCS cluster during unregistering the server state observers");
    return 1;
  }

  if (unregister_trans_observer(&trans_observer, p))
  {
    log_message(MY_ERROR_LEVEL,
                "Failure in GCS cluster during unregistering the transactions state observers");
    return 1;
  }

  log_message(MY_INFORMATION_LEVEL,
              "The observers in GCS cluster have been successfully unregistered");
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

void declare_plugin_running()
{
  gcs_running= true;
}

int configure_and_start_applier()
{
  DBUG_ENTER("configure_and_start_applier");

  int error= 0;

  //The applier did not stop properly or suffered a configuration error
  if (applier != NULL)
  {
    if ((error= applier->is_running())) //it is still running?
    {
      log_message(MY_ERROR_LEVEL,
                  "Cannot start the applier as a previous shutdown is still "
                  "running: The thread will stop once its task is complete.");
      DBUG_RETURN(error);
    }
    else
    {
      //clean a possible existent pipeline
      applier->terminate_applier_pipeline();
      //delete it and create from scratch
      delete applier;
    }
  }

  applier= new Applier_module();

  //For now, only defined pipelines are accepted.
  error=
    applier->setup_applier_module((Handler_pipeline_type)handler_pipeline_type,
                                  gcs_applier_thread_timeout);
  if (error)
    DBUG_RETURN(error);

  if ((error= applier->initialize_applier_thread()))
  {
    log_message(MY_ERROR_LEVEL, "Unable to initialize the plugin applier module!");
    //clean a possible existent pipeline
    applier->terminate_applier_pipeline();
    delete applier;
    applier= NULL;
  }
  else
    log_message(MY_INFORMATION_LEVEL,
                "Event applier module successfully initialized!");

  DBUG_RETURN(error);
}

int configure_and_start_gcs()
{
  if (gcs_instance->open_session(&gcs_plugin_event_handlers))
    return GCS_COMMUNICATION_LAYER_SESSION_ERROR;

  if (gcs_instance->join(string(gcs_group_pointer)))
  {
    gcs_instance->close_session();
    return GCS_COMMUNICATION_LAYER_JOIN_ERROR;
  }
  return 0;
}

static bool server_engine_initialized(){
  return is_server_engine_ready();
}

static int check_group_name_string(const char *str)
{
  DBUG_ENTER("check_group_name_string");

  if (!str)
  {
    log_message(MY_ERROR_LEVEL, "The group name option is mandatory");
    DBUG_RETURN(1);
  }
  if (!Uuid::is_valid(str))
  {
    log_message(MY_ERROR_LEVEL, "The group name '%s' is not a valid UUID", str);
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
  if (is_gcs_rpl_running())
  {
    log_message(MY_ERROR_LEVEL,
                "The group name cannot be changed when cluster is running");
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

static void update_applier_timeout(MYSQL_THD thd, SYS_VAR *var, void *ptr,
                                   const void *value)
{
  DBUG_ENTER("update_applier_timeout");

  ulong in_val= *static_cast<const ulong*>(value);
  gcs_applier_thread_timeout= in_val;

  if (applier != NULL)
  {
    applier->set_stop_wait_timeout(gcs_applier_thread_timeout);
  }

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

static const char* pipeline_names[]= { "STANDARD", NullS};

static TYPELIB pipeline_name_typelib_t= {
         array_elements(pipeline_names) - 1,
         "pipeline_name_typelib_t",
         pipeline_names,
         NULL
 };

static MYSQL_SYSVAR_ENUM(pipeline_type_var, handler_pipeline_type,
   PLUGIN_VAR_OPCMDARG,
   "pipeline types"
   "possible values are STANDARD",
   NULL, NULL, STANDARD_GCS_PIPELINE, &pipeline_name_typelib_t);

static MYSQL_SYSVAR_ULONG(
  stop_applier_timeout,  /* name */
  gcs_applier_thread_timeout,        /* var */
  PLUGIN_VAR_OPCMDARG,               /* optional var */
  "Timeout in seconds to wait for applier to stop before returning a warning.",
  NULL,                              /* check func. */
  update_applier_timeout,            /* update func. */
  LONG_TIMEOUT,                      /* default */
  2,                                 /* min */
  LONG_TIMEOUT,                      /* max */
  0                                  /* block */
);

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
  MYSQL_SYSVAR(pipeline_type_var),
  MYSQL_SYSVAR(stop_applier_timeout),
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
