/* Copyright (c) 2007, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_audit.h"

#include "current_thd.h"
#include "log.h"
#include "mysqld.h"                             // sql_statement_names
#include "sql_class.h"                          // THD
#include "sql_plugin.h"                         // my_plugin_foreach
#include "sql_rewrite.h"                        // mysql_rewrite_query

struct st_mysql_event_generic
{
  mysql_event_class_t event_class;
  const void *event;
};

/**
  @struct st_mysql_subscribe_event

  Plugin event subscription structure.
*/
struct st_mysql_subscribe_event
{
  /* Event class. */
  mysql_event_class_t event_class;
  /* Event subclass. */
  unsigned long       event_subclass;
  /* Event subscription masks. */
  unsigned long       *subscribe_mask;
};


unsigned long mysql_global_audit_mask[MYSQL_AUDIT_CLASS_MASK_SIZE];

static mysql_mutex_t LOCK_audit_mask;

static int event_class_dispatch(THD *thd, mysql_event_class_t event_class,
                                const void *event);

static int event_class_dispatch_error(THD *thd,
                                      mysql_event_class_t event_class,
                                      const char *event_name,
                                      const void *event);

static inline
void add_audit_mask(unsigned long *mask, const unsigned long *rhs)
{
  mask[0]|= rhs[0];
}

static inline
bool check_audit_mask(const unsigned long *lhs,
                      const unsigned long *rhs)
{
  return !(lhs[0] & rhs[0]);
}

/**
  Fill query and query charset info extracted from the thread object.

  @param[in]  thd     Thread data.
  @param[out] query   SQL query text.
  @param[out] charset SQL query charset.
*/
inline
void thd_get_audit_query(THD *thd, MYSQL_LEX_CSTRING *query,
                         const struct charset_info_st **charset)
{
  if (!thd->rewritten_query.length())
    mysql_rewrite_query(thd);

  if (thd->rewritten_query.length())
  {
    query->str= thd->rewritten_query.ptr();
    query->length= thd->rewritten_query.length();
    *charset= thd->rewritten_query.charset();
  }
  else
  {
    query->str= thd->query().str;
    query->length= thd->query().length;
    *charset= thd->charset();
  }
}

int mysql_audit_notify(THD *thd, mysql_event_general_subclass_t subclass,
                       int error_code, const char *msg, size_t msg_len)
{
  mysql_event_general event;

  if (mysql_audit_acquire_plugins(thd, MYSQL_AUDIT_GENERAL_CLASS,
                                  static_cast<unsigned long>(subclass)))
    return 0;

  event.event_subclass= subclass;
  event.general_error_code= error_code;
  event.general_thread_id= thd ? thd->thread_id() : 0;
  if (thd)
  {
    char user_buff[MAX_USER_HOST_SIZE];
    Security_context *sctx= thd->security_context();

    event.general_user.str= user_buff;
    event.general_user.length= make_user_name(thd->security_context(), user_buff);
    event.general_ip= sctx->ip();
    event.general_host= sctx->host();
    event.general_external_user= sctx->external_user();
    event.general_rows= thd->get_stmt_da()->current_row_for_condition();
    event.general_sql_command= sql_statement_names[thd->lex->sql_command];

    thd_get_audit_query(thd, &event.general_query,
                        (const charset_info_st**)&event.general_charset);

    event.general_time= thd->start_time.tv_sec;
  }
  else
  {
    static MYSQL_LEX_CSTRING empty={ C_STRING_WITH_LEN("") };

    event.general_user.str= NULL;
    event.general_user.length= 0;
    event.general_ip= empty;
    event.general_host= empty;
    event.general_external_user= empty;
    event.general_rows= 0;
    event.general_sql_command= empty;
    event.general_query.str= "";
    event.general_query.length= 0;
    event.general_time= my_time(0);
  }

  event.general_command.str= msg;
  event.general_command.length= msg_len;

  return event_class_dispatch(thd, MYSQL_AUDIT_GENERAL_CLASS, &event);
}

int mysql_audit_notify(THD *thd, mysql_event_connection_subclass_t subclass,
                       const char* subclass_name, int errcode)
{
  mysql_event_connection event;

  if (mysql_audit_acquire_plugins(thd, MYSQL_AUDIT_CONNECTION_CLASS,
                                  static_cast<unsigned long>(subclass)))
    return 0;

  event.event_subclass= subclass;
  event.status= errcode;
  event.connection_id= thd->thread_id();
  event.user.str= thd->security_context()->user().str;
  event.user.length= thd->security_context()->user().length;
  event.priv_user.str= thd->security_context()->priv_user().str;
  event.priv_user.length= thd->security_context()->priv_user().length;
  event.external_user.str= thd->security_context()->external_user().str;
  event.external_user.length= thd->security_context()->external_user().length;
  event.proxy_user.str= thd->security_context()->proxy_user().str;
  event.proxy_user.length= thd->security_context()->proxy_user().length;
  event.host.str= thd->security_context()->host().str;
  event.host.length= thd->security_context()->host().length;
  event.ip.str= thd->security_context()->ip().str;
  event.ip.length= thd->security_context()->ip().length;
  event.database.str= thd->db().str;
  event.database.length= thd->db().length;

  /* Keep this for backward compatibility. */
  event.connection_type= subclass == MYSQL_AUDIT_CONNECTION_CONNECT ?
                         thd->get_vio_type() : NO_VIO_TYPE;

  if (subclass == MYSQL_AUDIT_CONNECTION_DISCONNECT)
    /* Disconnect abort should not be allowed. */
    return event_class_dispatch(thd, MYSQL_AUDIT_CONNECTION_CLASS, &event);

  return event_class_dispatch_error(thd, MYSQL_AUDIT_CONNECTION_CLASS,
                                    subclass_name, &event);
}

int mysql_audit_notify(THD *thd, mysql_event_connection_subclass_t subclass,
                       const char* subclass_name)
{
  return mysql_audit_notify(thd, subclass, subclass_name,
                            thd->get_stmt_da()->is_error() ?
                            thd->get_stmt_da()->mysql_errno() : 0);
}

int mysql_audit_notify(THD *thd, mysql_event_parse_subclass_t subclass,
                       const char* subclass_name,
                       mysql_event_parse_rewrite_plugin_flag *flags,
                       LEX_CSTRING *rewritten_query)
{
  mysql_event_parse event;

  if (mysql_audit_acquire_plugins(thd, MYSQL_AUDIT_PARSE_CLASS, subclass))
    return 0;

  event.event_subclass= subclass;
  event.flags= flags;
  event.query.str= thd->query().str;
  event.query.length= thd->query().length;
  event.rewritten_query= rewritten_query;

  return event_class_dispatch_error(thd, MYSQL_AUDIT_PARSE_CLASS,
                                    subclass_name, &event);
}

/*
  Function commented out. No Audit API calls yet.

int mysql_audit_notify(THD *thd, mysql_event_table_access_subclass_t subclass,
                       const char *subclass_name,
                       const char *database, const char *table)
{
  LEX_CSTRING str;
  mysql_event_table_access event;

  mysql_audit_acquire_plugins(thd, MYSQL_AUDIT_TABLE_ACCESS_CLASS,
                              static_cast<unsigned long>(subclass));

  event.event_subclass= subclass;
  event.connection_id= thd->thread_id();
  event.sql_command_id= thd->lex->sql_command;

  thd_get_audit_query(thd, &event.query, &event.query_charset);

  lex_cstring_set(&str, database);
  event.table_database.str= str.str;
  event.table_database.length= str.length;

  lex_cstring_set(&str, table);
  event.table_name.str= str.str;
  event.table_name.length= str.length;

  return event_class_dispatch_error(thd, MYSQL_AUDIT_TABLE_ACCESS_CLASS,
                                    subclass_name, &event);
}
*/

int mysql_audit_notify(THD *thd, mysql_event_global_variable_subclass_t subclass,
                       const char* subclass_name, const char *name,
                       const char *value, const unsigned int value_length)
{
  mysql_event_global_variable event;

  if (mysql_audit_acquire_plugins(thd, MYSQL_AUDIT_GLOBAL_VARIABLE_CLASS,
                                  static_cast<unsigned long>(subclass)))
    return 0;

  event.event_subclass= subclass;
  event.connection_id= thd->thread_id();
  event.sql_command_id= thd->lex->sql_command;

  LEX_CSTRING name_str;
  lex_cstring_set(&name_str, name);
  event.variable_name.str= name_str.str;
  event.variable_name.length= name_str.length;

  event.variable_value.str= value;
  event.variable_value.length= value_length;

  return event_class_dispatch_error(thd, MYSQL_AUDIT_GLOBAL_VARIABLE_CLASS,
                                    subclass_name, &event);
}

int mysql_audit_notify(mysql_event_server_startup_subclass_t subclass,
                       const char **argv,
                       unsigned int argc)
{
  mysql_event_server_startup event;

  if (mysql_audit_acquire_plugins(0, MYSQL_AUDIT_SERVER_STARTUP_CLASS,
                                   static_cast<unsigned long>(subclass)))
    return 0;

  event.event_subclass= subclass;
  event.argv= argv;
  event.argc= argc;

  return event_class_dispatch(0, MYSQL_AUDIT_SERVER_STARTUP_CLASS, &event);
}

int mysql_audit_notify(mysql_event_server_shutdown_subclass_t subclass,
                       mysql_server_shutdown_reason_t reason, int exit_code)
{
  mysql_event_server_shutdown event;

  if (mysql_audit_acquire_plugins(0, MYSQL_AUDIT_SERVER_SHUTDOWN_CLASS,
                                  static_cast<unsigned long>(subclass)))
    return 0;

  event.event_subclass= subclass;
  event.exit_code = exit_code;
  event.reason= reason;

  return event_class_dispatch(0, MYSQL_AUDIT_SERVER_SHUTDOWN_CLASS, &event);
}

/*
Function commented out. No Audit API calls yet.

int mysql_audit_notify(THD *thd, mysql_event_authorization_subclass_t subclass,
                       const char* subclass_name,
                       const char *database, const char *table,
                       const char *object)
{
  mysql_event_authorization event;

  mysql_audit_acquire_plugins(thd, MYSQL_AUDIT_AUTHORIZATION_CLASS,
                              static_cast<unsigned long>(subclass));

  event.event_subclass= subclass;
  event.connection_id= thd->thread_id();
  event.sql_command_id= thd->lex->sql_command;

  thd_get_audit_query(thd, &event.query, &event.query_charset);

  LEX_CSTRING obj_str;

  lex_cstring_set(&obj_str, database ? database : "");
  event.database.str= database;
  event.database.length= obj_str.length;

  lex_cstring_set(&obj_str, table ? table : "");
  event.table.str= table;
  event.table.length = obj_str.length;

  lex_cstring_set(&obj_str, object ? object : "");
  event.object.str= object;
  event.object.length= obj_str.length;

  return event_class_dispatch_error(thd, MYSQL_AUDIT_AUTHORIZATION_CLASS,
                                    subclass_name, &event);
}
*/

int mysql_audit_notify(THD *thd, mysql_event_command_subclass_t subclass,
                       const char* subclass_name, enum_server_command command)
{
  mysql_event_command event;

  if (mysql_audit_acquire_plugins(thd, MYSQL_AUDIT_COMMAND_CLASS,
                                  static_cast<unsigned long>(subclass)))
    return 0;

  event.event_subclass = subclass;
  event.status= thd->get_stmt_da()->is_error() ?
                thd->get_stmt_da()->mysql_errno() : 0;
  event.connection_id = thd && thd->thread_id();
  event.command_id= command;

  if (subclass == MYSQL_AUDIT_COMMAND_END)
    return event_class_dispatch(thd, MYSQL_AUDIT_COMMAND_CLASS, &event);

  return event_class_dispatch_error(thd, MYSQL_AUDIT_COMMAND_CLASS,
                                    subclass_name, &event);
}

int mysql_audit_notify(THD *thd, mysql_event_query_subclass_t subclass,
                       const char* subclass_name)
{
  mysql_event_query event;

  if (mysql_audit_acquire_plugins(thd, MYSQL_AUDIT_QUERY_CLASS,
                                  static_cast<unsigned long>(subclass)))
    return 0;

  event.event_subclass = subclass;
  event.status = thd->get_stmt_da()->is_error() ?
                 thd->get_stmt_da()->mysql_errno() : 0;
  event.connection_id = thd->thread_id();

  event.sql_command_id= thd->lex->sql_command;

  thd_get_audit_query(thd, &event.query, &event.query_charset);

  return event_class_dispatch_error(thd, MYSQL_AUDIT_QUERY_CLASS,
                                    subclass_name, &event);
}

int mysql_audit_notify(THD *thd,
                       mysql_event_stored_program_subclass_t subclass,
                       const char *subclass_name,
                       const char *database, const char *name,
                       void * parameters)
{
  mysql_event_stored_program event;

  if (mysql_audit_acquire_plugins(thd, MYSQL_AUDIT_STORED_PROGRAM_CLASS,
                                  static_cast<unsigned long>(subclass)))
    return 0;

  event.event_subclass = subclass;
  event.connection_id= thd->thread_id();
  event.sql_command_id= thd->lex->sql_command;

  thd_get_audit_query(thd, &event.query, &event.query_charset);

  LEX_CSTRING obj_str;

  lex_cstring_set(&obj_str, database ? database : "");
  event.database.str= obj_str.str;
  event.database.length= obj_str.length;

  lex_cstring_set(&obj_str, name ? name : "");
  event.name.str= obj_str.str;
  event.name.length= obj_str.length;

  event.parameters= parameters;

  return event_class_dispatch_error(thd, MYSQL_AUDIT_STORED_PROGRAM_CLASS,
                                    subclass_name, &event);
}

/**
  Acquire and lock any additional audit plugins as required
  
  @param[in] thd
  @param[in] plugin
  @param[in] arg

  @retval FALSE Always  
*/

static my_bool acquire_plugins(THD *thd, plugin_ref plugin, void *arg)
{
  st_mysql_subscribe_event *evt = (st_mysql_subscribe_event *)arg;
  st_mysql_audit *data= plugin_data<st_mysql_audit*>(plugin);
  int i;

  /* Check if this plugin is interested in the event */
  if (check_audit_mask(&data->class_mask[evt->event_class],
                       &evt->event_subclass))
    return 0;

  /*
    Check if this plugin may already be registered. This will fail to
    acquire a newly installed plugin on a specific corner case where
    one or more event classes already in use by the calling thread
    are an event class of which the audit plugin has interest.
  */
  if (!check_audit_mask(&data->class_mask[evt->event_class],
                        &thd->audit_class_mask[evt->event_class]))
    return 0;

  /* Copy subscription masks from the plugin into the array. */
  for (i = MYSQL_AUDIT_GENERAL_CLASS; i < MYSQL_AUDIT_CLASS_MASK_SIZE; i++)
    evt->subscribe_mask[i]= data->class_mask[i];
  
  /* lock the plugin and add it to the list */
  plugin= my_plugin_lock(NULL, &plugin);
  thd->audit_class_plugins.push_back(plugin);

  return 0;
}


/**
  @brief Acquire audit plugins

  @param[in]   thd              MySQL thread handle
  @param[in]   event_class      Audit event class

  @details Ensure that audit plugins interested in given event
  class are locked by current thread.
*/
int mysql_audit_acquire_plugins(THD *thd, mysql_event_class_t event_class,
                                 unsigned long event_subclass)
{
  DBUG_ENTER("mysql_audit_acquire_plugins");
  unsigned long global_mask= mysql_global_audit_mask[event_class];

  if (thd && !check_audit_mask(&global_mask, &event_subclass) &&
      check_audit_mask(&thd->audit_class_mask[event_class],
                       &event_subclass))
  {
    /* There is a plugin registered for the subclass, but THD has not
       registered yet for this event. Refresh THD class mask. */

    unsigned long masks[MYSQL_AUDIT_CLASS_MASK_SIZE]= { 0, };
    st_mysql_subscribe_event evt= { event_class, event_subclass, masks };
    int i;
    plugin_foreach(thd, acquire_plugins, MYSQL_AUDIT_PLUGIN, &evt);
    for (i = MYSQL_AUDIT_GENERAL_CLASS; i < MYSQL_AUDIT_CLASS_MASK_SIZE; i++)
      add_audit_mask(&thd->audit_class_mask[i], &evt.subscribe_mask[i]);

    global_mask= thd->audit_class_mask[event_class];
  }

  /* Check whether there is a plugin registered for this event. */
  DBUG_RETURN(check_audit_mask(&global_mask, &event_subclass) ? 1 : 0);
}
 

/**
  Release any resources associated with the current thd.
  
  @param[in] thd

*/

void mysql_audit_release(THD *thd)
{
  plugin_ref *plugins, *plugins_last;
  
  if (!thd || thd->audit_class_plugins.empty())
    return;
  
  plugins= thd->audit_class_plugins.begin();
  plugins_last= thd->audit_class_plugins.end();
  for (; plugins != plugins_last; plugins++)
  {
    st_mysql_audit *data= plugin_data<st_mysql_audit*>(*plugins);
	
    /* Check to see if the plugin has a release method */
    if (!(data->release_thd))
      continue;

    /* Tell the plugin to release its resources */
    data->release_thd(thd);
  }

  /* Now we actually unlock the plugins */  
  plugin_unlock_list(NULL, thd->audit_class_plugins.begin(),
                     thd->audit_class_plugins.size());
  
  /* Reset the state of thread values */
  thd->audit_class_plugins.clear();
  thd->audit_class_mask.clear();
  thd->audit_class_mask.resize(MYSQL_AUDIT_CLASS_MASK_SIZE);
}


/**
  Initialize thd variables used by Audit
  
  @param[in] thd

*/

void mysql_audit_init_thd(THD *thd)
{
  thd->audit_class_mask.clear();
  thd->audit_class_mask.resize(MYSQL_AUDIT_CLASS_MASK_SIZE);
}


/**
  Free thd variables used by Audit
  
  @param thd Current thread
*/

void mysql_audit_free_thd(THD *thd)
{
  mysql_audit_release(thd);
  DBUG_ASSERT(thd->audit_class_plugins.empty());
}

#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_key key_LOCK_audit_mask;

static PSI_mutex_info all_audit_mutexes[]=
{
  { &key_LOCK_audit_mask, "LOCK_audit_mask", PSI_FLAG_GLOBAL}
};

static void init_audit_psi_keys(void)
{
  const char* category= "sql";
  int count;

  count= array_elements(all_audit_mutexes);
  mysql_mutex_register(category, all_audit_mutexes, count);
}
#endif /* HAVE_PSI_INTERFACE */

/**
  Initialize Audit global variables
*/

void mysql_audit_initialize()
{
#ifdef HAVE_PSI_INTERFACE
  init_audit_psi_keys();
#endif

  mysql_mutex_init(key_LOCK_audit_mask, &LOCK_audit_mask, MY_MUTEX_INIT_FAST);
  memset(mysql_global_audit_mask, 0, sizeof(mysql_global_audit_mask));
}


/**
  Finalize Audit global variables  
*/

void mysql_audit_finalize()
{
  mysql_mutex_destroy(&LOCK_audit_mask);
}


/**
  Initialize an Audit plug-in
  
  @param[in] plugin

  @retval FALSE  OK
  @retval TRUE   There was an error.
*/

int initialize_audit_plugin(st_plugin_int *plugin)
{
  st_mysql_audit *data= (st_mysql_audit*) plugin->plugin->info;
  int i;
  unsigned long masks= 0;

  for (i= MYSQL_AUDIT_GENERAL_CLASS; i < MYSQL_AUDIT_CLASS_MASK_SIZE; i++)
  {
    masks|= data->class_mask[i];
  }

  if (data->class_mask[MYSQL_AUDIT_AUTHORIZATION_CLASS])
  {
    sql_print_error("Plugin '%s' cannot subscribe to "
                 "MYSQL_AUDIT_AUTHORIZATION events. Currently not supported.",
                 plugin->name.str);
    return 1;
  }

  if (data->class_mask[MYSQL_AUDIT_TABLE_ACCESS_CLASS])
  {
    sql_print_error("Plugin '%s' cannot subscribe to "
                  "MYSQL_AUDIT_TABLE_ACCESS events. Currently not supported.",
                  plugin->name.str);
    return 1;
  }

  if (!data->event_notify || !masks)
  {
    sql_print_error("Plugin '%s' has invalid data.",
                    plugin->name.str);
    return 1;
  }
  
  if (plugin->plugin->init && plugin->plugin->init(plugin))
  {
    sql_print_error("Plugin '%s' init function returned error.",
                    plugin->name.str);
    return 1;
  }

  /* Make the interface info more easily accessible */
  plugin->data= plugin->plugin->info;
  
  /* Add the bits the plugin is interested in to the global mask */
  mysql_mutex_lock(&LOCK_audit_mask);
  for (i= MYSQL_AUDIT_GENERAL_CLASS; i < MYSQL_AUDIT_CLASS_MASK_SIZE; i++)
  {
    add_audit_mask(&mysql_global_audit_mask[i], &data->class_mask[i]);
  }
  mysql_mutex_unlock(&LOCK_audit_mask);

  return 0;
}


/**
  Performs a bitwise OR of the installed plugins event class masks

  @param[in] thd
  @param[in] plugin
  @param[in] arg

  @retval FALSE  always
*/
static my_bool calc_class_mask(THD *thd, plugin_ref plugin, void *arg)
{
  st_mysql_audit *data= plugin_data<st_mysql_audit*>(plugin);
  if (data)
  {
    int i;
    unsigned long *dst= (unsigned long *)arg;
    for (i = MYSQL_AUDIT_GENERAL_CLASS; i < MYSQL_AUDIT_CLASS_MASK_SIZE; i++)
    {
      add_audit_mask(&dst[i], &data->class_mask[i]);
    }
  }
  return 0;
}


/**
  Finalize an Audit plug-in
  
  @param[in] plugin

  @retval FALSE  OK
  @retval TRUE   There was an error.
*/
int finalize_audit_plugin(st_plugin_int *plugin)
{
  unsigned long event_class_mask[MYSQL_AUDIT_CLASS_MASK_SIZE];
  
  if (plugin->plugin->deinit && plugin->plugin->deinit(NULL))
  {
    DBUG_PRINT("warning", ("Plugin '%s' deinit function returned error.",
                            plugin->name.str));
    DBUG_EXECUTE("finalize_audit_plugin", return 1; );
  }
  
  plugin->data= NULL;
  memset(&event_class_mask, 0, sizeof(event_class_mask));

  /* Iterate through all the installed plugins to create new mask */

  /*
    LOCK_audit_mask/LOCK_plugin order is not fixed, but serialized with table
    lock on mysql.plugin.
  */
  mysql_mutex_lock(&LOCK_audit_mask);
  plugin_foreach(current_thd, calc_class_mask, MYSQL_AUDIT_PLUGIN,
                 &event_class_mask);

  /* Set the global audit mask */
  memmove(mysql_global_audit_mask, event_class_mask, sizeof(event_class_mask));
  mysql_mutex_unlock(&LOCK_audit_mask);

  return 0;
}


/**
  Dispatches an event by invoking the plugin's event_notify method.  

  @param[in] thd
  @param[in] plugin
  @param[in] arg

  @retval FALSE  always
*/

static int plugins_dispatch(THD *thd, plugin_ref plugin, void *arg)
{
  const struct st_mysql_event_generic *event_generic=
    (const struct st_mysql_event_generic *) arg;
  unsigned long subclass = (unsigned long)*(int *)event_generic->event;
  st_mysql_audit *data= plugin_data<st_mysql_audit*>(plugin);

  /* Check to see if the plugin is interested in this event */
  if (check_audit_mask(&data->class_mask[event_generic->event_class], &subclass))
    return 0;

  /* Actually notify the plugin */
  return data->event_notify(thd, event_generic->event_class,
                            event_generic->event);
}

static my_bool plugins_dispatch_bool(THD *thd, plugin_ref plugin, void *arg)
{
  return plugins_dispatch(thd, plugin, arg) ? TRUE : FALSE;
}

/**
  Distributes an audit event to plug-ins
  
  @param[in] thd
  @param[in] event
*/

static int event_class_dispatch(THD *thd, mysql_event_class_t event_class,
                                const void *event)
{
  int result= 0;
  struct st_mysql_event_generic event_generic;
  event_generic.event_class= event_class;
  event_generic.event= event;
  /*
    Check if we are doing a slow global dispatch. This event occurs when
    thd == NULL as it is not associated with any particular thread.
  */
  if (unlikely(!thd))
  {
    return plugin_foreach(thd, plugins_dispatch_bool,
                          MYSQL_AUDIT_PLUGIN, &event_generic) ? 1 : 0;
  }
  else
  {
    plugin_ref *plugins, *plugins_last;

    /* Use the cached set of audit plugins */
    plugins= thd->audit_class_plugins.begin();
    plugins_last= thd->audit_class_plugins.end();

    for (; plugins != plugins_last; plugins++)
      result|= plugins_dispatch(thd, *plugins, &event_generic);
  }

  return result;
}

static int event_class_dispatch_error(THD *thd,
                                      mysql_event_class_t event_class,
                                      const char *event_name,
                                      const void *event)
{
  int result= 0;
  bool err= thd ? thd->get_stmt_da()->is_error() : true;

  if (err)
    /* Audit API cannot modify the already set DA's error state. */
    event_class_dispatch(thd, event_class, event);
  else
  {
    /* We are not is the error state, we can modify the existing one. */
    thd->get_stmt_da()->set_overwrite_status(true);

    result= event_class_dispatch(thd, event_class, event);

    if (result)
    {
      if (!thd->get_stmt_da()->is_error())
      {
        my_error(ER_AUDIT_API_ABORT, MYF(0), event_name, result);
      }
    }

    thd->get_stmt_da()->set_overwrite_status(false);

    /* Because we rely on the error state, we have to notify our
    caller that the Audit API returned with error state. */
    if (thd->get_stmt_da()->is_error())
      result = result != 0 ? result : 1;
  }

  return result;
}

/**  There's at least one active audit plugin tracking a specified class */
bool is_audit_plugin_class_active(THD *thd __attribute__((unused)),
                                  unsigned long event_class)
{
  return mysql_global_audit_mask[event_class] != 0;
}
