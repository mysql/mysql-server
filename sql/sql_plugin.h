/* Copyright (C) 2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef _sql_plugin_h
#define _sql_plugin_h

/*
  the following #define adds server-only members to enum_mysql_show_type,
  that is defined in plugin.h
*/
#define SHOW_FUNC    SHOW_FUNC, SHOW_KEY_CACHE_LONG, SHOW_KEY_CACHE_LONGLONG, \
                     SHOW_LONG_STATUS, SHOW_DOUBLE_STATUS, SHOW_HAVE,   \
                     SHOW_MY_BOOL, SHOW_HA_ROWS, SHOW_SYS, SHOW_LONG_NOFLUSH
#include <mysql/plugin.h>
#undef SHOW_FUNC
typedef enum enum_mysql_show_type SHOW_TYPE;
typedef struct st_mysql_show_var SHOW_VAR;

#define MYSQL_ANY_PLUGIN         -1

enum enum_plugin_state
{
  PLUGIN_IS_FREED= 0,
  PLUGIN_IS_DELETED,
  PLUGIN_IS_UNINITIALIZED,
  PLUGIN_IS_READY
};

/* A handle for the dynamic library containing a plugin or plugins. */

struct st_plugin_dl
{
  LEX_STRING dl;
  void *handle;
  struct st_mysql_plugin *plugins;
  int version;
  uint ref_count;            /* number of plugins loaded from the library */
};

/* A handle of a plugin */

struct st_plugin_int
{
  LEX_STRING name;
  struct st_mysql_plugin *plugin;
  struct st_plugin_dl *plugin_dl;
  enum enum_plugin_state state;
  uint ref_count;               /* number of threads using the plugin */
  void *data;                   /* plugin type specific, e.g. handlerton */
};

typedef int (*plugin_type_init)(struct st_plugin_int *);

extern char *opt_plugin_dir_ptr;
extern char opt_plugin_dir[FN_REFLEN];
extern const LEX_STRING plugin_type_names[];
extern int plugin_init(int);
extern void plugin_shutdown(void);
extern my_bool plugin_is_ready(const LEX_STRING *name, int type);
extern st_plugin_int *plugin_lock(const LEX_STRING *name, int type);
extern void plugin_unlock(struct st_plugin_int *plugin);
extern my_bool mysql_install_plugin(THD *thd, const LEX_STRING *name, const LEX_STRING *dl);
extern my_bool mysql_uninstall_plugin(THD *thd, const LEX_STRING *name);

typedef my_bool (plugin_foreach_func)(THD *thd,
                                      st_plugin_int *plugin,
                                      void *arg);
extern my_bool plugin_foreach(THD *thd, plugin_foreach_func *func,
                              int type, void *arg);
#endif
