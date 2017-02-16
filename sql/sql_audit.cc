/* Copyright (c) 2007, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql_priv.h"
#include "sql_audit.h"

extern int initialize_audit_plugin(st_plugin_int *plugin);
extern int finalize_audit_plugin(st_plugin_int *plugin);

#ifndef EMBEDDED_LIBRARY

struct st_mysql_event_generic
{
  unsigned int event_class;
  const void *event;
};

unsigned long mysql_global_audit_mask[MYSQL_AUDIT_CLASS_MASK_SIZE];

static mysql_mutex_t LOCK_audit_mask;

static void event_class_dispatch(THD *, unsigned int, const void *);


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
  event.general_thread_id= thd ? thd->thread_id : 0;
  event.general_time= va_arg(ap, time_t);
  event.general_user= va_arg(ap, const char *);
  event.general_user_length= va_arg(ap, unsigned int);
  event.general_command= va_arg(ap, const char *);
  event.general_command_length= va_arg(ap, unsigned int);
  event.general_query= va_arg(ap, const char *);
  event.general_query_length= va_arg(ap, unsigned int);
  event.general_charset= va_arg(ap, struct charset_info_st *);
  event.general_rows= (unsigned long long) va_arg(ap, ha_rows);
  event.database= va_arg(ap, const char *);
  event.database_length= va_arg(ap, unsigned int);
  event.query_id= (unsigned long long) (thd ? thd->query_id : 0);
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
  event_class_dispatch(thd, MYSQL_AUDIT_CONNECTION_CLASS, &event);
}


static void table_class_handler(THD *thd, uint event_subclass, va_list ap)
{
  mysql_event_table event;
  event.event_subclass= event_subclass;
  event.read_only= va_arg(ap, int);
  event.thread_id= va_arg(ap, unsigned long);
  event.user= va_arg(ap, const char *);
  event.priv_user= va_arg(ap, const char *);
  event.priv_host= va_arg(ap, const char *);
  event.external_user= va_arg(ap, const char *);
  event.proxy_user= va_arg(ap, const char *);
  event.host= va_arg(ap, const char *);
  event.ip= va_arg(ap, const char *);
  event.database= va_arg(ap, const char *);
  event.database_length= va_arg(ap, unsigned int);
  event.table= va_arg(ap, const char *);
  event.table_length= va_arg(ap, unsigned int);
  event.new_database= va_arg(ap, const char *);
  event.new_database_length= va_arg(ap, unsigned int);
  event.new_table= va_arg(ap, const char *);
  event.new_table_length= va_arg(ap, unsigned int);
  event.query_id= (unsigned long long) (thd ? thd->query_id : 0);
  event_class_dispatch(thd, MYSQL_AUDIT_TABLE_CLASS, &event);
}


static audit_handler_t audit_handlers[] =
{
  general_class_handler, connection_class_handler,
  0,0,0,0,0,0,0,0,0,0,0,0,0, /* placeholders */
  table_class_handler
};

static const uint audit_handlers_count=
  (sizeof(audit_handlers) / sizeof(audit_handler_t));


/**
  Acquire and lock any additional audit plugins as required
  
  @param[in] thd
  @param[in] plugin
  @param[in] arg

  @retval FALSE Always  
*/

static my_bool acquire_plugins(THD *thd, plugin_ref plugin, void *arg)
{
  ulong *event_class_mask= (ulong*) arg;
  st_mysql_audit *data= plugin_data(plugin, struct st_mysql_audit *);

  /* Check if this plugin is interested in the event */
  if (check_audit_mask(data->class_mask, event_class_mask))
    return 0;

  /*
    Check if this plugin may already be registered. This will fail to
    acquire a newly installed plugin on a specific corner case where
    one or more event classes already in use by the calling thread
    are an event class of which the audit plugin has interest.
  */
  if (!check_audit_mask(data->class_mask, thd->audit_class_mask))
    return 0;
  
  /* Check if we need to initialize the array of acquired plugins */
  if (unlikely(!thd->audit_class_plugins.buffer))
  {
    /* specify some reasonable initialization defaults */
    my_init_dynamic_array(&thd->audit_class_plugins,
                          sizeof(plugin_ref), 16, 16);
  }
  
  /* lock the plugin and add it to the list */
  plugin= my_plugin_lock(NULL, plugin);
  insert_dynamic(&thd->audit_class_plugins, (uchar*) &plugin);

  return 0;
}


/**
  @brief Acquire audit plugins

  @param[in]   thd              MySQL thread handle
  @param[in]   event_class      Audit event class

  @details Ensure that audit plugins interested in given event
  class are locked by current thread.
*/
void mysql_audit_acquire_plugins(THD *thd, ulong *event_class_mask)
{
  DBUG_ENTER("mysql_audit_acquire_plugins");
  if (thd && !check_audit_mask(mysql_global_audit_mask, event_class_mask) &&
      check_audit_mask(thd->audit_class_mask, event_class_mask))
  {
    plugin_foreach(thd, acquire_plugins, MYSQL_AUDIT_PLUGIN, event_class_mask);
    add_audit_mask(thd->audit_class_mask, event_class_mask);
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
  unsigned long event_class_mask[MYSQL_AUDIT_CLASS_MASK_SIZE];
  set_audit_mask(event_class_mask, event_class);
  mysql_audit_acquire_plugins(thd, event_class_mask);
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
  
  if (!thd || !(thd->audit_class_plugins.elements))
    return;
  
  plugins= (plugin_ref*) thd->audit_class_plugins.buffer;
  plugins_last= plugins + thd->audit_class_plugins.elements;
  for (; plugins < plugins_last; plugins++)
  {
    st_mysql_audit *data= plugin_data(*plugins, struct st_mysql_audit *);
	
    /* Check to see if the plugin has a release method */
    if (!(data->release_thd))
      continue;

    /* Tell the plugin to release its resources */
    data->release_thd(thd);
  }

  /* Now we actually unlock the plugins */  
  plugin_unlock_list(NULL, (plugin_ref*) thd->audit_class_plugins.buffer,
                     thd->audit_class_plugins.elements);
  
  /* Reset the state of thread values */
  reset_dynamic(&thd->audit_class_plugins);
  bzero(thd->audit_class_mask, sizeof(thd->audit_class_mask));
}


/**
  Initialize thd variables used by Audit
  
  @param[in] thd

*/

void mysql_audit_init_thd(THD *thd)
{
  bzero(&thd->audit_class_plugins, sizeof(thd->audit_class_plugins));
  bzero(thd->audit_class_mask, sizeof(thd->audit_class_mask));
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
  DBUG_ASSERT(thd->audit_class_plugins.elements == 0);
  delete_dynamic(&thd->audit_class_plugins);
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

  if (PSI_server == NULL)
    return;

  count= array_elements(all_audit_mutexes);
  PSI_server->register_mutex(category, all_audit_mutexes, count);
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
  bzero(mysql_global_audit_mask, sizeof(mysql_global_audit_mask));
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
  
  if (!data->class_mask || !data->event_notify ||
      !data->class_mask[0])
  {
    sql_print_error("Plugin '%s' has invalid data.",
                    plugin->name.str);
    return 1;
  }
  
  if (plugin->plugin->init && plugin->plugin->init(NULL))
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

  /*
    Pre-acquire the newly inslalled audit plugin for events that
    may potentially occur further during INSTALL PLUGIN.

    When audit event is triggered, audit subsystem acquires interested
    plugins by walking through plugin list. Evidently plugin list
    iterator protects plugin list by acquiring LOCK_plugin, see
    plugin_foreach_with_mask().

    On the other hand [UN]INSTALL PLUGIN is acquiring LOCK_plugin
    rather for a long time.

    When audit event is triggered during [UN]INSTALL PLUGIN, plugin
    list iterator acquires the same lock (within the same thread)
    second time.

    This hack should be removed when LOCK_plugin is fixed so it
    protects only what it supposed to protect.

    See also mysql_install_plugin() and mysql_uninstall_plugin()
  */
  THD *thd= current_thd;
  if (thd)
  {
    acquire_plugins(thd, plugin_int_to_ref(plugin), data->class_mask);
    add_audit_mask(thd->audit_class_mask, data->class_mask);
  }

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
  st_mysql_audit *data= plugin_data(plugin, struct st_mysql_audit *);
  if ((data= plugin_data(plugin, struct st_mysql_audit *)))
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
  bzero(&event_class_mask, sizeof(event_class_mask));

  /* Iterate through all the installed plugins to create new mask */

  /*
    LOCK_audit_mask/LOCK_plugin order is not fixed, but serialized with table
    lock on mysql.plugin.
  */
  mysql_mutex_lock(&LOCK_audit_mask);
  plugin_foreach(current_thd, calc_class_mask, MYSQL_AUDIT_PLUGIN,
                 &event_class_mask);

  /* Set the global audit mask */
  bmove(mysql_global_audit_mask, event_class_mask, sizeof(event_class_mask));
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
  st_mysql_audit *data= plugin_data(plugin, struct st_mysql_audit *);

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
    plugins= (plugin_ref*) thd->audit_class_plugins.buffer;
    plugins_last= plugins + thd->audit_class_plugins.elements;

    for (; plugins < plugins_last; plugins++)
      plugins_dispatch(thd, *plugins, &event_generic);
  }
}


#else /* EMBEDDED_LIBRARY */


void mysql_audit_acquire_plugins(THD *thd, ulong *event_class_mask)
{
}


void mysql_audit_initialize()
{
}


void mysql_audit_finalize()
{
}


int initialize_audit_plugin(st_plugin_int *plugin)
{
  return 1;
}


int finalize_audit_plugin(st_plugin_int *plugin)
{
  return 0;
}


void mysql_audit_release(THD *thd)
{
}


#endif /* EMBEDDED_LIBRARY */
