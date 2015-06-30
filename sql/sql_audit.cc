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
  unsigned int event_class;
  const void *event;
};

unsigned long mysql_global_audit_mask[MYSQL_AUDIT_CLASS_MASK_SIZE];

static mysql_mutex_t LOCK_audit_mask;

static void event_class_dispatch(THD *thd, unsigned int event_class,
                                 const void *event);


static inline
void set_audit_mask(unsigned long *mask, uint event_class)
{
  mask[0]= 1;
  mask[0]<<= event_class;
}

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


typedef void (*audit_handler_t)(THD *thd, uint event_subtype, va_list ap);

/**
  MYSQL_AUDIT_GENERAL_CLASS handler
  
  @param[in] thd
  @param[in] event_subtype
  @param[in] error_code
  @param[in] ap
  
*/

static void general_class_handler(THD *thd, uint event_subtype, va_list ap)
{
  mysql_event_general event;
  event.event_subclass= event_subtype;
  event.general_error_code= va_arg(ap, int);
  event.general_thread_id= thd ? thd->thread_id() : 0;
  event.general_time= va_arg(ap, time_t);
  event.general_user= va_arg(ap, const char *);                // user
  event.general_user_length= va_arg(ap, unsigned int);         // userlen
  event.general_command= va_arg(ap, const char *);             // cmd
  event.general_command_length= va_arg(ap, unsigned int);      // cmdlen
  event.general_query= va_arg(ap, const char *);               // query
  event.general_query_length= va_arg(ap, unsigned int);        // querylen
  event.general_charset= va_arg(ap, struct charset_info_st *); // clientcs
  event.general_rows= (unsigned long long) va_arg(ap, ha_rows);
  event.general_sql_command= va_arg(ap, MYSQL_LEX_STRING);
  event.general_host= va_arg(ap, MYSQL_LEX_STRING);
  event.general_external_user= va_arg(ap, MYSQL_LEX_STRING);
  event.general_ip= va_arg(ap, MYSQL_LEX_STRING);
  event_class_dispatch(thd, MYSQL_AUDIT_GENERAL_CLASS, &event);
}


static void connection_class_handler(THD *thd, uint event_subclass, va_list ap)
{
  mysql_event_connection event;
  event.event_subclass= event_subclass;
  event.status= va_arg(ap, int);
  event.thread_id= va_arg(ap, unsigned long);
  event.user= va_arg(ap, const char *);
  event.user_length= va_arg(ap, unsigned int);
  event.priv_user= va_arg(ap, const char *);
  event.priv_user_length= va_arg(ap, unsigned int);
  event.external_user= va_arg(ap, const char *);
  event.external_user_length= va_arg(ap, unsigned int);
  event.proxy_user= va_arg(ap, const char *);
  event.proxy_user_length= va_arg(ap, unsigned int);
  event.host= va_arg(ap, const char *);
  event.host_length= va_arg(ap, unsigned int);
  event.ip= va_arg(ap, const char *);
  event.ip_length= va_arg(ap, unsigned int);
  event.database= va_arg(ap, const char *);
  event.database_length= va_arg(ap, unsigned int);
  event.connection_type = va_arg(ap, int);
  event_class_dispatch(thd, MYSQL_AUDIT_CONNECTION_CLASS, &event);
}


static audit_handler_t audit_handlers[] =
{
  general_class_handler, connection_class_handler
};

#ifndef DBUG_OFF
static const uint audit_handlers_count=
  (sizeof(audit_handlers) / sizeof(audit_handler_t));
#endif


/**
  Acquire and lock any additional audit plugins as required
  
  @param[in] thd
  @param[in] plugin
  @param[in] arg

  @retval FALSE Always  
*/

static my_bool acquire_plugins(THD *thd, plugin_ref plugin, void *arg)
{
  uint event_class= *(uint*) arg;
  unsigned long event_class_mask[MYSQL_AUDIT_CLASS_MASK_SIZE];
  st_mysql_audit *data= plugin_data<st_mysql_audit*>(plugin);

  set_audit_mask(event_class_mask, event_class);

  /* Check if this plugin is interested in the event */
  if (check_audit_mask(data->class_mask, event_class_mask))
    return 0;

  /*
    Check if this plugin may already be registered. This will fail to
    acquire a newly installed plugin on a specific corner case where
    one or more event classes already in use by the calling thread
    are an event class of which the audit plugin has interest.
  */
  if (!check_audit_mask(data->class_mask, &thd->audit_class_mask[0]))
    return 0;
  
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
void mysql_audit_acquire_plugins(THD *thd, uint event_class)
{
  unsigned long event_class_mask[MYSQL_AUDIT_CLASS_MASK_SIZE];
  DBUG_ENTER("mysql_audit_acquire_plugins");
  set_audit_mask(event_class_mask, event_class);
  if (thd && !check_audit_mask(mysql_global_audit_mask, event_class_mask) &&
      check_audit_mask(&thd->audit_class_mask[0], event_class_mask))
  {
    plugin_foreach(thd, acquire_plugins, MYSQL_AUDIT_PLUGIN, &event_class);
    add_audit_mask(&thd->audit_class_mask[0], event_class_mask);
  }
  DBUG_VOID_RETURN;
}
 

/**
  Notify the audit system of an event
  
  @param[in] thd
  @param[in] event_class
  @param[in] event_subtype
  @param[in] error_code

*/

void mysql_audit_notify(THD *thd, uint event_class, uint event_subtype, ...)
{
  va_list ap;
  audit_handler_t *handlers= audit_handlers + event_class;
  DBUG_ASSERT(event_class < audit_handlers_count);
  mysql_audit_acquire_plugins(thd, event_class);
  va_start(ap, event_subtype);  
  (*handlers)(thd, event_subtype, ap);
  va_end(ap);
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


void mysql_audit_general_log(THD *thd, const char *cmd, size_t cmdlen)
{
  if (mysql_global_audit_mask[0] & MYSQL_AUDIT_GENERAL_CLASSMASK)
  {
    MYSQL_LEX_STRING sql_command, ip, host, external_user;
    LEX_CSTRING query= EMPTY_CSTR;
    static MYSQL_LEX_STRING empty= { C_STRING_WITH_LEN("") };
    char user_buff[MAX_USER_HOST_SIZE + 1];
    const char *user= user_buff;
    size_t userlen= make_user_name(thd->security_context(), user_buff);
    time_t time= (time_t) thd->start_time.tv_sec;
    int error_code= 0;

    if (thd)
    {
      if (!thd->rewritten_query.length())
        mysql_rewrite_query(thd);
      if (thd->rewritten_query.length())
      {
        query.str= thd->rewritten_query.ptr();
        query.length= thd->rewritten_query.length();
      }
      else
        query= thd->query();
      Security_context *sctx= thd->security_context();
      LEX_CSTRING sctx_host= sctx->host();
      LEX_CSTRING sctx_ip= sctx->ip();
      LEX_CSTRING sctx_external_user= sctx->external_user();
      ip.str= (char *) sctx_ip.str;
      ip.length= sctx_ip.length;
      host.str= (char *) sctx_host.str;
      host.length= sctx_host.length;
      external_user.str= (char *) sctx_external_user.str;
      external_user.length= sctx_external_user.length;
      sql_command.str= (char *) sql_statement_names[thd->lex->sql_command].str;
      sql_command.length= sql_statement_names[thd->lex->sql_command].length;
    }
    else
    {
      ip= empty;
      host= empty;
      external_user= empty;
      sql_command= empty;
    }
    const CHARSET_INFO *clientcs= thd ? thd->variables.character_set_client
      : global_system_variables.character_set_client;

    mysql_audit_notify(thd, MYSQL_AUDIT_GENERAL_CLASS, MYSQL_AUDIT_GENERAL_LOG,
                       error_code, time, user, userlen, cmd, cmdlen, query.str,
                       query.length, clientcs,
                       static_cast<ha_rows>(0), /* general_rows */
                       sql_command, host, external_user, ip);

  }
}


void mysql_audit_general(THD *thd, uint event_subtype,
                         int error_code, const char *msg)
{
  if (mysql_global_audit_mask[0] & MYSQL_AUDIT_GENERAL_CLASSMASK)
  {
    time_t time= my_time(0);
    size_t msglen= msg ? strlen(msg) : 0;
    size_t userlen;
    const char *user;
    char user_buff[MAX_USER_HOST_SIZE];
    LEX_CSTRING query= EMPTY_CSTR;
    const CHARSET_INFO *query_charset= thd->charset();
    MYSQL_LEX_STRING ip, host, external_user, sql_command;
    ha_rows rows;
    Security_context *sctx;
    LEX_CSTRING sctx_host, sctx_ip, sctx_external_user;
    static MYSQL_LEX_STRING empty= { C_STRING_WITH_LEN("") };

    if (thd)
    {
      if (!thd->rewritten_query.length())
        mysql_rewrite_query(thd);
      if (thd->rewritten_query.length())
      {
        query.str= thd->rewritten_query.ptr();
        query.length= thd->rewritten_query.length();
        query_charset= thd->rewritten_query.charset();
      }
      else
        query= thd->query();
      user= user_buff;
      userlen= make_user_name(thd->security_context(), user_buff);
      sctx= thd->security_context();
      rows= thd->get_stmt_da()->current_row_for_condition();
      sctx_ip= sctx->ip();
      ip.str= (char *) sctx_ip.str;
      ip.length= sctx_ip.length;
      sctx_host= sctx->host();
      host.str= (char *) sctx_host.str;
      host.length= sctx_host.length;
      sctx_external_user= sctx->external_user();
      external_user.str= (char *) sctx_external_user.str;
      external_user.length= sctx_external_user.length;
      sql_command.str= (char *) sql_statement_names[thd->lex->sql_command].str;
      sql_command.length= sql_statement_names[thd->lex->sql_command].length;
    }
    else
    {
      user= 0;
      userlen= 0;
      ip= empty;
      host= empty;
      external_user= empty;
      sql_command= empty;
      rows= 0;
    }

    mysql_audit_notify(thd, MYSQL_AUDIT_GENERAL_CLASS, event_subtype,
                       error_code, time, user, userlen, msg, msglen,
                       query.str, query.length, query_charset, rows,
                       sql_command, host, external_user, ip);
  }
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
  
  @param[in] thd
  @param[in] plugin
  @param[in] arg

  @retval FALSE Always  
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
  
  if (!data->event_notify || !data->class_mask[0])
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
  add_audit_mask(mysql_global_audit_mask, data->class_mask);
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
    add_audit_mask((unsigned long *) arg, data->class_mask);
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

static my_bool plugins_dispatch(THD *thd, plugin_ref plugin, void *arg)
{
  const struct st_mysql_event_generic *event_generic=
    (const struct st_mysql_event_generic *) arg;
  unsigned long event_class_mask[MYSQL_AUDIT_CLASS_MASK_SIZE];
  st_mysql_audit *data= plugin_data<st_mysql_audit*>(plugin);

  set_audit_mask(event_class_mask, event_generic->event_class);

  /* Check to see if the plugin is interested in this event */
  if (check_audit_mask(data->class_mask, event_class_mask))
    return 0;

  /* Actually notify the plugin */
  data->event_notify(thd, event_generic->event_class, event_generic->event);

  return 0;
}


/**
  Distributes an audit event to plug-ins
  
  @param[in] thd
  @param[in] event
*/

static void event_class_dispatch(THD *thd, unsigned int event_class,
                                 const void *event)
{
  struct st_mysql_event_generic event_generic;
  event_generic.event_class= event_class;
  event_generic.event= event;
  /*
    Check if we are doing a slow global dispatch. This event occurs when
    thd == NULL as it is not associated with any particular thread.
  */
  if (unlikely(!thd))
  {
    plugin_foreach(thd, plugins_dispatch, MYSQL_AUDIT_PLUGIN, &event_generic);
  }
  else
  {
    plugin_ref *plugins, *plugins_last;

    /* Use the cached set of audit plugins */
    plugins= thd->audit_class_plugins.begin();
    plugins_last= thd->audit_class_plugins.end();

    for (; plugins != plugins_last; plugins++)
      plugins_dispatch(thd, *plugins, &event_generic);
  }
}


/**  There's at least one active audit plugin tracking the general events */
bool is_any_audit_plugin_active(THD *thd __attribute__((unused)))
{
  return (mysql_global_audit_mask[0] & MYSQL_AUDIT_GENERAL_CLASSMASK);
}

