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

#include "mysql_priv.h"
#include <my_pthread.h>
#define REPORT_TO_LOG  1
#define REPORT_TO_USER 2

extern struct st_mysql_plugin *mysqld_builtins[];

char *opt_plugin_dir_ptr;
char opt_plugin_dir[FN_REFLEN];
const LEX_STRING plugin_type_names[MYSQL_MAX_PLUGIN_TYPE_NUM]=
{
  { (char *)STRING_WITH_LEN("UDF") },
  { (char *)STRING_WITH_LEN("STORAGE ENGINE") },
  { (char *)STRING_WITH_LEN("FTPARSER") }
};

plugin_type_init plugin_type_initialize[MYSQL_MAX_PLUGIN_TYPE_NUM]=
{
  0,ha_initialize_handlerton,0
};

static const char *plugin_interface_version_sym=
                   "_mysql_plugin_interface_version_";
static const char *sizeof_st_plugin_sym=
                   "_mysql_sizeof_struct_st_plugin_";
static const char *plugin_declarations_sym= "_mysql_plugin_declarations_";
static int min_plugin_interface_version= 0x0000;
/* Note that 'int version' must be the first field of every plugin
   sub-structure (plugin->info).
*/
static int min_plugin_info_interface_version[MYSQL_MAX_PLUGIN_TYPE_NUM]=
{
  0x0000,
  MYSQL_HANDLERTON_INTERFACE_VERSION,
  MYSQL_FTPARSER_INTERFACE_VERSION
};
static int cur_plugin_info_interface_version[MYSQL_MAX_PLUGIN_TYPE_NUM]=
{
  0x0000, /* UDF: not implemented */
  MYSQL_HANDLERTON_INTERFACE_VERSION,
  MYSQL_FTPARSER_INTERFACE_VERSION
};

static DYNAMIC_ARRAY plugin_dl_array;
static DYNAMIC_ARRAY plugin_array;
static HASH plugin_hash[MYSQL_MAX_PLUGIN_TYPE_NUM];
static rw_lock_t THR_LOCK_plugin;
static bool initialized= 0;

static struct st_plugin_dl *plugin_dl_find(const LEX_STRING *dl)
{
  uint i;
  DBUG_ENTER("plugin_dl_find");
  for (i= 0; i < plugin_dl_array.elements; i++)
  {
    struct st_plugin_dl *tmp= dynamic_element(&plugin_dl_array, i,
                                              struct st_plugin_dl *);
    if (tmp->ref_count &&
        ! my_strnncoll(files_charset_info,
                       (const uchar *)dl->str, dl->length,
                       (const uchar *)tmp->dl.str, tmp->dl.length))
      DBUG_RETURN(tmp);
  }
  DBUG_RETURN(0);
}


static st_plugin_dl *plugin_dl_insert_or_reuse(struct st_plugin_dl *plugin_dl)
{
  uint i;
  DBUG_ENTER("plugin_dl_insert_or_reuse");
  for (i= 0; i < plugin_dl_array.elements; i++)
  {
    struct st_plugin_dl *tmp= dynamic_element(&plugin_dl_array, i,
                                              struct st_plugin_dl *);
    if (! tmp->ref_count)
    {
      memcpy(tmp, plugin_dl, sizeof(struct st_plugin_dl));
      DBUG_RETURN(tmp);
    }
  }
  if (insert_dynamic(&plugin_dl_array, (gptr)plugin_dl))
    DBUG_RETURN(0);
  DBUG_RETURN(dynamic_element(&plugin_dl_array, plugin_dl_array.elements - 1,
                              struct st_plugin_dl *));
}

static inline void free_plugin_mem(struct st_plugin_dl *p)
{
#ifdef HAVE_DLOPEN
  if (p->handle)
    dlclose(p->handle);
#endif
  my_free(p->dl.str, MYF(MY_ALLOW_ZERO_PTR));
  if (p->version != MYSQL_PLUGIN_INTERFACE_VERSION)
    my_free((gptr)p->plugins, MYF(MY_ALLOW_ZERO_PTR));
}

static st_plugin_dl *plugin_dl_add(const LEX_STRING *dl, int report)
{
#ifdef HAVE_DLOPEN
  char dlpath[FN_REFLEN];
  uint plugin_dir_len, dummy_errors, dlpathlen;
  struct st_plugin_dl *tmp, plugin_dl;
  void *sym;
  DBUG_ENTER("plugin_dl_add");
  plugin_dir_len= strlen(opt_plugin_dir);
  /*
    Ensure that the dll doesn't have a path.
    This is done to ensure that only approved libraries from the
    plugin directory are used (to make this even remotely secure).
  */
  if (my_strchr(files_charset_info, dl->str, dl->str + dl->length, FN_LIBCHAR) ||
      dl->length > NAME_LEN ||
      plugin_dir_len + dl->length + 1 >= FN_REFLEN)
  {
    if (report & REPORT_TO_USER)
      my_error(ER_UDF_NO_PATHS, MYF(0));
    if (report & REPORT_TO_LOG)
      sql_print_error(ER(ER_UDF_NO_PATHS));
    DBUG_RETURN(0);
  }
  /* If this dll is already loaded just increase ref_count. */
  if ((tmp= plugin_dl_find(dl)))
  {
    tmp->ref_count++;
    DBUG_RETURN(tmp);
  }
  bzero(&plugin_dl, sizeof(plugin_dl));
  /* Compile dll path */
  dlpathlen=
    strxnmov(dlpath, sizeof(dlpath) - 1, opt_plugin_dir, "/", dl->str, NullS) -
    dlpath;
  plugin_dl.ref_count= 1;
  /* Open new dll handle */
  if (!(plugin_dl.handle= dlopen(dlpath, RTLD_NOW)))
  {
    const char *errmsg=dlerror();
    if (!strncmp(dlpath, errmsg, dlpathlen))
    { // if errmsg starts from dlpath, trim this prefix.
      errmsg+=dlpathlen;
      if (*errmsg == ':') errmsg++;
      if (*errmsg == ' ') errmsg++;
    }
    if (report & REPORT_TO_USER)
      my_error(ER_CANT_OPEN_LIBRARY, MYF(0), dlpath, errno, errmsg);
    if (report & REPORT_TO_LOG)
      sql_print_error(ER(ER_CANT_OPEN_LIBRARY), dlpath, errno, errmsg);
    DBUG_RETURN(0);
  }
  /* Determine interface version */
  if (!(sym= dlsym(plugin_dl.handle, plugin_interface_version_sym)))
  {
    free_plugin_mem(&plugin_dl);
    if (report & REPORT_TO_USER)
      my_error(ER_CANT_FIND_DL_ENTRY, MYF(0), plugin_interface_version_sym);
    if (report & REPORT_TO_LOG)
      sql_print_error(ER(ER_CANT_FIND_DL_ENTRY), plugin_interface_version_sym);
    DBUG_RETURN(0);
  }
  plugin_dl.version= *(int *)sym;
  /* Versioning */
  if (plugin_dl.version < min_plugin_interface_version ||
      (plugin_dl.version >> 8) > (MYSQL_PLUGIN_INTERFACE_VERSION >> 8))
  {
    free_plugin_mem(&plugin_dl);
    if (report & REPORT_TO_USER)
      my_error(ER_CANT_OPEN_LIBRARY, MYF(0), dlpath, 0,
               "plugin interface version mismatch");
    if (report & REPORT_TO_LOG)
      sql_print_error(ER(ER_CANT_OPEN_LIBRARY), dlpath, 0,
                      "plugin interface version mismatch");
    DBUG_RETURN(0);
  }
  /* Find plugin declarations */
  if (!(sym= dlsym(plugin_dl.handle, plugin_declarations_sym)))
  {
    free_plugin_mem(&plugin_dl);
    if (report & REPORT_TO_USER)
      my_error(ER_CANT_FIND_DL_ENTRY, MYF(0), plugin_declarations_sym);
    if (report & REPORT_TO_LOG)
      sql_print_error(ER(ER_CANT_FIND_DL_ENTRY), plugin_declarations_sym);
    DBUG_RETURN(0);
  }

  if (plugin_dl.version != MYSQL_PLUGIN_INTERFACE_VERSION)
  {
    int i;
    uint sizeof_st_plugin;
    struct st_mysql_plugin *old, *cur;
    char *ptr= (char *)sym;

    if ((sym= dlsym(plugin_dl.handle, sizeof_st_plugin_sym)))
      sizeof_st_plugin= *(int *)sym;
    else
    {
#ifdef ERROR_ON_NO_SIZEOF_PLUGIN_SYMBOL
      free_plugin_mem(&plugin_dl);
      if (report & REPORT_TO_USER)
        my_error(ER_CANT_FIND_DL_ENTRY, MYF(0), sizeof_st_plugin_sym);
      if (report & REPORT_TO_LOG)
        sql_print_error(ER(ER_CANT_FIND_DL_ENTRY), sizeof_st_plugin_sym);
      DBUG_RETURN(0);
#else
      /*
        When the following assert starts failing, we'll have to switch
        to the upper branch of the #ifdef
      */
      DBUG_ASSERT(min_plugin_interface_version == 0);
      sizeof_st_plugin= (int)offsetof(struct st_mysql_plugin, version);
#endif
    }

    for (i= 0;
         ((struct st_mysql_plugin *)(ptr+i*sizeof_st_plugin))->info;
         i++)
      /* no op */;

    cur= (struct st_mysql_plugin*)
          my_malloc(i*sizeof(struct st_mysql_plugin), MYF(MY_ZEROFILL|MY_WME));
    if (!cur)
    {
      free_plugin_mem(&plugin_dl);
      if (report & REPORT_TO_USER)
        my_error(ER_OUTOFMEMORY, MYF(0), plugin_dl.dl.length);
      if (report & REPORT_TO_LOG)
        sql_print_error(ER(ER_OUTOFMEMORY), plugin_dl.dl.length);
      DBUG_RETURN(0);
    }
    /*
      All st_plugin fields not initialized in the plugin explicitly, are
      set to 0. It matches C standard behaviour for struct initializers that
      have less values than the struct definition.
    */
    for (i=0;
         (old=(struct st_mysql_plugin *)(ptr+i*sizeof_st_plugin))->info;
         i++)
      memcpy(cur+i, old, min(sizeof(cur[i]), sizeof_st_plugin));

    sym= cur;
  }
  plugin_dl.plugins= (struct st_mysql_plugin *)sym;

  /* Duplicate and convert dll name */
  plugin_dl.dl.length= dl->length * files_charset_info->mbmaxlen + 1;
  if (! (plugin_dl.dl.str= my_malloc(plugin_dl.dl.length, MYF(0))))
  {
    free_plugin_mem(&plugin_dl);
    if (report & REPORT_TO_USER)
      my_error(ER_OUTOFMEMORY, MYF(0), plugin_dl.dl.length);
    if (report & REPORT_TO_LOG)
      sql_print_error(ER(ER_OUTOFMEMORY), plugin_dl.dl.length);
    DBUG_RETURN(0);
  }
  plugin_dl.dl.length= copy_and_convert(plugin_dl.dl.str, plugin_dl.dl.length,
    files_charset_info, dl->str, dl->length, system_charset_info,
    &dummy_errors);
  plugin_dl.dl.str[plugin_dl.dl.length]= 0;
  /* Add this dll to array */
  if (! (tmp= plugin_dl_insert_or_reuse(&plugin_dl)))
  {
    free_plugin_mem(&plugin_dl);
    if (report & REPORT_TO_USER)
      my_error(ER_OUTOFMEMORY, MYF(0), sizeof(struct st_plugin_dl));
    if (report & REPORT_TO_LOG)
      sql_print_error(ER(ER_OUTOFMEMORY), sizeof(struct st_plugin_dl));
    DBUG_RETURN(0);
  }
  DBUG_RETURN(tmp);
#else
  DBUG_ENTER("plugin_dl_add");
  if (report & REPORT_TO_USER)
    my_error(ER_FEATURE_DISABLED, MYF(0), "plugin", "HAVE_DLOPEN");
  if (report & REPORT_TO_LOG)
    sql_print_error(ER(ER_FEATURE_DISABLED), "plugin", "HAVE_DLOPEN");
  DBUG_RETURN(0);
#endif
}


static void plugin_dl_del(const LEX_STRING *dl)
{
#ifdef HAVE_DLOPEN
  uint i;
  DBUG_ENTER("plugin_dl_del");
  for (i= 0; i < plugin_dl_array.elements; i++)
  {
    struct st_plugin_dl *tmp= dynamic_element(&plugin_dl_array, i,
                                              struct st_plugin_dl *);
    if (tmp->ref_count &&
        ! my_strnncoll(files_charset_info,
                       (const uchar *)dl->str, dl->length,
                       (const uchar *)tmp->dl.str, tmp->dl.length))
    {
      /* Do not remove this element, unless no other plugin uses this dll. */
      if (! --tmp->ref_count)
      {
        free_plugin_mem(tmp);
        bzero(tmp, sizeof(struct st_plugin_dl));
      }
      break;
    }
  }
  DBUG_VOID_RETURN;
#endif
}


static struct st_plugin_int *plugin_find_internal(const LEX_STRING *name, int type)
{
  uint i;
  DBUG_ENTER("plugin_find_internal");
  if (! initialized)
    DBUG_RETURN(0);
  if (type == MYSQL_ANY_PLUGIN)
  {
    for (i= 0; i < MYSQL_MAX_PLUGIN_TYPE_NUM; i++)
    {
      struct st_plugin_int *plugin= (st_plugin_int *)
        hash_search(&plugin_hash[i], (const byte *)name->str, name->length);
      if (plugin)
        DBUG_RETURN(plugin);
    }
  }
  else
    DBUG_RETURN((st_plugin_int *)
        hash_search(&plugin_hash[type], (const byte *)name->str, name->length));
  DBUG_RETURN(0);
}


my_bool plugin_is_ready(const LEX_STRING *name, int type)
{
  my_bool rc= FALSE;
  struct st_plugin_int *plugin;
  DBUG_ENTER("plugin_is_ready");
  rw_rdlock(&THR_LOCK_plugin);
  if ((plugin= plugin_find_internal(name, type)) &&
      plugin->state == PLUGIN_IS_READY)
    rc= TRUE;
  rw_unlock(&THR_LOCK_plugin);
  DBUG_RETURN(rc);
}


struct st_plugin_int *plugin_lock(const LEX_STRING *name, int type)
{
  struct st_plugin_int *rc;
  DBUG_ENTER("plugin_lock");
  rw_wrlock(&THR_LOCK_plugin);
  if ((rc= plugin_find_internal(name, type)))
  {
    if (rc->state == PLUGIN_IS_READY || rc->state == PLUGIN_IS_UNINITIALIZED)
      rc->ref_count++;
    else
      rc= 0;
  }
  rw_unlock(&THR_LOCK_plugin);
  DBUG_RETURN(rc);
}


static st_plugin_int *plugin_insert_or_reuse(struct st_plugin_int *plugin)
{
  uint i;
  DBUG_ENTER("plugin_insert_or_reuse");
  for (i= 0; i < plugin_array.elements; i++)
  {
    struct st_plugin_int *tmp= dynamic_element(&plugin_array, i,
                                               struct st_plugin_int *);
    if (tmp->state == PLUGIN_IS_FREED)
    {
      memcpy(tmp, plugin, sizeof(struct st_plugin_int));
      DBUG_RETURN(tmp);
    }
  }
  if (insert_dynamic(&plugin_array, (gptr)plugin))
    DBUG_RETURN(0);
  DBUG_RETURN(dynamic_element(&plugin_array, plugin_array.elements - 1,
                              struct st_plugin_int *));
}

static my_bool plugin_add(const LEX_STRING *name, const LEX_STRING *dl, int report)
{
  struct st_plugin_int tmp;
  struct st_mysql_plugin *plugin;
  DBUG_ENTER("plugin_add");
  if (plugin_find_internal(name, MYSQL_ANY_PLUGIN))
  {
    if (report & REPORT_TO_USER)
      my_error(ER_UDF_EXISTS, MYF(0), name->str);
    if (report & REPORT_TO_LOG)
      sql_print_error(ER(ER_UDF_EXISTS), name->str);
    DBUG_RETURN(TRUE);
  }
  if (! (tmp.plugin_dl= plugin_dl_add(dl, report)))
    DBUG_RETURN(TRUE);
  /* Find plugin by name */
  for (plugin= tmp.plugin_dl->plugins; plugin->info; plugin++)
  {
    uint name_len= strlen(plugin->name);
    if (plugin->type >= 0 && plugin->type < MYSQL_MAX_PLUGIN_TYPE_NUM &&
        ! my_strnncoll(system_charset_info,
                       (const uchar *)name->str, name->length,
                       (const uchar *)plugin->name,
                       name_len))
    {
      struct st_plugin_int *tmp_plugin_ptr;
      if (*(int*)plugin->info <
          min_plugin_info_interface_version[plugin->type] ||
          ((*(int*)plugin->info) >> 8) >
          (cur_plugin_info_interface_version[plugin->type] >> 8))
      {
        char buf[256];
        strxnmov(buf, sizeof(buf) - 1, "API version for ",
                 plugin_type_names[plugin->type].str,
                 " plugin is too different", NullS);
        if (report & REPORT_TO_USER)
          my_error(ER_CANT_OPEN_LIBRARY, MYF(0), dl->str, 0, buf);
        if (report & REPORT_TO_LOG)
          sql_print_error(ER(ER_CANT_OPEN_LIBRARY), dl->str, 0, buf);
        goto err;
      }
      tmp.plugin= plugin;
      tmp.name.str= (char *)plugin->name;
      tmp.name.length= name_len;
      tmp.ref_count= 0;
      tmp.state= PLUGIN_IS_UNINITIALIZED;
      if (plugin->status_vars)
      {
        SHOW_VAR array[2]= {
          {plugin->name, (char*)plugin->status_vars, SHOW_ARRAY},
          {0, 0, SHOW_UNDEF}
        };
        if (add_status_vars(array)) // add_status_vars makes a copy
          goto err;
      }
      if (! (tmp_plugin_ptr= plugin_insert_or_reuse(&tmp)))
        goto err;
      if (my_hash_insert(&plugin_hash[plugin->type], (byte*)tmp_plugin_ptr))
      {
        tmp_plugin_ptr->state= PLUGIN_IS_FREED;
        goto err;
      }
      DBUG_RETURN(FALSE);
    }
  }
  if (report & REPORT_TO_USER)
    my_error(ER_CANT_FIND_DL_ENTRY, MYF(0), name->str);
  if (report & REPORT_TO_LOG)
    sql_print_error(ER(ER_CANT_FIND_DL_ENTRY), name->str);
err:
  if (plugin->status_vars)
  {
    SHOW_VAR array[2]= {
      {plugin->name, (char*)plugin->status_vars, SHOW_ARRAY},
      {0, 0, SHOW_UNDEF}
    };
    remove_status_vars(array);
  }
  plugin_dl_del(dl);
  DBUG_RETURN(TRUE);
}


static void plugin_del(const LEX_STRING *name)
{
  uint i;
  struct st_plugin_int *plugin;
  DBUG_ENTER("plugin_del");
  if ((plugin= plugin_find_internal(name, MYSQL_ANY_PLUGIN)))
  {
    if (plugin->plugin->status_vars)
    {
      SHOW_VAR array[2]= {
        {plugin->plugin->name, (char*)plugin->plugin->status_vars, SHOW_ARRAY},
        {0, 0, SHOW_UNDEF}
      };
      remove_status_vars(array);
    }
    hash_delete(&plugin_hash[plugin->plugin->type], (byte*)plugin);
    plugin_dl_del(&plugin->plugin_dl->dl);
    plugin->state= PLUGIN_IS_FREED;
  }
  DBUG_VOID_RETURN;
}


void plugin_unlock(struct st_plugin_int *plugin)
{
  DBUG_ENTER("plugin_unlock");
  rw_wrlock(&THR_LOCK_plugin);
  DBUG_ASSERT(plugin && plugin->ref_count);
  plugin->ref_count--;
  if (plugin->state == PLUGIN_IS_DELETED && ! plugin->ref_count)
  {
    if (plugin->plugin->deinit)
      plugin->plugin->deinit();
    plugin_del(&plugin->name);
  }
  rw_unlock(&THR_LOCK_plugin);
  DBUG_VOID_RETURN;
}


static int plugin_initialize(struct st_plugin_int *plugin)
{
  DBUG_ENTER("plugin_initialize");

  if (plugin->plugin->init)
  {
    if (plugin->plugin->init())
    {
      sql_print_error("Plugin '%s' init function returned error.",
                      plugin->name.str);
      goto err;
    }
  }
  if (plugin_type_initialize[plugin->plugin->type] &&
      (*plugin_type_initialize[plugin->plugin->type])(plugin))
  {
    sql_print_error("Plugin '%s' registration as a %s failed.",
                    plugin->name.str, plugin_type_names[plugin->plugin->type]);
    goto err;
  }

  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}

static int plugin_finalize(THD *thd, struct st_plugin_int *plugin)
{
  int rc;
  DBUG_ENTER("plugin_finalize");

  if (plugin->ref_count)
  {
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, 0,
                 "Plugin is busy and will be uninstalled on shutdown");
    goto err;
  }

  switch (plugin->plugin->type)
  {
  case MYSQL_STORAGE_ENGINE_PLUGIN:
    if (ha_finalize_handlerton(plugin))
    {
      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, 0,
                   "Storage engine shutdown failed. "
                   "It will be uninstalled on shutdown");
      sql_print_warning("Storage engine '%s' shutdown failed. "
                        "It will be uninstalled on shutdown", plugin->name.str);
      goto err;
    }
    break;
  default:
    break;
  }

  if (plugin->plugin->deinit)
  {
    if ((rc= plugin->plugin->deinit()))
    {
      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, 0,
                   "Plugin deinit failed. "
                   "It will be uninstalled on shutdown");
      sql_print_warning("Plugin '%s' deinit failed. "
                        "It will be uninstalled on shutdown", plugin->name.str);
      goto err;
    }
  }

  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}

static void plugin_call_initializer(void)
{
  uint i;
  DBUG_ENTER("plugin_call_initializer");
  for (i= 0; i < plugin_array.elements; i++)
  {
    struct st_plugin_int *tmp= dynamic_element(&plugin_array, i,
                                               struct st_plugin_int *);
    if (tmp->state == PLUGIN_IS_UNINITIALIZED)
    {
      if (plugin_initialize(tmp))
        plugin_del(&tmp->name);
      else
        tmp->state= PLUGIN_IS_READY;
    }
  }
  DBUG_VOID_RETURN;
}


static void plugin_call_deinitializer(void)
{
  uint i;
  DBUG_ENTER("plugin_call_deinitializer");
  for (i= 0; i < plugin_array.elements; i++)
  {
    struct st_plugin_int *tmp= dynamic_element(&plugin_array, i,
                                               struct st_plugin_int *);
    if (tmp->state == PLUGIN_IS_READY)
    {
      if (tmp->plugin->deinit)
      {
        DBUG_PRINT("info", ("Deinitializing plugin: '%s'", tmp->name.str));
        if (tmp->plugin->deinit())
        {
          DBUG_PRINT("warning", ("Plugin '%s' deinit function returned error.",
                                 tmp->name.str));
        }
      }
      tmp->state= PLUGIN_IS_UNINITIALIZED;
    }
  }
  DBUG_VOID_RETURN;
}


static byte *get_hash_key(const byte *buff, uint *length,
                   my_bool not_used __attribute__((unused)))
{
  struct st_plugin_int *plugin= (st_plugin_int *)buff;
  *length= (uint)plugin->name.length;
  return((byte *)plugin->name.str);
}


int plugin_init(void)
{
  int i;
  struct st_mysql_plugin **builtins;
  struct st_mysql_plugin *plugin;
  DBUG_ENTER("plugin_init");

  if (initialized)
    DBUG_RETURN(0);

  my_rwlock_init(&THR_LOCK_plugin, NULL);

  if (my_init_dynamic_array(&plugin_dl_array,
                            sizeof(struct st_plugin_dl),16,16) ||
      my_init_dynamic_array(&plugin_array,
                            sizeof(struct st_plugin_int),16,16))
    goto err;

  for (i= 0; i < MYSQL_MAX_PLUGIN_TYPE_NUM; i++)
  {
    if (hash_init(&plugin_hash[i], system_charset_info, 16, 0, 0,
                  get_hash_key, NULL, 0))
      goto err;
  }

  /* Register all the built-in plugins */
  for (builtins= mysqld_builtins; *builtins; builtins++)
  {
    for (plugin= *builtins; plugin->info; plugin++)
    {
      if (plugin_register_builtin(plugin))
        goto err;
      struct st_plugin_int *tmp=dynamic_element(&plugin_array,
                                                plugin_array.elements-1,
                                                struct st_plugin_int *);
      if (plugin_initialize(tmp))
        goto err;
      tmp->state= PLUGIN_IS_READY;
    }
  }

  initialized= 1;

  DBUG_RETURN(0);

err:
  DBUG_RETURN(1);
}


my_bool plugin_register_builtin(struct st_mysql_plugin *plugin)
{
  struct st_plugin_int tmp;
  DBUG_ENTER("plugin_register_builtin");

  tmp.plugin= plugin;
  tmp.name.str= (char *)plugin->name;
  tmp.name.length= strlen(plugin->name);
  tmp.state= PLUGIN_IS_UNINITIALIZED;

  /* Cannot be unloaded */
  tmp.ref_count= 1;
  tmp.plugin_dl= 0;

  if (insert_dynamic(&plugin_array, (gptr)&tmp))
    DBUG_RETURN(1);

  if (my_hash_insert(&plugin_hash[plugin->type],
                     (byte*)dynamic_element(&plugin_array,
                                            plugin_array.elements - 1,
                                            struct st_plugin_int *)))
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}


void plugin_load(void)
{
  TABLE_LIST tables;
  TABLE *table;
  READ_RECORD read_record_info;
  int error, i;
  MEM_ROOT mem;
  THD *new_thd;
  DBUG_ENTER("plugin_load");

  DBUG_ASSERT(initialized);

  if (!(new_thd= new THD))
  {
    sql_print_error("Can't allocate memory for plugin structures");
    delete new_thd;
    DBUG_VOID_RETURN;
  }
  init_sql_alloc(&mem, 1024, 0);
  new_thd->thread_stack= (char*) &tables;
  new_thd->store_globals();
  new_thd->db= my_strdup("mysql", MYF(0));
  new_thd->db_length= 5;
  bzero((gptr)&tables, sizeof(tables));
  tables.alias= tables.table_name= (char*)"plugin";
  tables.lock_type= TL_READ;
  tables.db= new_thd->db;
  if (simple_open_n_lock_tables(new_thd, &tables))
  {
    DBUG_PRINT("error",("Can't open plugin table"));
    sql_print_error("Can't open the mysql.plugin table. Please run the mysql_upgrade script to create it.");
    goto end;
  }
  table= tables.table;
  init_read_record(&read_record_info, new_thd, table, NULL, 1, 0);
  table->use_all_columns();
  while (!(error= read_record_info.read_record(&read_record_info)))
  {
    DBUG_PRINT("info", ("init plugin record"));
    LEX_STRING name, dl;
    name.str= get_field(&mem, table->field[0]);
    name.length= strlen(name.str);
    dl.str= get_field(&mem, table->field[1]);
    dl.length= strlen(dl.str);
    if (plugin_add(&name, &dl, REPORT_TO_LOG))
      DBUG_PRINT("warning", ("Couldn't load plugin named '%s' with soname '%s'.",
                             name.str, dl.str));
  }
  plugin_call_initializer();
  if (error > 0)
    sql_print_error(ER(ER_GET_ERRNO), my_errno);
  end_read_record(&read_record_info);
  new_thd->version--; // Force close to free memory
end:
  free_root(&mem, MYF(0));
  close_thread_tables(new_thd);
  delete new_thd;
  /* Remember that we don't have a THD */
  my_pthread_setspecific_ptr(THR_THD, 0);
  DBUG_VOID_RETURN;
}


void plugin_free(void)
{
  uint i;
  DBUG_ENTER("plugin_free");
  plugin_call_deinitializer();
  for (i= 0; i < MYSQL_MAX_PLUGIN_TYPE_NUM; i++)
    hash_free(&plugin_hash[i]);
  delete_dynamic(&plugin_array);
  for (i= 0; i < plugin_dl_array.elements; i++)
  {
    struct st_plugin_dl *tmp= dynamic_element(&plugin_dl_array, i,
                                              struct st_plugin_dl *);
    free_plugin_mem(tmp);
  }
  delete_dynamic(&plugin_dl_array);
  if (initialized)
  {
    initialized= 0;
    rwlock_destroy(&THR_LOCK_plugin);
  }
  DBUG_VOID_RETURN;
}


my_bool mysql_install_plugin(THD *thd, const LEX_STRING *name, const LEX_STRING *dl)
{
  TABLE_LIST tables;
  TABLE *table;
  int error;
  struct st_plugin_int *tmp;
  DBUG_ENTER("mysql_install_plugin");

  bzero(&tables, sizeof(tables));
  tables.db= (char *)"mysql";
  tables.table_name= tables.alias= (char *)"plugin";
  if (check_table_access(thd, INSERT_ACL, &tables, 0))
    DBUG_RETURN(TRUE);

  /* need to open before acquiring THR_LOCK_plugin or it will deadlock */
  if (! (table = open_ltable(thd, &tables, TL_WRITE)))
    DBUG_RETURN(TRUE);

  rw_wrlock(&THR_LOCK_plugin);
  if (plugin_add(name, dl, REPORT_TO_USER))
    goto err;
  tmp= plugin_find_internal(name, MYSQL_ANY_PLUGIN);

  if (plugin_initialize(tmp))
  {
    my_error(ER_CANT_INITIALIZE_UDF, MYF(0), name->str,
             "Plugin initialization function failed.");
    goto err;
  }

  tmp->state= PLUGIN_IS_READY;

  table->use_all_columns();
  restore_record(table, s->default_values);
  table->field[0]->store(name->str, name->length, system_charset_info);
  table->field[1]->store(dl->str, dl->length, files_charset_info);
  error= table->file->ha_write_row(table->record[0]);
  if (error)
  {
    table->file->print_error(error, MYF(0));
    goto deinit;
  }

  rw_unlock(&THR_LOCK_plugin);
  DBUG_RETURN(FALSE);
deinit:
  if (tmp->plugin->deinit)
    tmp->plugin->deinit();
err:
  plugin_del(name);
  rw_unlock(&THR_LOCK_plugin);
  DBUG_RETURN(TRUE);
}


my_bool mysql_uninstall_plugin(THD *thd, const LEX_STRING *name)
{
  TABLE *table;
  TABLE_LIST tables;
  struct st_plugin_int *plugin;
  DBUG_ENTER("mysql_uninstall_plugin");

  bzero(&tables, sizeof(tables));
  tables.db= (char *)"mysql";
  tables.table_name= tables.alias= (char *)"plugin";

  /* need to open before acquiring THR_LOCK_plugin or it will deadlock */
  if (! (table= open_ltable(thd, &tables, TL_WRITE)))
    DBUG_RETURN(TRUE);

  rw_wrlock(&THR_LOCK_plugin);
  if (!(plugin= plugin_find_internal(name, MYSQL_ANY_PLUGIN)))
  {
    my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "PLUGIN", name->str);
    goto err;
  }
  if (!plugin->plugin_dl)
  {
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, 0,
                 "Built-in plugins cannot be deleted,.");
    my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "PLUGIN", name->str);
    goto err;
  }

  if (!plugin_finalize(thd, plugin))
    plugin_del(name);
  else
    plugin->state= PLUGIN_IS_DELETED;

  table->use_all_columns();
  table->field[0]->store(name->str, name->length, system_charset_info);
  if (! table->file->index_read_idx(table->record[0], 0,
                                    (byte *)table->field[0]->ptr,
                                    table->key_info[0].key_length,
                                    HA_READ_KEY_EXACT))
  {
    int error;
    if ((error= table->file->ha_delete_row(table->record[0])))
    {
      table->file->print_error(error, MYF(0));
      goto err;
    }
  }
  rw_unlock(&THR_LOCK_plugin);
  DBUG_RETURN(FALSE);
err:
  rw_unlock(&THR_LOCK_plugin);
  DBUG_RETURN(TRUE);
}


my_bool plugin_foreach(THD *thd, plugin_foreach_func *func,
                       int type, void *arg)
{
  uint idx;
  struct st_plugin_int *plugin;
  DBUG_ENTER("plugin_foreach");
  rw_rdlock(&THR_LOCK_plugin);

  if (type == MYSQL_ANY_PLUGIN)
  {
    for (idx= 0; idx < plugin_array.elements; idx++)
    {
      plugin= dynamic_element(&plugin_array, idx, struct st_plugin_int *);

      /* FREED records may have garbage pointers */
      if ((plugin->state != PLUGIN_IS_FREED) &&
          func(thd, plugin, arg))
        goto err;
    }
  }
  else
  {
    HASH *hash= &plugin_hash[type];
    for (idx= 0; idx < hash->records; idx++)
    {
      plugin= (struct st_plugin_int *) hash_element(hash, idx);
      if ((plugin->state != PLUGIN_IS_FREED) &&
          (plugin->state != PLUGIN_IS_DELETED) &&
          func(thd, plugin, arg))
        goto err;
    }
  }

  rw_unlock(&THR_LOCK_plugin);
  DBUG_RETURN(FALSE);
err:
  rw_unlock(&THR_LOCK_plugin);
  DBUG_RETURN(TRUE);
}
