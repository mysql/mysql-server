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


static MYSQL_PLUGIN plugin_info_ptr;
char gcs_replication_group[UUID_LENGTH+1];
rpl_sidno gcs_cluster_sidno;
char gcs_replication_boot;
char *gcs_group_pointer=NULL;
bool gcs_running= false;


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
  if (gcs_running)
    return 2;
  if (check_group_name_string(gcs_group_pointer))
    return 1;
  if (init_cluster_sidno())
    return 1;
  gcs_running= true;
  return 0;
}

int gcs_rpl_stop()
{
  gcs_running= false;
  return 0;
}

int gcs_replication_init(MYSQL_PLUGIN plugin_info)
{
  plugin_info_ptr= plugin_info;
  if (init_gcs_rpl())
    return 1;

  if(register_server_state_observer(&server_state_observer, (void *)plugin_info_ptr))
  {
    sql_print_error("Failure on GCS cluster during registering the server state observers");
    return 1;
  }

  if (register_trans_observer(&trans_observer, (void *)plugin_info_ptr))
  {
    sql_print_error("Failure on GCS cluster during registering the transactions state observers");
    return 1;
  }

  if (gcs_replication_boot)
  {
    if (start_gcs_rpl())
      return 1;
    gcs_running= true;
  }

  return 0;
}

int gcs_replication_deinit(void *p)
{
  if (cleanup_gcs_rpl())
    return 1;

  if (unregister_server_state_observer(&server_state_observer, p))
  {
    sql_print_error("Failure on GCS cluster during unregistering the server state observers");
    return 1;
  }

  if (unregister_trans_observer(&trans_observer, p))
  {
    sql_print_error("Failure on GCS cluster during unregistering the transactions state observers");
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

static SYS_VAR* gcs_system_vars[]= {
  MYSQL_SYSVAR(group_name),
  MYSQL_SYSVAR(start_on_boot),
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
