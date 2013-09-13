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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include "./include/gcs_plugin.h"
#include <sql_class.h>                          // THD
#include <replication.h>
#include <log.h>
#include <gcs_replication.h>

/**
  Handle assigned when loading the plugin.
  Used with the error reporting functions.
*/

static MYSQL_PLUGIN plugin_info_ptr;
extern char gcs_replication_group[NAME_CHAR_LEN];
extern char gcs_replication_boot;
char *gcs_group_pointer=NULL;
bool gcs_running= false;

int gcs_before_handle_conncection(Server_state_param *param)
{
  return 0;
}

int gcs_before_recovery(Server_state_param *param)
{
  return 0;
}

int gcs_after_engine_recovery(Server_state_param *param)
{
  return 0;
}

int gcs_after_recovery(Server_state_param *param)
{
  return 0;
}

int gcs_before_server_shutdown(Server_state_param *param)
{
  return 0;
}

int gcs_after_server_shutdown(Server_state_param *param)
{
  return 0;
}

Server_state_observer server_state_observer = {
  sizeof(Server_state_observer),

  gcs_before_handle_conncection, //before the client connect the node
  gcs_before_recovery,           //before_recovery
  gcs_after_engine_recovery,     //after engine recovery
  gcs_after_recovery,            //after_recovery
  gcs_before_server_shutdown,    //before shutdown
  gcs_after_server_shutdown,     //after shutdown
};

int gcs_trans_before_commit(Trans_param *param)
{
  DBUG_ENTER("gcs_trans_before_commit");
  DBUG_RETURN(0);
}

int gcs_trans_before_rollback(Trans_param *param)
{
  DBUG_ENTER("gcs_trans_before_rollback");
  DBUG_RETURN(0);
}

int gcs_trans_after_commit(Trans_param *param)
{
  DBUG_ENTER("gcs_trans_after_commit");
  DBUG_RETURN(0);
}

int gcs_trans_after_rollback(Trans_param *param)
{
  DBUG_ENTER("gcs_trans_after_rollback");
  DBUG_RETURN(0);
}


Trans_observer trans_observer = {
  sizeof(Trans_observer),

  gcs_trans_before_commit,
  gcs_trans_before_rollback,
  gcs_trans_after_commit,
  gcs_trans_after_rollback,
};

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
  if (strcmp(gcs_group_pointer, "NULL") == 0)
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
    return 1;

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

static int check_group_name(MYSQL_THD thd, SYS_VAR *var, void* prt,
                            struct st_mysql_value *value)
{
  DBUG_ENTER("check_group_name");

  char buff[NAME_CHAR_LEN];
  const char *str;

  int length= sizeof(buff);
  str= value->val_str(value, buff, &length);
  if (!str)
    DBUG_RETURN(1);
  if (length > NAME_CHAR_LEN-1)
  {
    sql_print_error("The group name '%s' is too long, it can have a maximum of %d characters.",
                    str, NAME_CHAR_LEN-1);
    DBUG_RETURN(1);
  }

  *(const char**)prt= str;
  DBUG_RETURN(0);
}

static void update_group_name(MYSQL_THD thd, SYS_VAR *var, void *ptr, const
                              void *val)
{
  DBUG_ENTER("update_group_name");

  const char *newGroup= *(const char**)val;
  strncpy(gcs_replication_group, newGroup, NAME_CHAR_LEN-1);
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
  "NULL");

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
