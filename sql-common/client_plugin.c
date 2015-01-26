/* Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file
  
  Support code for the client side (libmysql) plugins

  Client plugins are somewhat different from server plugins, they are simpler.

  They do not need to be installed or in any way explicitly loaded on the
  client, they are loaded automatically on demand.
  One client plugin per shared object, soname *must* match the plugin name.

  There is no reference counting and no unloading either.
*/

#include <my_global.h>
#include "mysql.h"
#include <my_sys.h>
#include <m_string.h>
#include <my_thread.h>

#include <sql_common.h>
#include "errmsg.h"
#include <mysql/client_plugin.h>

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#if defined(CLIENT_PROTOCOL_TRACING)
#include <mysql/plugin_trace.h>
#endif

PSI_memory_key key_memory_root;
PSI_memory_key key_memory_load_env_plugins;

#ifdef HAVE_PSI_INTERFACE
PSI_mutex_key key_mutex_LOCK_load_client_plugin;

static PSI_mutex_info all_client_plugin_mutexes[]=
{
  {&key_mutex_LOCK_load_client_plugin, "LOCK_load_client_plugin", PSI_FLAG_GLOBAL}
};

static PSI_memory_info all_client_plugin_memory[]=
{
  {&key_memory_root, "root", PSI_FLAG_GLOBAL},
  {&key_memory_load_env_plugins, "load_env_plugins", PSI_FLAG_GLOBAL}
};

static void init_client_plugin_psi_keys()
{
  const char* category= "sql";
  int count;

  count= array_elements(all_client_plugin_mutexes);
  mysql_mutex_register(category, all_client_plugin_mutexes, count);

  count= array_elements(all_client_plugin_memory);
  mysql_memory_register(category, all_client_plugin_memory, count);
}
#endif /* HAVE_PSI_INTERFACE */

struct st_client_plugin_int {
  struct st_client_plugin_int *next;
  void   *dlhandle;
  struct st_mysql_client_plugin *plugin;
};

static my_bool initialized= 0;
static MEM_ROOT mem_root;

static const char *plugin_declarations_sym= "_mysql_client_plugin_declaration_";
static uint plugin_version[MYSQL_CLIENT_MAX_PLUGINS]=
{
  0, /* these two are taken by Connector/C */
  0, /* these two are taken by Connector/C */
  MYSQL_CLIENT_AUTHENTICATION_PLUGIN_INTERFACE_VERSION,
  MYSQL_CLIENT_TRACE_PLUGIN_INTERFACE_VERSION,
};

/*
  Loaded plugins are stored in a linked list.
  The list is append-only, the elements are added to the head (like in a stack).
  The elements are added under a mutex, but the list can be read and traversed
  without any mutex because once an element is added to the list, it stays
  there. The main purpose of a mutex is to prevent two threads from
  loading the same plugin twice in parallel.
*/
struct st_client_plugin_int *plugin_list[MYSQL_CLIENT_MAX_PLUGINS];
static mysql_mutex_t LOCK_load_client_plugin;

static int is_not_initialized(MYSQL *mysql, const char *name)
{
  if (initialized)
    return 0;

  set_mysql_extended_error(mysql, CR_AUTH_PLUGIN_CANNOT_LOAD,
                           unknown_sqlstate, ER(CR_AUTH_PLUGIN_CANNOT_LOAD),
                           name, "not initialized");
  return 1;
}

/**
  finds a plugin in the list

  @param name   plugin name to search for
  @param type   plugin type

  @note this does NOT necessarily need a mutex, take care!
  
  @retval a pointer to a found plugin or 0
*/
static struct st_mysql_client_plugin *
find_plugin(const char *name, int type)
{
  struct st_client_plugin_int *p;

  DBUG_ASSERT(initialized);
  DBUG_ASSERT(type >= 0 && type < MYSQL_CLIENT_MAX_PLUGINS);
  if (type < 0 || type >= MYSQL_CLIENT_MAX_PLUGINS)
    return 0;

  for (p= plugin_list[type]; p; p= p->next)
  {
    if (strcmp(p->plugin->name, name) == 0)
      return p->plugin;
  }
  return NULL;
}

/**
  verifies the plugin and adds it to the list

  @param mysql          MYSQL structure (for error reporting)
  @param plugin         plugin to install
  @param dlhandle       a handle to the shared object (returned by dlopen)
                        or 0 if the plugin was not dynamically loaded
  @param argc           number of arguments in the 'va_list args'
  @param args           arguments passed to the plugin initialization function

  @retval a pointer to an installed plugin or 0
*/
static struct st_mysql_client_plugin *
do_add_plugin(MYSQL *mysql, struct st_mysql_client_plugin *plugin,
              void *dlhandle,
              int argc, va_list args)
{
  const char *errmsg;
  struct st_client_plugin_int plugin_int, *p;
  char errbuf[1024];

  DBUG_ASSERT(initialized);

  plugin_int.plugin= plugin;
  plugin_int.dlhandle= dlhandle;

  if (plugin->type >= MYSQL_CLIENT_MAX_PLUGINS)
  {
    errmsg= "Unknown client plugin type";
    goto err1;
  }

  if (plugin->interface_version < plugin_version[plugin->type] ||
      (plugin->interface_version >> 8) >
       (plugin_version[plugin->type] >> 8))
  {
    errmsg= "Incompatible client plugin interface";
    goto err1;
  }

#if defined(CLIENT_PROTOCOL_TRACING) && !defined(MYSQL_SERVER)
  /*
    If we try to load a protocol trace plugin but one is already
    loaded (global trace_plugin pointer is not NULL) then we ignore
    the new trace plugin and give error. This is done before the
    new plugin gets initialized.
  */
  if (plugin->type == MYSQL_CLIENT_TRACE_PLUGIN && NULL != trace_plugin)
  {
    errmsg= "Can not load another trace plugin while one is already loaded";
    goto err1;
  }
#endif

  /* Call the plugin initialization function, if any */
  if (plugin->init && plugin->init(errbuf, sizeof(errbuf), argc, args))
  {
    errmsg= errbuf;
    goto err1;
  }

  p= (struct st_client_plugin_int *)
    memdup_root(&mem_root, &plugin_int, sizeof(plugin_int));

  if (!p)
  {
    errmsg= "Out of memory";
    goto err2;
  }

  mysql_mutex_assert_owner(&LOCK_load_client_plugin);

  p->next= plugin_list[plugin->type];
  plugin_list[plugin->type]= p;
  net_clear_error(&mysql->net);

#if defined(CLIENT_PROTOCOL_TRACING) && !defined(MYSQL_SERVER)
  /*
    If loaded plugin is a protocol trace one, then set the global
    trace_plugin pointer to point at it. When trace_plugin is not NULL,
    each new connection will be traced using the plugin pointed by it
    (see MYSQL_TRACE_STAGE() macro in libmysql/mysql_trace.h).
  */
  if (plugin->type == MYSQL_CLIENT_TRACE_PLUGIN)
  {
    trace_plugin = (struct st_mysql_client_plugin_TRACE*)plugin;
  }
#endif

  return plugin;

err2:
  if (plugin->deinit)
    plugin->deinit();
err1:
  set_mysql_extended_error(mysql, CR_AUTH_PLUGIN_CANNOT_LOAD, unknown_sqlstate,
                           ER(CR_AUTH_PLUGIN_CANNOT_LOAD), plugin->name,
                           errmsg);
  if (dlhandle)
    dlclose(dlhandle);
  return NULL;
}


static struct st_mysql_client_plugin *
add_plugin_noargs(MYSQL *mysql, struct st_mysql_client_plugin *plugin,
                  void *dlhandle,
                  int argc, ...)
{
  struct st_mysql_client_plugin *retval= NULL;
  va_list ap;
  va_start(ap, argc);
  retval= do_add_plugin(mysql, plugin, dlhandle, argc, ap);
  va_end(ap);
  return retval;
}


static struct st_mysql_client_plugin *
add_plugin_withargs(MYSQL *mysql, struct st_mysql_client_plugin *plugin,
                    void *dlhandle,
                    int argc, va_list args)
{
  return do_add_plugin(mysql, plugin, dlhandle, argc, args);
}



/**
  Loads plugins which are specified in the environment variable
  LIBMYSQL_PLUGINS.
  
  Multiple plugins must be separated by semicolon. This function doesn't
  return or log an error.

  The function is be called by mysql_client_plugin_init

  @todo
  Support extended syntax, passing parameters to plugins, for example
  LIBMYSQL_PLUGINS="plugin1(param1,param2);plugin2;..."
  or
  LIBMYSQL_PLUGINS="plugin1=int:param1,str:param2;plugin2;..."
*/
static void load_env_plugins(MYSQL *mysql)
{
  char *plugs, *free_env, *s= getenv("LIBMYSQL_PLUGINS");
  char *enable_cleartext_plugin= getenv("LIBMYSQL_ENABLE_CLEARTEXT_PLUGIN");

  if (enable_cleartext_plugin && strchr("1Yy", enable_cleartext_plugin[0]))
    libmysql_cleartext_plugin_enabled= 1;

  /* no plugins to load */
  if(!s)
    return;

  free_env= plugs= my_strdup(key_memory_load_env_plugins,
                             s, MYF(MY_WME));

  do {
    if ((s= strchr(plugs, ';')))
      *s= '\0';
    mysql_load_plugin(mysql, plugs, -1, 0);
    plugs= s + 1;
  } while (s);

  my_free(free_env);

}


/********** extern functions to be used by libmysql *********************/

/**
  Initializes the client plugin layer.

  This function must be called before any other client plugin function.

  @retval 0    successful
  @retval != 0 error occured
*/
int mysql_client_plugin_init()
{
  MYSQL mysql;
  struct st_mysql_client_plugin **builtin;

  if (initialized)
    return 0;

#ifdef HAVE_PSI_INTERFACE
  init_client_plugin_psi_keys();
#endif /* HAVE_PSI_INTERFACE */

  memset(&mysql, 0, sizeof(mysql)); /* dummy mysql for set_mysql_extended_error */

  mysql_mutex_init(key_mutex_LOCK_load_client_plugin,
                   &LOCK_load_client_plugin, MY_MUTEX_INIT_SLOW);
  init_alloc_root(key_memory_root, &mem_root, 128, 128);

  memset(&plugin_list, 0, sizeof(plugin_list));

  initialized= 1;

  mysql_mutex_lock(&LOCK_load_client_plugin);

  for (builtin= mysql_client_builtins; *builtin; builtin++)
    add_plugin_noargs(&mysql, *builtin, 0, 0);

  mysql_mutex_unlock(&LOCK_load_client_plugin);

  load_env_plugins(&mysql);

  mysql_close_free(&mysql);

  return 0;
}

/**
  Deinitializes the client plugin layer.

  Unloades all client plugins and frees any associated resources.
*/
void mysql_client_plugin_deinit()
{
  int i;
  struct st_client_plugin_int *p;

  if (!initialized)
    return;

  for (i=0; i < MYSQL_CLIENT_MAX_PLUGINS; i++)
    for (p= plugin_list[i]; p; p= p->next)
    {
      if (p->plugin->deinit)
        p->plugin->deinit();
      if (p->dlhandle)
        dlclose(p->dlhandle);
    }

  memset(&plugin_list, 0, sizeof(plugin_list));
  initialized= 0;
  free_root(&mem_root, MYF(0));
  mysql_mutex_destroy(&LOCK_load_client_plugin);
}


/************* public facing functions, for client consumption *********/

/* see <mysql/client_plugin.h> for a full description */
struct st_mysql_client_plugin *
mysql_client_register_plugin(MYSQL *mysql,
                             struct st_mysql_client_plugin *plugin)
{
  if (is_not_initialized(mysql, plugin->name))
    return NULL;

  mysql_mutex_lock(&LOCK_load_client_plugin);

  /* make sure the plugin wasn't loaded meanwhile */
  if (find_plugin(plugin->name, plugin->type))
  {
    set_mysql_extended_error(mysql, CR_AUTH_PLUGIN_CANNOT_LOAD,
                             unknown_sqlstate, ER(CR_AUTH_PLUGIN_CANNOT_LOAD),
                             plugin->name, "it is already loaded");
    plugin= NULL;
  }
  else
    plugin= add_plugin_noargs(mysql, plugin, 0, 0);

  mysql_mutex_unlock(&LOCK_load_client_plugin);
  return plugin;
}

/* see <mysql/client_plugin.h> for a full description */
struct st_mysql_client_plugin *
mysql_load_plugin_v(MYSQL *mysql, const char *name, int type,
                    int argc, va_list args)
{
  const char *errmsg;
  char dlpath[FN_REFLEN+1];
  void *sym, *dlhandle;
  struct st_mysql_client_plugin *plugin;
  const char *plugindir;
#ifdef _WIN32
  char win_errormsg[2048];
#endif

  DBUG_ENTER ("mysql_load_plugin_v");
  DBUG_PRINT ("entry", ("name=%s type=%d int argc=%d", name, type, argc));
  if (is_not_initialized(mysql, name))
  {
    DBUG_PRINT ("leave", ("mysql not initialized"));
    DBUG_RETURN (NULL);
  }

  mysql_mutex_lock(&LOCK_load_client_plugin);

  /* make sure the plugin wasn't loaded meanwhile */
  if (type >= 0 && find_plugin(name, type))
  {
    errmsg= "it is already loaded";
    goto err;
  }

  if (mysql->options.extension && mysql->options.extension->plugin_dir)
  {
    plugindir= mysql->options.extension->plugin_dir;
  }
  else
  {
    plugindir= getenv("LIBMYSQL_PLUGIN_DIR");
    if (!plugindir)
    {
      plugindir= PLUGINDIR;
    }
  }

  /* Compile dll path */
  strxnmov(dlpath, sizeof(dlpath) - 1,
           plugindir, "/",
           name, SO_EXT, NullS);
   
  DBUG_PRINT ("info", ("dlopeninig %s", dlpath));
  /* Open new dll handle */
  if (!(dlhandle= dlopen(dlpath, RTLD_NOW)))
  {
#if defined(__APPLE__)
    /* Apple supports plugins with .so also, so try this as well */
    strxnmov(dlpath, sizeof(dlpath) - 1,
             plugindir, "/",
             name, ".so", NullS);
    if ((dlhandle= dlopen(dlpath, RTLD_NOW)))
      goto have_plugin;
#endif

#ifdef _WIN32
    /* There should be no win32 calls between failed dlopen() and GetLastError() */
    if(FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                  0, GetLastError(), 0, win_errormsg, 2048, NULL))
      errmsg= win_errormsg;
    else
      errmsg= "";
#else
    errmsg= dlerror();
#endif
    DBUG_PRINT ("info", ("failed to dlopen"));
    goto err;
  }

#if defined(__APPLE__)
have_plugin:  
#endif
  if (!(sym= dlsym(dlhandle, plugin_declarations_sym)))
  {
    errmsg= "not a plugin";
    dlclose(dlhandle);
    goto err;
  }

  plugin= (struct st_mysql_client_plugin*)sym;

  if (type >=0 && type != plugin->type)
  {
    errmsg= "type mismatch";
    goto err;
  }

  if (strcmp(name, plugin->name))
  {
    errmsg= "name mismatch";
    goto err;
  }

  if (type < 0 && find_plugin(name, plugin->type))
  {
    errmsg= "it is already loaded";
    goto err;
  }

  plugin= add_plugin_withargs(mysql, plugin, dlhandle, argc, args);

  mysql_mutex_unlock(&LOCK_load_client_plugin);

  DBUG_PRINT ("leave", ("plugin loaded ok"));
  DBUG_RETURN (plugin);

err:
  mysql_mutex_unlock(&LOCK_load_client_plugin);
  DBUG_PRINT ("leave", ("plugin load error : %s", errmsg));
  set_mysql_extended_error(mysql, CR_AUTH_PLUGIN_CANNOT_LOAD, unknown_sqlstate,
                           ER(CR_AUTH_PLUGIN_CANNOT_LOAD), name, errmsg);
  DBUG_RETURN (NULL);
}

/* see <mysql/client_plugin.h> for a full description */
struct st_mysql_client_plugin *
mysql_load_plugin(MYSQL *mysql, const char *name, int type, int argc, ...)
{
  struct st_mysql_client_plugin *p;
  va_list args;
  va_start(args, argc);
  p= mysql_load_plugin_v(mysql, name, type, argc, args);
  va_end(args);
  return p;
}

/* see <mysql/client_plugin.h> for a full description */
struct st_mysql_client_plugin *
mysql_client_find_plugin(MYSQL *mysql, const char *name, int type)
{
  struct st_mysql_client_plugin *p;

  DBUG_ENTER ("mysql_client_find_plugin");
  DBUG_PRINT ("entry", ("name=%s, type=%d", name, type));
  if (is_not_initialized(mysql, name))
    DBUG_RETURN (NULL);

  if (type < 0 || type >= MYSQL_CLIENT_MAX_PLUGINS)
  {
    set_mysql_extended_error(mysql, CR_AUTH_PLUGIN_CANNOT_LOAD, unknown_sqlstate,
                             ER(CR_AUTH_PLUGIN_CANNOT_LOAD), name,
                             "invalid type");
  }

  if ((p= find_plugin(name, type)))
  {
    DBUG_PRINT ("leave", ("found %p", p));
    DBUG_RETURN (p);
  }

  /* not found, load it */
  p= mysql_load_plugin(mysql, name, type, 0);
  DBUG_PRINT ("leave", ("loaded %p", p));
  DBUG_RETURN (p);
}


/* see <mysql/client_plugin.h> for a full description */
int mysql_plugin_options(struct st_mysql_client_plugin *plugin,
                                 const char *option,
                                 const void *value)
{
  DBUG_ENTER("mysql_plugin_options");
  /* does the plugin support options call? */
  if (!plugin || !plugin->options)
    DBUG_RETURN(1);
  DBUG_RETURN(plugin->options(option, value));
}
