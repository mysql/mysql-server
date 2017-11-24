/* Copyright (c) 2005, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _sql_plugin_h
#define _sql_plugin_h

#include <my_global.h>
#include <vector>
#include <mysql/plugin.h>

#include "mysql/mysql_lex_string.h"         // LEX_STRING
#include "my_alloc.h"                       /* MEM_ROOT */
#include "my_getopt.h"                      /* my_option */
#include "sql_const.h"                      /* SHOW_COMP_OPTION */

extern const char *global_plugin_typelib_names[];
extern mysql_mutex_t LOCK_plugin_delete;

#include <my_sys.h>
#include "sql_list.h"
#include "sql_cmd.h"
#include "sql_plugin_ref.h"

#ifdef DBUG_OFF
#define plugin_ref_to_int(A) A
#define plugin_int_to_ref(A) A
#else
#define plugin_ref_to_int(A) (A ? A[0] : NULL)
#define plugin_int_to_ref(A) &(A)
#endif

/*
  the following flags are valid for plugin_init()
*/
#define PLUGIN_INIT_SKIP_DYNAMIC_LOADING 1
#define PLUGIN_INIT_SKIP_PLUGIN_TABLE    2
#define PLUGIN_INIT_SKIP_INITIALIZATION  4

typedef struct st_mysql_show_var SHOW_VAR;
typedef struct st_mysql_lex_string LEX_STRING;

#define MYSQL_ANY_PLUGIN         -1

/*
  different values of st_plugin_int::state
  though they look like a bitmap, plugin may only
  be in one of those eigenstates, not in a superposition of them :)
  It's a bitmap, because it makes it easier to test
  "whether the state is one of those..."
*/
#define PLUGIN_IS_FREED         1
#define PLUGIN_IS_DELETED       2
#define PLUGIN_IS_UNINITIALIZED 4
#define PLUGIN_IS_READY         8
#define PLUGIN_IS_DYING         16
#define PLUGIN_IS_DISABLED      32

/* A handle for the dynamic library containing a plugin or plugins. */

struct st_plugin_dl
{
  LEX_STRING dl;
  void *handle;
  struct st_mysql_plugin *plugins;
  int version;
  uint ref_count;            /* number of plugins loaded from the library */
};


/**
   This class implements the INSTALL PLUGIN statement.
*/

class Sql_cmd_install_plugin : public Sql_cmd
{
public:
  Sql_cmd_install_plugin(const LEX_STRING& comment,
                         const LEX_STRING& ident)
  : m_comment(comment), m_ident(ident)
  { }

  virtual enum_sql_command sql_command_code() const
  { return SQLCOM_INSTALL_PLUGIN; }

  /**
    Install a new plugin by inserting a row into the
    mysql.plugin table, creating a cache entry and
    initializing plugin's internal data.

    @param thd  Thread context

    @returns false if success, true otherwise
  */
  virtual bool execute(THD *thd);

private:
  LEX_STRING m_comment;
  LEX_STRING m_ident;
};


/**
   This class implements the UNINSTALL PLUGIN statement.
*/

class Sql_cmd_uninstall_plugin : public Sql_cmd
{
public:
  explicit Sql_cmd_uninstall_plugin(const LEX_STRING& comment)
  : m_comment(comment)
  { }

  virtual enum_sql_command sql_command_code() const
  { return SQLCOM_UNINSTALL_PLUGIN; }

  /**
    Uninstall a plugin by removing a row from the
    mysql.plugin table, deleting a cache entry and
    deinitializing plugin's internal data.

    @param thd  Thread context

    @returns false if success, true otherwise
  */
  virtual bool execute(THD *thd);

private:
  LEX_STRING m_comment;
};


typedef int (*plugin_type_init)(struct st_plugin_int *);

extern I_List<i_string> *opt_plugin_load_list_ptr;
extern I_List<i_string> *opt_early_plugin_load_list_ptr;
extern char *opt_plugin_dir_ptr;
extern char opt_plugin_dir[FN_REFLEN];
extern const LEX_STRING plugin_type_names[];

extern bool plugin_register_early_plugins(int *argc, char **argv, int flags);
extern bool plugin_register_builtin_and_init_core_se(int *argc, char **argv);
extern bool plugin_register_dynamic_and_init_all(int *argc,
                                                 char **argv, int init_flags);
extern void plugin_shutdown(void);
extern void memcached_shutdown(void);
void add_plugin_options(std::vector<my_option> *options, MEM_ROOT *mem_root);
extern bool plugin_is_ready(const LEX_CSTRING &name, int type);
#define my_plugin_lock_by_name(A,B,C) plugin_lock_by_name(A,B,C)
#define my_plugin_lock_by_name_ci(A,B,C) plugin_lock_by_name(A,B,C)
#define my_plugin_lock(A,B) plugin_lock(A,B)
#define my_plugin_lock_ci(A,B) plugin_lock(A,B)
extern plugin_ref plugin_lock(THD *thd, plugin_ref *ptr);
extern plugin_ref plugin_lock_by_name(THD *thd, const LEX_CSTRING &name,
                                      int type);
extern void plugin_unlock(THD *thd, plugin_ref plugin);
extern void plugin_unlock_list(THD *thd, plugin_ref *list, size_t count);
extern bool plugin_register_builtin(struct st_mysql_plugin *plugin);
extern void plugin_thdvar_init(THD *thd, bool enable_plugins);
extern void plugin_thdvar_cleanup(THD *thd, bool enable_plugins);
extern SHOW_COMP_OPTION plugin_status(const char *name, size_t len, int type);
extern bool check_valid_path(const char *path, size_t length);
extern void alloc_and_copy_thd_dynamic_variables(THD *thd, bool global_lock);

typedef my_bool (plugin_foreach_func)(THD *thd,
                                      plugin_ref plugin,
                                      void *arg);
#define plugin_foreach(A,B,C,D) plugin_foreach_with_mask(A,B,C,PLUGIN_IS_READY,D)
extern bool plugin_foreach_with_mask(THD *thd, plugin_foreach_func *func,
                                     int type, uint state_mask, void *arg);
extern bool plugin_foreach_with_mask(THD *thd, plugin_foreach_func **funcs,
                                     int type, uint state_mask, void *arg);
int lock_plugin_data();
int unlock_plugin_data();

/**
  Initialize one plugin.
*/
bool plugin_early_load_one(int *argc, char **argv, const char *plugin);

#endif
