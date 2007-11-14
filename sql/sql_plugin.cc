/* Copyright (C) 2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysql_priv.h"
#include <my_pthread.h>
#include <my_getopt.h>
#define REPORT_TO_LOG  1
#define REPORT_TO_USER 2

#ifdef DBUG_OFF
#define plugin_ref_to_int(A) A
#define plugin_int_to_ref(A) A
#else
#define plugin_ref_to_int(A) (A ? A[0] : NULL)
#define plugin_int_to_ref(A) &(A)
#endif

extern struct st_mysql_plugin *mysqld_builtins[];

char *opt_plugin_load= NULL;
char *opt_plugin_dir_ptr;
char opt_plugin_dir[FN_REFLEN];
/*
  When you ad a new plugin type, add both a string and make sure that the
  init and deinit array are correctly updated.
*/
const LEX_STRING plugin_type_names[MYSQL_MAX_PLUGIN_TYPE_NUM]=
{
  { C_STRING_WITH_LEN("UDF") },
  { C_STRING_WITH_LEN("STORAGE ENGINE") },
  { C_STRING_WITH_LEN("FTPARSER") },
  { C_STRING_WITH_LEN("DAEMON") },
  { C_STRING_WITH_LEN("INFORMATION SCHEMA") }
};

extern int initialize_schema_table(st_plugin_int *plugin);
extern int finalize_schema_table(st_plugin_int *plugin);

/*
  The number of elements in both plugin_type_initialize and
  plugin_type_deinitialize should equal to the number of plugins
  defined.
*/
plugin_type_init plugin_type_initialize[MYSQL_MAX_PLUGIN_TYPE_NUM]=
{
  0,ha_initialize_handlerton,0,0,initialize_schema_table
};

plugin_type_init plugin_type_deinitialize[MYSQL_MAX_PLUGIN_TYPE_NUM]=
{
  0,ha_finalize_handlerton,0,0,finalize_schema_table
};

#ifdef HAVE_DLOPEN
static const char *plugin_interface_version_sym=
                   "_mysql_plugin_interface_version_";
static const char *sizeof_st_plugin_sym=
                   "_mysql_sizeof_struct_st_plugin_";
static const char *plugin_declarations_sym= "_mysql_plugin_declarations_";
static int min_plugin_interface_version= MYSQL_PLUGIN_INTERFACE_VERSION & ~0xFF;
#endif

/* Note that 'int version' must be the first field of every plugin
   sub-structure (plugin->info).
*/
static int min_plugin_info_interface_version[MYSQL_MAX_PLUGIN_TYPE_NUM]=
{
  0x0000,
  MYSQL_HANDLERTON_INTERFACE_VERSION,
  MYSQL_FTPARSER_INTERFACE_VERSION,
  MYSQL_DAEMON_INTERFACE_VERSION,
  MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION
};
static int cur_plugin_info_interface_version[MYSQL_MAX_PLUGIN_TYPE_NUM]=
{
  0x0000, /* UDF: not implemented */
  MYSQL_HANDLERTON_INTERFACE_VERSION,
  MYSQL_FTPARSER_INTERFACE_VERSION,
  MYSQL_DAEMON_INTERFACE_VERSION,
  MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION
};

static bool initialized= 0;

/*
  A mutex LOCK_plugin must be acquired before accessing the
  following variables/structures.
  We are always manipulating ref count, so a rwlock here is unneccessary.
*/
pthread_mutex_t LOCK_plugin;
static DYNAMIC_ARRAY plugin_dl_array;
static DYNAMIC_ARRAY plugin_array;
static HASH plugin_hash[MYSQL_MAX_PLUGIN_TYPE_NUM];
static bool reap_needed= false;
static int plugin_array_version=0;

/*
  write-lock on LOCK_system_variables_hash is required before modifying
  the following variables/structures
*/
static MEM_ROOT plugin_mem_root;
static uint global_variables_dynamic_size= 0;
static HASH bookmark_hash;


/*
  hidden part of opaque value passed to variable check functions.
  Used to provide a object-like structure to non C++ consumers.
*/
struct st_item_value_holder : public st_mysql_value
{
  Item *item;
};


/*
  stored in bookmark_hash, this structure is never removed from the
  hash and is used to mark a single offset for a thd local variable
  even if plugins have been uninstalled and reinstalled, repeatedly.
  This structure is allocated from plugin_mem_root.

  The key format is as follows:
    1 byte         - variable type code
    name_len bytes - variable name
    '\0'           - end of key
*/
struct st_bookmark
{
  uint name_len;
  int offset;
  uint version;
  char key[1];
};


/*
  skeleton of a plugin variable - portion of structure common to all.
*/
struct st_mysql_sys_var
{
  MYSQL_PLUGIN_VAR_HEADER;
};


/*
  sys_var class for access to all plugin variables visible to the user
*/
class sys_var_pluginvar: public sys_var
{
public:
  struct st_plugin_int *plugin;
  struct st_mysql_sys_var *plugin_var;

  static void *operator new(size_t size, MEM_ROOT *mem_root)
  { return (void*) alloc_root(mem_root, (uint) size); }
  static void operator delete(void *ptr_arg,size_t size)
  { TRASH(ptr_arg, size); }

  sys_var_pluginvar(const char *name_arg,
                    struct st_mysql_sys_var *plugin_var_arg)
    :sys_var(name_arg), plugin_var(plugin_var_arg) {}
  sys_var_pluginvar *cast_pluginvar() { return this; }
  bool is_readonly() const { return plugin_var->flags & PLUGIN_VAR_READONLY; }
  bool check_type(enum_var_type type)
  { return !(plugin_var->flags & PLUGIN_VAR_THDLOCAL) && type != OPT_GLOBAL; }
  bool check_update_type(Item_result type);
  SHOW_TYPE show_type();
  uchar* real_value_ptr(THD *thd, enum_var_type type);
  TYPELIB* plugin_var_typelib(void);
  uchar* value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  bool check(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  bool update(THD *thd, set_var *var);
};


/* prototypes */
static void plugin_load(MEM_ROOT *tmp_root, int *argc, char **argv);
static bool plugin_load_list(MEM_ROOT *tmp_root, int *argc, char **argv,
                             const char *list);
static int test_plugin_options(MEM_ROOT *, struct st_plugin_int *,
                               int *, char **, my_bool);
static bool register_builtin(struct st_mysql_plugin *, struct st_plugin_int *,
                             struct st_plugin_int **);
static void unlock_variables(THD *thd, struct system_variables *vars);
static void cleanup_variables(THD *thd, struct system_variables *vars);
static void plugin_vars_free_values(sys_var *vars);
static void plugin_opt_set_limits(struct my_option *options,
                                  const struct st_mysql_sys_var *opt);
#define my_intern_plugin_lock(A,B) intern_plugin_lock(A,B CALLER_INFO)
#define my_intern_plugin_lock_ci(A,B) intern_plugin_lock(A,B ORIG_CALLER_INFO)
static plugin_ref intern_plugin_lock(LEX *lex, plugin_ref plugin
                                     CALLER_INFO_PROTO);
static void intern_plugin_unlock(LEX *lex, plugin_ref plugin);
static void reap_plugins(void);


/* declared in set_var.cc */
extern sys_var *intern_find_sys_var(const char *str, uint length, bool no_error);

#ifdef EMBEDDED_LIBRARY
/* declared in sql_base.cc */
extern bool check_if_table_exists(THD *thd, TABLE_LIST *table, bool *exists);
#endif /* EMBEDDED_LIBRARY */


/****************************************************************************
  Value type thunks, allows the C world to play in the C++ world
****************************************************************************/

static int item_value_type(struct st_mysql_value *value)
{
  switch (((st_item_value_holder*)value)->item->result_type()) {
  case INT_RESULT:
    return MYSQL_VALUE_TYPE_INT;
  case REAL_RESULT:
    return MYSQL_VALUE_TYPE_REAL;
  default:
    return MYSQL_VALUE_TYPE_STRING;
  }
}

static const char *item_val_str(struct st_mysql_value *value,
                                char *buffer, int *length)
{
  String str(buffer, *length, system_charset_info), *res;
  if (!(res= ((st_item_value_holder*)value)->item->val_str(&str)))
    return NULL;
  *length= res->length();
  if (res->c_ptr_quick() == buffer)
    return buffer;

  /*
    Lets be nice and create a temporary string since the
    buffer was too small
  */
  return current_thd->strmake(res->c_ptr_quick(), res->length());
}


static int item_val_int(struct st_mysql_value *value, long long *buf)
{
  Item *item= ((st_item_value_holder*)value)->item;
  *buf= item->val_int();
  if (item->is_null())
    return 1;
  return 0;
}


static int item_val_real(struct st_mysql_value *value, double *buf)
{
  Item *item= ((st_item_value_holder*)value)->item;
  *buf= item->val_real();
  if (item->is_null())
    return 1;
  return 0;
}


/****************************************************************************
  Plugin support code
****************************************************************************/

#ifdef HAVE_DLOPEN

static struct st_plugin_dl *plugin_dl_find(const LEX_STRING *dl)
{
  uint i;
  struct st_plugin_dl *tmp;
  DBUG_ENTER("plugin_dl_find");
  for (i= 0; i < plugin_dl_array.elements; i++)
  {
    tmp= *dynamic_element(&plugin_dl_array, i, struct st_plugin_dl **);
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
  struct st_plugin_dl *tmp;
  DBUG_ENTER("plugin_dl_insert_or_reuse");
  for (i= 0; i < plugin_dl_array.elements; i++)
  {
    tmp= *dynamic_element(&plugin_dl_array, i, struct st_plugin_dl **);
    if (! tmp->ref_count)
    {
      memcpy(tmp, plugin_dl, sizeof(struct st_plugin_dl));
      DBUG_RETURN(tmp);
    }
  }
  if (insert_dynamic(&plugin_dl_array, (uchar*)&plugin_dl))
    DBUG_RETURN(0);
  tmp= *dynamic_element(&plugin_dl_array, plugin_dl_array.elements - 1,
                        struct st_plugin_dl **)=
      (struct st_plugin_dl *) memdup_root(&plugin_mem_root, (uchar*)plugin_dl,
                                           sizeof(struct st_plugin_dl));
  DBUG_RETURN(tmp);
}
#endif /* HAVE_DLOPEN */


static inline void free_plugin_mem(struct st_plugin_dl *p)
{
#ifdef HAVE_DLOPEN
  if (p->handle)
    dlclose(p->handle);
#endif
  my_free(p->dl.str, MYF(MY_ALLOW_ZERO_PTR));
  if (p->version != MYSQL_PLUGIN_INTERFACE_VERSION)
    my_free((uchar*)p->plugins, MYF(MY_ALLOW_ZERO_PTR));
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
      check_string_char_length((LEX_STRING *) dl, "", NAME_CHAR_LEN,
                               system_charset_info, 1) ||
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
  if (! (plugin_dl.dl.str= (char*) my_malloc(plugin_dl.dl.length, MYF(0))))
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

  safe_mutex_assert_owner(&LOCK_plugin);

  for (i= 0; i < plugin_dl_array.elements; i++)
  {
    struct st_plugin_dl *tmp= *dynamic_element(&plugin_dl_array, i,
                                               struct st_plugin_dl **);
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

  safe_mutex_assert_owner(&LOCK_plugin);

  if (type == MYSQL_ANY_PLUGIN)
  {
    for (i= 0; i < MYSQL_MAX_PLUGIN_TYPE_NUM; i++)
    {
      struct st_plugin_int *plugin= (st_plugin_int *)
        hash_search(&plugin_hash[i], (const uchar *)name->str, name->length);
      if (plugin)
        DBUG_RETURN(plugin);
    }
  }
  else
    DBUG_RETURN((st_plugin_int *)
        hash_search(&plugin_hash[type], (const uchar *)name->str, name->length));
  DBUG_RETURN(0);
}


static SHOW_COMP_OPTION plugin_status(const LEX_STRING *name, int type)
{
  SHOW_COMP_OPTION rc= SHOW_OPTION_NO;
  struct st_plugin_int *plugin;
  DBUG_ENTER("plugin_is_ready");
  pthread_mutex_lock(&LOCK_plugin);
  if ((plugin= plugin_find_internal(name, type)))
  {
    rc= SHOW_OPTION_DISABLED;
    if (plugin->state == PLUGIN_IS_READY)
      rc= SHOW_OPTION_YES;
  }
  pthread_mutex_unlock(&LOCK_plugin);
  DBUG_RETURN(rc);
}


bool plugin_is_ready(const LEX_STRING *name, int type)
{
  bool rc= FALSE;
  if (plugin_status(name, type) == SHOW_OPTION_YES)
    rc= TRUE;
  return rc;
}


SHOW_COMP_OPTION sys_var_have_plugin::get_option()
{
  LEX_STRING plugin_name= { (char *) plugin_name_str, plugin_name_len };
  return plugin_status(&plugin_name, plugin_type);
}


static plugin_ref intern_plugin_lock(LEX *lex, plugin_ref rc CALLER_INFO_PROTO)
{
  st_plugin_int *pi= plugin_ref_to_int(rc);
  DBUG_ENTER("intern_plugin_lock");

  safe_mutex_assert_owner(&LOCK_plugin);

  if (pi->state & (PLUGIN_IS_READY | PLUGIN_IS_UNINITIALIZED))
  {
    plugin_ref plugin;
#ifdef DBUG_OFF
    /* built-in plugins don't need ref counting */
    if (!pi->plugin_dl)
      DBUG_RETURN(pi);

    plugin= pi;
#else
    /*
      For debugging, we do an additional malloc which allows the
      memory manager and/or valgrind to track locked references and
      double unlocks to aid resolving reference counting.problems.
    */
    if (!(plugin= (plugin_ref) my_malloc_ci(sizeof(pi), MYF(MY_WME))))
      DBUG_RETURN(NULL);

    *plugin= pi;
#endif
    pi->ref_count++;
    DBUG_PRINT("info",("thd: 0x%lx, plugin: \"%s\", ref_count: %d",
                       (long) current_thd, pi->name.str, pi->ref_count));

    if (lex)
      insert_dynamic(&lex->plugins, (uchar*)&plugin);
    DBUG_RETURN(plugin);
  }
  DBUG_RETURN(NULL);
}


plugin_ref plugin_lock(THD *thd, plugin_ref *ptr CALLER_INFO_PROTO)
{
  LEX *lex= thd ? thd->lex : 0;
  plugin_ref rc;
  DBUG_ENTER("plugin_lock");
  pthread_mutex_lock(&LOCK_plugin);
  rc= my_intern_plugin_lock_ci(lex, *ptr);
  pthread_mutex_unlock(&LOCK_plugin);
  DBUG_RETURN(rc);
}


plugin_ref plugin_lock_by_name(THD *thd, const LEX_STRING *name, int type
                               CALLER_INFO_PROTO)
{
  LEX *lex= thd ? thd->lex : 0;
  plugin_ref rc= NULL;
  st_plugin_int *plugin;
  DBUG_ENTER("plugin_lock_by_name");
  pthread_mutex_lock(&LOCK_plugin);
  if ((plugin= plugin_find_internal(name, type)))
    rc= my_intern_plugin_lock_ci(lex, plugin_int_to_ref(plugin));
  pthread_mutex_unlock(&LOCK_plugin);
  DBUG_RETURN(rc);
}


static st_plugin_int *plugin_insert_or_reuse(struct st_plugin_int *plugin)
{
  uint i;
  struct st_plugin_int *tmp;
  DBUG_ENTER("plugin_insert_or_reuse");
  for (i= 0; i < plugin_array.elements; i++)
  {
    tmp= *dynamic_element(&plugin_array, i, struct st_plugin_int **);
    if (tmp->state == PLUGIN_IS_FREED)
    {
      memcpy(tmp, plugin, sizeof(struct st_plugin_int));
      DBUG_RETURN(tmp);
    }
  }
  if (insert_dynamic(&plugin_array, (uchar*)&plugin))
    DBUG_RETURN(0);
  tmp= *dynamic_element(&plugin_array, plugin_array.elements - 1,
                        struct st_plugin_int **)=
       (struct st_plugin_int *) memdup_root(&plugin_mem_root, (uchar*)plugin,
                                            sizeof(struct st_plugin_int));
  DBUG_RETURN(tmp);
}


/*
  NOTE
    Requires that a write-lock is held on LOCK_system_variables_hash
*/
static bool plugin_add(MEM_ROOT *tmp_root,
                       const LEX_STRING *name, const LEX_STRING *dl,
                       int *argc, char **argv, int report)
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
  /* Clear the whole struct to catch future extensions. */
  bzero((char*) &tmp, sizeof(tmp));
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
      if (!test_plugin_options(tmp_root, &tmp, argc, argv, true))
      {
        if ((tmp_plugin_ptr= plugin_insert_or_reuse(&tmp)))
        {
          plugin_array_version++;
          if (!my_hash_insert(&plugin_hash[plugin->type], (uchar*)tmp_plugin_ptr))
          {
            init_alloc_root(&tmp_plugin_ptr->mem_root, 4096, 4096);
            DBUG_RETURN(FALSE);
          }
          tmp_plugin_ptr->state= PLUGIN_IS_FREED;
        }
        mysql_del_sys_var_chain(tmp.system_vars);
        goto err;
      }
      /* plugin was disabled */
      plugin_dl_del(dl);
      DBUG_RETURN(FALSE);
    }
  }
  if (report & REPORT_TO_USER)
    my_error(ER_CANT_FIND_DL_ENTRY, MYF(0), name->str);
  if (report & REPORT_TO_LOG)
    sql_print_error(ER(ER_CANT_FIND_DL_ENTRY), name->str);
err:
  plugin_dl_del(dl);
  DBUG_RETURN(TRUE);
}


static void plugin_deinitialize(struct st_plugin_int *plugin, bool ref_check)
{
  /*
    we don't want to hold the LOCK_plugin mutex as it may cause
    deinitialization to deadlock if plugins have worker threads
    with plugin locks
  */
  safe_mutex_assert_not_owner(&LOCK_plugin);

  if (plugin->plugin->status_vars)
  {
#ifdef FIX_LATER
    /*
      We have a problem right now where we can not prepend without
      breaking backwards compatibility. We will fix this shortly so
      that engines have "use names" and we wil use those for
      CREATE TABLE, and use the plugin name then for adding automatic
      variable names.
    */
    SHOW_VAR array[2]= {
      {plugin->plugin->name, (char*)plugin->plugin->status_vars, SHOW_ARRAY},
      {0, 0, SHOW_UNDEF}
    };
    remove_status_vars(array);
#else
    remove_status_vars(plugin->plugin->status_vars);
#endif /* FIX_LATER */
  }

  if (plugin_type_deinitialize[plugin->plugin->type])
  {
    if ((*plugin_type_deinitialize[plugin->plugin->type])(plugin))
    {
      sql_print_error("Plugin '%s' of type %s failed deinitialization",
                      plugin->name.str, plugin_type_names[plugin->plugin->type].str);
    }
  }
  else if (plugin->plugin->deinit)
  {
    DBUG_PRINT("info", ("Deinitializing plugin: '%s'", plugin->name.str));
    if (plugin->plugin->deinit(plugin))
    {
      DBUG_PRINT("warning", ("Plugin '%s' deinit function returned error.",
                             plugin->name.str));
    }
  }
  plugin->state= PLUGIN_IS_UNINITIALIZED;

  /*
    We do the check here because NDB has a worker THD which doesn't
    exit until NDB is shut down.
  */
  if (ref_check && plugin->ref_count)
    sql_print_error("Plugin '%s' has ref_count=%d after deinitialization.",
                    plugin->name.str, plugin->ref_count);
}


static void plugin_del(struct st_plugin_int *plugin)
{
  DBUG_ENTER("plugin_del(plugin)");
  safe_mutex_assert_owner(&LOCK_plugin);
  /* Free allocated strings before deleting the plugin. */
  plugin_vars_free_values(plugin->system_vars);
  hash_delete(&plugin_hash[plugin->plugin->type], (uchar*)plugin);
  if (plugin->plugin_dl)
    plugin_dl_del(&plugin->plugin_dl->dl);
  plugin->state= PLUGIN_IS_FREED;
  plugin_array_version++;
  rw_wrlock(&LOCK_system_variables_hash);
  mysql_del_sys_var_chain(plugin->system_vars);
  rw_unlock(&LOCK_system_variables_hash);
  free_root(&plugin->mem_root, MYF(0));
  DBUG_VOID_RETURN;
}

#ifdef NOT_USED

static void plugin_del(const LEX_STRING *name)
{
  struct st_plugin_int *plugin;
  DBUG_ENTER("plugin_del(name)");
  if ((plugin= plugin_find_internal(name, MYSQL_ANY_PLUGIN)))
    plugin_del(plugin);
  DBUG_VOID_RETURN;
}

#endif

static void reap_plugins(void)
{
  uint count, idx;
  struct st_plugin_int *plugin, **reap, **list;

  safe_mutex_assert_owner(&LOCK_plugin);

  if (!reap_needed)
    return;

  reap_needed= false;
  count= plugin_array.elements;
  reap= (struct st_plugin_int **)my_alloca(sizeof(plugin)*(count+1));
  *(reap++)= NULL;

  for (idx= 0; idx < count; idx++)
  {
    plugin= *dynamic_element(&plugin_array, idx, struct st_plugin_int **);
    if (plugin->state == PLUGIN_IS_DELETED && !plugin->ref_count)
    {
      /* change the status flag to prevent reaping by another thread */
      plugin->state= PLUGIN_IS_DYING;
      *(reap++)= plugin;
    }
  }

  pthread_mutex_unlock(&LOCK_plugin);

  list= reap;
  while ((plugin= *(--list)))
    plugin_deinitialize(plugin, true);

  pthread_mutex_lock(&LOCK_plugin);

  while ((plugin= *(--reap)))
    plugin_del(plugin);

  my_afree(reap);
}

static void intern_plugin_unlock(LEX *lex, plugin_ref plugin)
{
  int i;
  st_plugin_int *pi;
  DBUG_ENTER("intern_plugin_unlock");

  safe_mutex_assert_owner(&LOCK_plugin);

  if (!plugin)
    DBUG_VOID_RETURN;

  pi= plugin_ref_to_int(plugin);

#ifdef DBUG_OFF
  if (!pi->plugin_dl)
    DBUG_VOID_RETURN;
#else
  my_free((uchar*) plugin, MYF(MY_WME));
#endif

  DBUG_PRINT("info",("unlocking plugin, name= %s, ref_count= %d",
                     pi->name.str, pi->ref_count));
  if (lex)
  {
    /*
      Remove one instance of this plugin from the use list.
      We are searching backwards so that plugins locked last
      could be unlocked faster - optimizing for LIFO semantics.
    */
    for (i= lex->plugins.elements - 1; i >= 0; i--)
      if (plugin == *dynamic_element(&lex->plugins, i, plugin_ref*))
      {
        delete_dynamic_element(&lex->plugins, i);
        break;
      }
    DBUG_ASSERT(i >= 0);
  }

  DBUG_ASSERT(pi->ref_count);
  pi->ref_count--;

  if (pi->state == PLUGIN_IS_DELETED && !pi->ref_count)
    reap_needed= true;

  DBUG_VOID_RETURN;
}


void plugin_unlock(THD *thd, plugin_ref plugin)
{
  LEX *lex= thd ? thd->lex : 0;
  DBUG_ENTER("plugin_unlock");
  if (!plugin)
    DBUG_VOID_RETURN;
#ifdef DBUG_OFF
  /* built-in plugins don't need ref counting */
  if (!plugin_dlib(plugin))
    DBUG_VOID_RETURN;
#endif
  pthread_mutex_lock(&LOCK_plugin);
  intern_plugin_unlock(lex, plugin);
  reap_plugins();
  pthread_mutex_unlock(&LOCK_plugin);
  DBUG_VOID_RETURN;
}


void plugin_unlock_list(THD *thd, plugin_ref *list, uint count)
{
  LEX *lex= thd ? thd->lex : 0;
  DBUG_ENTER("plugin_unlock_list");
  DBUG_ASSERT(list);
  pthread_mutex_lock(&LOCK_plugin);
  while (count--)
    intern_plugin_unlock(lex, *list++);
  reap_plugins();
  pthread_mutex_unlock(&LOCK_plugin);
  DBUG_VOID_RETURN;
}


static int plugin_initialize(struct st_plugin_int *plugin)
{
  DBUG_ENTER("plugin_initialize");

  safe_mutex_assert_owner(&LOCK_plugin);

  if (plugin_type_initialize[plugin->plugin->type])
  {
    if ((*plugin_type_initialize[plugin->plugin->type])(plugin))
    {
      sql_print_error("Plugin '%s' registration as a %s failed.",
                      plugin->name.str, plugin_type_names[plugin->plugin->type].str);
      goto err;
    }
  }
  else if (plugin->plugin->init)
  {
    if (plugin->plugin->init(plugin))
    {
      sql_print_error("Plugin '%s' init function returned error.",
                      plugin->name.str);
      goto err;
    }
  }

  plugin->state= PLUGIN_IS_READY;

  if (plugin->plugin->status_vars)
  {
#ifdef FIX_LATER
    /*
      We have a problem right now where we can not prepend without
      breaking backwards compatibility. We will fix this shortly so
      that engines have "use names" and we wil use those for
      CREATE TABLE, and use the plugin name then for adding automatic
      variable names.
    */
    SHOW_VAR array[2]= {
      {plugin->plugin->name, (char*)plugin->plugin->status_vars, SHOW_ARRAY},
      {0, 0, SHOW_UNDEF}
    };
    if (add_status_vars(array)) // add_status_vars makes a copy
      goto err;
#else
    add_status_vars(plugin->plugin->status_vars); // add_status_vars makes a copy
#endif /* FIX_LATER */
  }

  /*
    set the plugin attribute of plugin's sys vars so they are pointing
    to the active plugin
  */
  if (plugin->system_vars)
  {
    sys_var_pluginvar *var= plugin->system_vars->cast_pluginvar();
    for (;;)
    {
      var->plugin= plugin;
      if (!var->next)
        break;
      var= var->next->cast_pluginvar();
    }
  }

  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}


extern "C" uchar *get_plugin_hash_key(const uchar *, size_t *, my_bool);
extern "C" uchar *get_bookmark_hash_key(const uchar *, size_t *, my_bool);


uchar *get_plugin_hash_key(const uchar *buff, size_t *length,
                           my_bool not_used __attribute__((unused)))
{
  struct st_plugin_int *plugin= (st_plugin_int *)buff;
  *length= (uint)plugin->name.length;
  return((uchar *)plugin->name.str);
}


uchar *get_bookmark_hash_key(const uchar *buff, size_t *length,
                             my_bool not_used __attribute__((unused)))
{
  struct st_bookmark *var= (st_bookmark *)buff;
  *length= var->name_len + 1;
  return (uchar*) var->key;
}


/*
  The logic is that we first load and initialize all compiled in plugins.
  From there we load up the dynamic types (assuming we have not been told to
  skip this part).

  Finally we initialize everything, aka the dynamic that have yet to initialize.
*/
int plugin_init(int *argc, char **argv, int flags)
{
  uint i;
  bool def_enabled, is_myisam;
  struct st_mysql_plugin **builtins;
  struct st_mysql_plugin *plugin;
  struct st_plugin_int tmp, *plugin_ptr, **reap;
  MEM_ROOT tmp_root;
  DBUG_ENTER("plugin_init");

  if (initialized)
    DBUG_RETURN(0);

  init_alloc_root(&plugin_mem_root, 4096, 4096);
  init_alloc_root(&tmp_root, 4096, 4096);

  if (hash_init(&bookmark_hash, &my_charset_bin, 16, 0, 0,
                  get_bookmark_hash_key, NULL, HASH_UNIQUE))
      goto err;


  pthread_mutex_init(&LOCK_plugin, MY_MUTEX_INIT_FAST);

  if (my_init_dynamic_array(&plugin_dl_array,
                            sizeof(struct st_plugin_dl *),16,16) ||
      my_init_dynamic_array(&plugin_array,
                            sizeof(struct st_plugin_int *),16,16))
    goto err;

  for (i= 0; i < MYSQL_MAX_PLUGIN_TYPE_NUM; i++)
  {
    if (hash_init(&plugin_hash[i], system_charset_info, 16, 0, 0,
                  get_plugin_hash_key, NULL, HASH_UNIQUE))
      goto err;
  }

  pthread_mutex_lock(&LOCK_plugin);

  initialized= 1;

  /*
    First we register builtin plugins
  */
  for (builtins= mysqld_builtins; *builtins; builtins++)
  {
    for (plugin= *builtins; plugin->info; plugin++)
    {
      /* by default, only ndbcluster is disabled */
      def_enabled=
        my_strcasecmp(&my_charset_latin1, plugin->name, "NDBCLUSTER") != 0;
      bzero(&tmp, sizeof(tmp));
      tmp.plugin= plugin;
      tmp.name.str= (char *)plugin->name;
      tmp.name.length= strlen(plugin->name);

      free_root(&tmp_root, MYF(MY_MARK_BLOCKS_FREE));
      if (test_plugin_options(&tmp_root, &tmp, argc, argv, def_enabled))
        continue;

      if (register_builtin(plugin, &tmp, &plugin_ptr))
        goto err_unlock;

      /* only initialize MyISAM and CSV at this stage */
      if (!(is_myisam=
            !my_strcasecmp(&my_charset_latin1, plugin->name, "MyISAM")) &&
          my_strcasecmp(&my_charset_latin1, plugin->name, "CSV"))
        continue;

      if (plugin_initialize(plugin_ptr))
        goto err_unlock;

      /*
        initialize the global default storage engine so that it may
        not be null in any child thread.
      */
      if (is_myisam)
      {
        DBUG_ASSERT(!global_system_variables.table_plugin);
        global_system_variables.table_plugin=
          my_intern_plugin_lock(NULL, plugin_int_to_ref(plugin_ptr));
        DBUG_ASSERT(plugin_ptr->ref_count == 1);
      }
    }
  }

  /* should now be set to MyISAM storage engine */
  DBUG_ASSERT(global_system_variables.table_plugin);

  pthread_mutex_unlock(&LOCK_plugin);

  /* Register all dynamic plugins */
  if (!(flags & PLUGIN_INIT_SKIP_DYNAMIC_LOADING))
  {
    if (opt_plugin_load &&
        plugin_load_list(&tmp_root, argc, argv, opt_plugin_load))
      goto err;
    if (!(flags & PLUGIN_INIT_SKIP_PLUGIN_TABLE))
      plugin_load(&tmp_root, argc, argv);
  }

  if (flags & PLUGIN_INIT_SKIP_INITIALIZATION)
    goto end;

  /*
    Now we initialize all remaining plugins
  */

  pthread_mutex_lock(&LOCK_plugin);
  reap= (st_plugin_int **) my_alloca((plugin_array.elements+1) * sizeof(void*));
  *(reap++)= NULL;

  for (i= 0; i < plugin_array.elements; i++)
  {
    plugin_ptr= *dynamic_element(&plugin_array, i, struct st_plugin_int **);
    if (plugin_ptr->state == PLUGIN_IS_UNINITIALIZED)
    {
      if (plugin_initialize(plugin_ptr))
      {
        plugin_ptr->state= PLUGIN_IS_DYING;
        *(reap++)= plugin_ptr;
      }
    }
  }

  /*
    Check if any plugins have to be reaped
  */
  while ((plugin_ptr= *(--reap)))
  {
    pthread_mutex_unlock(&LOCK_plugin);
    plugin_deinitialize(plugin_ptr, true);
    pthread_mutex_lock(&LOCK_plugin);
    plugin_del(plugin_ptr);
  }

  pthread_mutex_unlock(&LOCK_plugin);
  my_afree(reap);

end:
  free_root(&tmp_root, MYF(0));

  DBUG_RETURN(0);

err_unlock:
  pthread_mutex_unlock(&LOCK_plugin);
err:
  free_root(&tmp_root, MYF(0));
  DBUG_RETURN(1);
}


static bool register_builtin(struct st_mysql_plugin *plugin,
                             struct st_plugin_int *tmp,
                             struct st_plugin_int **ptr)
{
  DBUG_ENTER("register_builtin");

  tmp->state= PLUGIN_IS_UNINITIALIZED;
  tmp->ref_count= 0;
  tmp->plugin_dl= 0;

  if (insert_dynamic(&plugin_array, (uchar*)&tmp))
    DBUG_RETURN(1);

  *ptr= *dynamic_element(&plugin_array, plugin_array.elements - 1,
                         struct st_plugin_int **)=
        (struct st_plugin_int *) memdup_root(&plugin_mem_root, (uchar*)tmp,
                                             sizeof(struct st_plugin_int));

  if (my_hash_insert(&plugin_hash[plugin->type],(uchar*) *ptr))
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}


#ifdef NOT_USED_YET
/*
  Register a plugin at run time. (note, this doesn't initialize a plugin)
  Will be useful for embedded applications.

  SYNOPSIS
    plugin_register_builtin()
    thd         current thread (used to store scratch data in mem_root)
    plugin      static plugin to install

  RETURN
    false - plugin registered successfully
*/
bool plugin_register_builtin(THD *thd, struct st_mysql_plugin *plugin)
{
  struct st_plugin_int tmp, *ptr;
  bool result= true;
  int dummy_argc= 0;
  DBUG_ENTER("plugin_register_builtin");

  bzero(&tmp, sizeof(tmp));
  tmp.plugin= plugin;
  tmp.name.str= (char *)plugin->name;
  tmp.name.length= strlen(plugin->name);

  pthread_mutex_lock(&LOCK_plugin);
  rw_wrlock(&LOCK_system_variables_hash);

  if (test_plugin_options(thd->mem_root, &tmp, &dummy_argc, NULL, true))
    goto end;

  if ((result= register_builtin(plugin, &tmp, &ptr)))
    mysql_del_sys_var_chain(tmp.system_vars);

end:
  rw_unlock(&LOCK_system_variables_hash);
  pthread_mutex_unlock(&LOCK_plugin);

  DBUG_RETURN(result);;
}
#endif /* NOT_USED_YET */


/*
  called only by plugin_init()
*/
static void plugin_load(MEM_ROOT *tmp_root, int *argc, char **argv)
{
  TABLE_LIST tables;
  TABLE *table;
  READ_RECORD read_record_info;
  int error;
  THD *new_thd;
#ifdef EMBEDDED_LIBRARY
  bool table_exists;
#endif /* EMBEDDED_LIBRARY */
  DBUG_ENTER("plugin_load");

  if (!(new_thd= new THD))
  {
    sql_print_error("Can't allocate memory for plugin structures");
    delete new_thd;
    DBUG_VOID_RETURN;
  }
  new_thd->thread_stack= (char*) &tables;
  new_thd->store_globals();
  lex_start(new_thd);
  new_thd->db= my_strdup("mysql", MYF(0));
  new_thd->db_length= 5;
  bzero((uchar*)&tables, sizeof(tables));
  tables.alias= tables.table_name= (char*)"plugin";
  tables.lock_type= TL_READ;
  tables.db= new_thd->db;

#ifdef EMBEDDED_LIBRARY
  /*
    When building an embedded library, if the mysql.plugin table
    does not exist, we silently ignore the missing table
  */
  pthread_mutex_lock(&LOCK_open);
  if (check_if_table_exists(new_thd, &tables, &table_exists))
    table_exists= FALSE;
  pthread_mutex_unlock(&LOCK_open);
  if (!table_exists)
    goto end;
#endif /* EMBEDDED_LIBRARY */

  if (simple_open_n_lock_tables(new_thd, &tables))
  {
    DBUG_PRINT("error",("Can't open plugin table"));
    sql_print_error("Can't open the mysql.plugin table. Please "
                    "run mysql_upgrade to create it.");
    goto end;
  }
  table= tables.table;
  init_read_record(&read_record_info, new_thd, table, NULL, 1, 0);
  table->use_all_columns();
  /*
    there're no other threads running yet, so we don't need a mutex.
    but plugin_add() before is designed to work in multi-threaded
    environment, and it uses safe_mutex_assert_owner(), so we lock
    the mutex here to satisfy the assert
  */
  pthread_mutex_lock(&LOCK_plugin);
  while (!(error= read_record_info.read_record(&read_record_info)))
  {
    DBUG_PRINT("info", ("init plugin record"));
    String str_name, str_dl;
    get_field(tmp_root, table->field[0], &str_name);
    get_field(tmp_root, table->field[1], &str_dl);

    LEX_STRING name= {(char *)str_name.ptr(), str_name.length()};
    LEX_STRING dl= {(char *)str_dl.ptr(), str_dl.length()};

    if (plugin_add(tmp_root, &name, &dl, argc, argv, REPORT_TO_LOG))
      sql_print_warning("Couldn't load plugin named '%s' with soname '%s'.",
                        str_name.c_ptr(), str_dl.c_ptr());
    free_root(tmp_root, MYF(MY_MARK_BLOCKS_FREE));
  }
  pthread_mutex_unlock(&LOCK_plugin);
  if (error > 0)
    sql_print_error(ER(ER_GET_ERRNO), my_errno);
  end_read_record(&read_record_info);
  new_thd->version--; // Force close to free memory
end:
  close_thread_tables(new_thd);
  delete new_thd;
  /* Remember that we don't have a THD */
  my_pthread_setspecific_ptr(THR_THD, 0);
  DBUG_VOID_RETURN;
}


/*
  called only by plugin_init()
*/
static bool plugin_load_list(MEM_ROOT *tmp_root, int *argc, char **argv,
                             const char *list)
{
  char buffer[FN_REFLEN];
  LEX_STRING name= {buffer, 0}, dl= {NULL, 0}, *str= &name;
  struct st_plugin_dl *plugin_dl;
  struct st_mysql_plugin *plugin;
  char *p= buffer;
  DBUG_ENTER("plugin_load_list");
  while (list)
  {
    if (p == buffer + sizeof(buffer) - 1)
      break;
    switch ((*(p++)= *(list++))) {
    case '\0':
      list= NULL; /* terminate the loop */
      /* fall through */
#ifndef __WIN__
    case ':':     /* can't use this as delimiter as it may be drive letter */
#endif
    case ';':
      name.str[name.length]= '\0';
      if (str != &dl)  // load all plugins in named module
      {
        dl= name;
        if ((plugin_dl= plugin_dl_add(&dl, REPORT_TO_LOG)))
        {
          for (plugin= plugin_dl->plugins; plugin->info; plugin++)
          {
            name.str= (char *) plugin->name;
            name.length= strlen(name.str);

            free_root(tmp_root, MYF(MY_MARK_BLOCKS_FREE));
            if (plugin_add(tmp_root, &name, &dl, argc, argv, REPORT_TO_LOG))
              goto error;
          }
          plugin_dl_del(&dl); // reduce ref count
        }
      }
      else
      {
        free_root(tmp_root, MYF(MY_MARK_BLOCKS_FREE));
        if (plugin_add(tmp_root, &name, &dl, argc, argv, REPORT_TO_LOG))
          goto error;
      }
      name.length= dl.length= 0;
      dl.str= NULL; name.str= p= buffer;
      str= &name;
      continue;
    case '=':
    case '#':
      if (str == &name)
      {
        str= &dl;
        str->str= p;
        continue;
      }
    default:
      str->length++;
      continue;
    }
  }
  DBUG_RETURN(FALSE);
error:
  sql_print_error("Couldn't load plugin named '%s' with soname '%s'.",
                  name.str, dl.str);
  DBUG_RETURN(TRUE);
}


void plugin_shutdown(void)
{
  uint i, count= plugin_array.elements, free_slots= 0;
  struct st_plugin_int **plugins, *plugin;
  struct st_plugin_dl **dl;
  DBUG_ENTER("plugin_shutdown");

  if (initialized)
  {
    pthread_mutex_lock(&LOCK_plugin);

    reap_needed= true;

    /*
      We want to shut down plugins in a reasonable order, this will
      become important when we have plugins which depend upon each other.
      Circular references cannot be reaped so they are forced afterwards.
      TODO: Have an additional step here to notify all active plugins that
      shutdown is requested to allow plugins to deinitialize in parallel.
    */
    while (reap_needed && (count= plugin_array.elements))
    {
      reap_plugins();
      for (i= free_slots= 0; i < count; i++)
      {
        plugin= *dynamic_element(&plugin_array, i, struct st_plugin_int **);
        switch (plugin->state) {
        case PLUGIN_IS_READY:
          plugin->state= PLUGIN_IS_DELETED;
          reap_needed= true;
          break;
        case PLUGIN_IS_FREED:
        case PLUGIN_IS_UNINITIALIZED:
          free_slots++;
          break;
        }
      }
      if (!reap_needed)
      {
        /*
          release any plugin references held.
        */
        unlock_variables(NULL, &global_system_variables);
        unlock_variables(NULL, &max_system_variables);
      }
    }

    if (count > free_slots)
      sql_print_warning("Forcing shutdown of %d plugins", count - free_slots);

    plugins= (struct st_plugin_int **) my_alloca(sizeof(void*) * (count+1));

    /*
      If we have any plugins which did not die cleanly, we force shutdown
    */
    for (i= 0; i < count; i++)
    {
      plugins[i]= *dynamic_element(&plugin_array, i, struct st_plugin_int **);
      /* change the state to ensure no reaping races */
      if (plugins[i]->state == PLUGIN_IS_DELETED)
        plugins[i]->state= PLUGIN_IS_DYING;
    }
    pthread_mutex_unlock(&LOCK_plugin);

    /*
      We loop through all plugins and call deinit() if they have one.
    */
    for (i= 0; i < count; i++)
      if (!(plugins[i]->state & (PLUGIN_IS_UNINITIALIZED | PLUGIN_IS_FREED)))
      {
        sql_print_information("Plugin '%s' will be forced to shutdown",
                              plugins[i]->name.str);
        /*
          We are forcing deinit on plugins so we don't want to do a ref_count
          check until we have processed all the plugins.
        */
        plugin_deinitialize(plugins[i], false);
      }

    /*
      It's perfectly safe not to lock LOCK_plugin, as there're no
      concurrent threads anymore. But some functions called from here
      use safe_mutex_assert_owner(), so we lock the mutex to satisfy it
    */
    pthread_mutex_lock(&LOCK_plugin);

    /*
      We defer checking ref_counts until after all plugins are deinitialized
      as some may have worker threads holding on to plugin references.
    */
    for (i= 0; i < count; i++)
    {
      if (plugins[i]->ref_count)
        sql_print_error("Plugin '%s' has ref_count=%d after shutdown.",
                        plugins[i]->name.str, plugins[i]->ref_count);
      if (plugins[i]->state & PLUGIN_IS_UNINITIALIZED)
        plugin_del(plugins[i]);
    }

    /*
      Now we can deallocate all memory.
    */

    cleanup_variables(NULL, &global_system_variables);
    cleanup_variables(NULL, &max_system_variables);
    pthread_mutex_unlock(&LOCK_plugin);

    initialized= 0;
    pthread_mutex_destroy(&LOCK_plugin);

    my_afree(plugins);
  }

  /* Dispose of the memory */

  for (i= 0; i < MYSQL_MAX_PLUGIN_TYPE_NUM; i++)
    hash_free(&plugin_hash[i]);
  delete_dynamic(&plugin_array);

  count= plugin_dl_array.elements;
  dl= (struct st_plugin_dl **)my_alloca(sizeof(void*) * count);
  for (i= 0; i < count; i++)
    dl[i]= *dynamic_element(&plugin_dl_array, i, struct st_plugin_dl **);
  for (i= 0; i < plugin_dl_array.elements; i++)
    free_plugin_mem(dl[i]);
  my_afree(dl);
  delete_dynamic(&plugin_dl_array);

  hash_free(&bookmark_hash);
  free_root(&plugin_mem_root, MYF(0));

  global_variables_dynamic_size= 0;

  DBUG_VOID_RETURN;
}


bool mysql_install_plugin(THD *thd, const LEX_STRING *name, const LEX_STRING *dl)
{
  TABLE_LIST tables;
  TABLE *table;
  int error, argc;
  char *argv[2];
  struct st_plugin_int *tmp;
  DBUG_ENTER("mysql_install_plugin");

  bzero(&tables, sizeof(tables));
  tables.db= (char *)"mysql";
  tables.table_name= tables.alias= (char *)"plugin";
  if (check_table_access(thd, INSERT_ACL, &tables, 0))
    DBUG_RETURN(TRUE);

  /* need to open before acquiring LOCK_plugin or it will deadlock */
  if (! (table = open_ltable(thd, &tables, TL_WRITE, 0)))
    DBUG_RETURN(TRUE);

  pthread_mutex_lock(&LOCK_plugin);
  rw_wrlock(&LOCK_system_variables_hash);
  /* handle_options() assumes arg0 (program name) always exists */
  argv[0]= const_cast<char*>(""); // without a cast gcc emits a warning
  argv[1]= 0;
  argc= 1;
  error= plugin_add(thd->mem_root, name, dl, &argc, argv, REPORT_TO_USER);
  rw_unlock(&LOCK_system_variables_hash);

  if (error || !(tmp= plugin_find_internal(name, MYSQL_ANY_PLUGIN)))
    goto err;

  if (plugin_initialize(tmp))
  {
    my_error(ER_CANT_INITIALIZE_UDF, MYF(0), name->str,
             "Plugin initialization function failed.");
    goto deinit;
  }

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

  pthread_mutex_unlock(&LOCK_plugin);
  DBUG_RETURN(FALSE);
deinit:
  tmp->state= PLUGIN_IS_DELETED;
  reap_needed= true;
  reap_plugins();
err:
  pthread_mutex_unlock(&LOCK_plugin);
  DBUG_RETURN(TRUE);
}


bool mysql_uninstall_plugin(THD *thd, const LEX_STRING *name)
{
  TABLE *table;
  TABLE_LIST tables;
  struct st_plugin_int *plugin;
  DBUG_ENTER("mysql_uninstall_plugin");

  bzero(&tables, sizeof(tables));
  tables.db= (char *)"mysql";
  tables.table_name= tables.alias= (char *)"plugin";

  /* need to open before acquiring LOCK_plugin or it will deadlock */
  if (! (table= open_ltable(thd, &tables, TL_WRITE, 0)))
    DBUG_RETURN(TRUE);

  pthread_mutex_lock(&LOCK_plugin);
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

  plugin->state= PLUGIN_IS_DELETED;
  if (plugin->ref_count)
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, 0,
                 "Plugin is busy and will be uninstalled on shutdown");
  else
    reap_needed= true;
  reap_plugins();
  pthread_mutex_unlock(&LOCK_plugin);

  table->use_all_columns();
  table->field[0]->store(name->str, name->length, system_charset_info);
  if (! table->file->index_read_idx_map(table->record[0], 0,
                                        (uchar *)table->field[0]->ptr,
                                        HA_WHOLE_KEY,
                                        HA_READ_KEY_EXACT))
  {
    int error;
    if ((error= table->file->ha_delete_row(table->record[0])))
    {
      table->file->print_error(error, MYF(0));
      DBUG_RETURN(TRUE);
    }
  }
  DBUG_RETURN(FALSE);
err:
  pthread_mutex_unlock(&LOCK_plugin);
  DBUG_RETURN(TRUE);
}


bool plugin_foreach_with_mask(THD *thd, plugin_foreach_func *func,
                       int type, uint state_mask, void *arg)
{
  uint idx, total;
  struct st_plugin_int *plugin, **plugins;
  int version=plugin_array_version;
  DBUG_ENTER("plugin_foreach_with_mask");

  if (!initialized)
    DBUG_RETURN(FALSE);

  state_mask= ~state_mask; // do it only once

  pthread_mutex_lock(&LOCK_plugin);
  total= type == MYSQL_ANY_PLUGIN ? plugin_array.elements
                                  : plugin_hash[type].records;
  /*
    Do the alloca out here in case we do have a working alloca:
        leaving the nested stack frame invalidates alloca allocation.
  */
  plugins=(struct st_plugin_int **)my_alloca(total*sizeof(plugin));
  if (type == MYSQL_ANY_PLUGIN)
  {
    for (idx= 0; idx < total; idx++)
    {
      plugin= *dynamic_element(&plugin_array, idx, struct st_plugin_int **);
      plugins[idx]= !(plugin->state & state_mask) ? plugin : NULL;
    }
  }
  else
  {
    HASH *hash= plugin_hash + type;
    for (idx= 0; idx < total; idx++)
    {
      plugin= (struct st_plugin_int *) hash_element(hash, idx);
      plugins[idx]= !(plugin->state & state_mask) ? plugin : NULL;
    }
  }
  pthread_mutex_unlock(&LOCK_plugin);

  for (idx= 0; idx < total; idx++)
  {
    if (unlikely(version != plugin_array_version))
    {
      pthread_mutex_lock(&LOCK_plugin);
      for (uint i=idx; i < total; i++)
        if (plugins[i] && plugins[i]->state & state_mask)
          plugins[i]=0;
      pthread_mutex_unlock(&LOCK_plugin);
    }
    plugin= plugins[idx];
    /* It will stop iterating on first engine error when "func" returns TRUE */
    if (plugin && func(thd, plugin_int_to_ref(plugin), arg))
        goto err;
  }

  my_afree(plugins);
  DBUG_RETURN(FALSE);
err:
  my_afree(plugins);
  DBUG_RETURN(TRUE);
}


/****************************************************************************
  Internal type declarations for variables support
****************************************************************************/

#undef MYSQL_SYSVAR_NAME
#define MYSQL_SYSVAR_NAME(name) name
#define PLUGIN_VAR_TYPEMASK 0x007f

#define EXTRA_OPTIONS 3 /* options for: 'foo', 'plugin-foo' and NULL */

typedef DECLARE_MYSQL_SYSVAR_BASIC(sysvar_bool_t, my_bool);
typedef DECLARE_MYSQL_THDVAR_BASIC(thdvar_bool_t, my_bool);
typedef DECLARE_MYSQL_SYSVAR_BASIC(sysvar_str_t, char *);
typedef DECLARE_MYSQL_THDVAR_BASIC(thdvar_str_t, char *);

typedef DECLARE_MYSQL_SYSVAR_TYPELIB(sysvar_enum_t, unsigned long);
typedef DECLARE_MYSQL_THDVAR_TYPELIB(thdvar_enum_t, unsigned long);
typedef DECLARE_MYSQL_SYSVAR_TYPELIB(sysvar_set_t, ulonglong);
typedef DECLARE_MYSQL_THDVAR_TYPELIB(thdvar_set_t, ulonglong);

typedef DECLARE_MYSQL_SYSVAR_SIMPLE(sysvar_int_t, int);
typedef DECLARE_MYSQL_SYSVAR_SIMPLE(sysvar_long_t, long);
typedef DECLARE_MYSQL_SYSVAR_SIMPLE(sysvar_longlong_t, longlong);
typedef DECLARE_MYSQL_SYSVAR_SIMPLE(sysvar_uint_t, uint);
typedef DECLARE_MYSQL_SYSVAR_SIMPLE(sysvar_ulong_t, ulong);
typedef DECLARE_MYSQL_SYSVAR_SIMPLE(sysvar_ulonglong_t, ulonglong);

typedef DECLARE_MYSQL_THDVAR_SIMPLE(thdvar_int_t, int);
typedef DECLARE_MYSQL_THDVAR_SIMPLE(thdvar_long_t, long);
typedef DECLARE_MYSQL_THDVAR_SIMPLE(thdvar_longlong_t, longlong);
typedef DECLARE_MYSQL_THDVAR_SIMPLE(thdvar_uint_t, uint);
typedef DECLARE_MYSQL_THDVAR_SIMPLE(thdvar_ulong_t, ulong);
typedef DECLARE_MYSQL_THDVAR_SIMPLE(thdvar_ulonglong_t, ulonglong);

#define SET_PLUGIN_VAR_RESOLVE(opt)\
  *(mysql_sys_var_ptr_p*)&((opt)->resolve)= mysql_sys_var_ptr
typedef uchar *(*mysql_sys_var_ptr_p)(void* a_thd, int offset);


/****************************************************************************
  default variable data check and update functions
****************************************************************************/

static int check_func_bool(THD *thd, struct st_mysql_sys_var *var,
                           void *save, st_mysql_value *value)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *strvalue= "NULL", *str;
  int result, length;
  long long tmp;

  if (value->value_type(value) == MYSQL_VALUE_TYPE_STRING)
  {
    length= sizeof(buff);
    if (!(str= value->val_str(value, buff, &length)) ||
        (result= find_type(&bool_typelib, str, length, 1)-1) < 0)
    {
      if (str)
        strvalue= str;
      goto err;
    }
  }
  else
  {
    if (value->val_int(value, &tmp) < 0)
      goto err;
    if (tmp > 1)
    {
      llstr(tmp, buff);
      strvalue= buff;
      goto err;
    }
    result= (int) tmp;
  }
  *(int*)save= -result;
  return 0;
err:
  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), var->name, strvalue);
  return 1;
}


static int check_func_int(THD *thd, struct st_mysql_sys_var *var,
                          void *save, st_mysql_value *value)
{
  long long tmp;
  struct my_option options;
  value->val_int(value, &tmp);
  plugin_opt_set_limits(&options, var);
  *(int *)save= (int) getopt_ull_limit_value(tmp, &options);
  return (thd->variables.sql_mode & MODE_STRICT_ALL_TABLES) &&
         (*(int *)save != (int) tmp);
}


static int check_func_long(THD *thd, struct st_mysql_sys_var *var,
                          void *save, st_mysql_value *value)
{
  long long tmp;
  struct my_option options;
  value->val_int(value, &tmp);
  plugin_opt_set_limits(&options, var);
  *(long *)save= (long) getopt_ull_limit_value(tmp, &options);
  return (thd->variables.sql_mode & MODE_STRICT_ALL_TABLES) &&
         (*(long *)save != (long) tmp);
}


static int check_func_longlong(THD *thd, struct st_mysql_sys_var *var,
                          void *save, st_mysql_value *value)
{
  long long tmp;
  struct my_option options;
  value->val_int(value, &tmp);
  plugin_opt_set_limits(&options, var);
  *(ulonglong *)save= getopt_ull_limit_value(tmp, &options);
  return (thd->variables.sql_mode & MODE_STRICT_ALL_TABLES) &&
         (*(long long *)save != tmp);
}

static int check_func_str(THD *thd, struct st_mysql_sys_var *var,
                          void *save, st_mysql_value *value)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *str;
  int length;

  length= sizeof(buff);
  if ((str= value->val_str(value, buff, &length)))
    str= thd->strmake(str, length);
  *(const char**)save= str;
  return 0;
}


static int check_func_enum(THD *thd, struct st_mysql_sys_var *var,
                           void *save, st_mysql_value *value)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *strvalue= "NULL", *str;
  TYPELIB *typelib;
  long long tmp;
  long result;
  int length;

  if (var->flags & PLUGIN_VAR_THDLOCAL)
    typelib= ((thdvar_enum_t*) var)->typelib;
  else
    typelib= ((sysvar_enum_t*) var)->typelib;

  if (value->value_type(value) == MYSQL_VALUE_TYPE_STRING)
  {
    length= sizeof(buff);
    if (!(str= value->val_str(value, buff, &length)))
      goto err;
    if ((result= find_type(typelib, str, length, 1)-1) < 0)
    {
      strvalue= str;
      goto err;
    }
  }
  else
  {
    if (value->val_int(value, &tmp))
      goto err;
    if (tmp >= typelib->count)
    {
      llstr(tmp, buff);
      strvalue= buff;
      goto err;
    }
    result= (long) tmp;
  }
  *(long*)save= result;
  return 0;
err:
  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), var->name, strvalue);
  return 1;
}


static int check_func_set(THD *thd, struct st_mysql_sys_var *var,
                          void *save, st_mysql_value *value)
{
  char buff[STRING_BUFFER_USUAL_SIZE], *error= 0;
  const char *strvalue= "NULL", *str;
  TYPELIB *typelib;
  ulonglong result;
  uint error_len;
  bool not_used;
  int length;

  if (var->flags & PLUGIN_VAR_THDLOCAL)
    typelib= ((thdvar_set_t*) var)->typelib;
  else
    typelib= ((sysvar_set_t*)var)->typelib;

  if (value->value_type(value) == MYSQL_VALUE_TYPE_STRING)
  {
    length= sizeof(buff);
    if (!(str= value->val_str(value, buff, &length)))
      goto err;
    result= find_set(typelib, str, length, NULL,
                     &error, &error_len, &not_used);
    if (error_len)
    {
      strmake(buff, error, min(sizeof(buff), error_len));
      strvalue= buff;
      goto err;
    }
  }
  else
  {
    if (value->val_int(value, (long long *)&result))
      goto err;
    if (unlikely((result >= (ULL(1) << typelib->count)) &&
                 (typelib->count < sizeof(long)*8)))
    {
      llstr(result, buff);
      strvalue= buff;
      goto err;
    }
  }
  *(ulonglong*)save= result;
  return 0;
err:
  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), var->name, strvalue);
  return 1;
}


static void update_func_bool(THD *thd, struct st_mysql_sys_var *var,
                             void *tgt, void *save)
{
  *(my_bool *) tgt= *(int *) save ? 1 : 0;
}


static void update_func_int(THD *thd, struct st_mysql_sys_var *var,
                             void *tgt, void *save)
{
  *(int *)tgt= *(int *) save;
}


static void update_func_long(THD *thd, struct st_mysql_sys_var *var,
                             void *tgt, void *save)
{
  *(long *)tgt= *(long *) save;
}


static void update_func_longlong(THD *thd, struct st_mysql_sys_var *var,
                             void *tgt, void *save)
{
  *(longlong *)tgt= *(ulonglong *) save;
}


static void update_func_str(THD *thd, struct st_mysql_sys_var *var,
                             void *tgt, void *save)
{
  char *old= *(char **) tgt;
  *(char **)tgt= *(char **) save;
  if (var->flags & PLUGIN_VAR_MEMALLOC)
  {
    *(char **)tgt= my_strdup(*(char **) save, MYF(0));
    my_free(old, MYF(0));
  }
}


/****************************************************************************
  System Variables support
****************************************************************************/


sys_var *find_sys_var(THD *thd, const char *str, uint length)
{
  sys_var *var;
  sys_var_pluginvar *pi= NULL;
  plugin_ref plugin;
  DBUG_ENTER("find_sys_var");

  pthread_mutex_lock(&LOCK_plugin);
  rw_rdlock(&LOCK_system_variables_hash);
  if ((var= intern_find_sys_var(str, length, false)) &&
      (pi= var->cast_pluginvar()))
  {
    rw_unlock(&LOCK_system_variables_hash);
    LEX *lex= thd ? thd->lex : 0;
    if (!(plugin= my_intern_plugin_lock(lex, plugin_int_to_ref(pi->plugin))))
      var= NULL; /* failed to lock it, it must be uninstalling */
    else
    if (!(plugin_state(plugin) & PLUGIN_IS_READY))
    {
      /* initialization not completed */
      var= NULL;
      intern_plugin_unlock(lex, plugin);
    }
  }
  else
    rw_unlock(&LOCK_system_variables_hash);
  pthread_mutex_unlock(&LOCK_plugin);

  /*
    If the variable exists but the plugin it is associated with is not ready
    then the intern_plugin_lock did not raise an error, so we do it here.
  */
  if (pi && !var)
    my_error(ER_UNKNOWN_SYSTEM_VARIABLE, MYF(0), (char*) str);
  DBUG_RETURN(var);
}


/*
  called by register_var, construct_options and test_plugin_options.
  Returns the 'bookmark' for the named variable.
  LOCK_system_variables_hash should be at least read locked
*/
static st_bookmark *find_bookmark(const char *plugin, const char *name,
                                  int flags)
{
  st_bookmark *result= NULL;
  uint namelen, length, pluginlen= 0;
  char *varname, *p;

  if (!(flags & PLUGIN_VAR_THDLOCAL))
    return NULL;

  namelen= strlen(name);
  if (plugin)
    pluginlen= strlen(plugin) + 1;
  length= namelen + pluginlen + 2;
  varname= (char*) my_alloca(length);

  if (plugin)
  {
    strxmov(varname + 1, plugin, "_", name, NullS);
    for (p= varname + 1; *p; p++)
      if (*p == '-')
        *p= '_';
  }
  else
    memcpy(varname + 1, name, namelen + 1);

  varname[0]= flags & PLUGIN_VAR_TYPEMASK;

  result= (st_bookmark*) hash_search(&bookmark_hash,
                                     (const uchar*) varname, length - 1);

  my_afree(varname);
  return result;
}


/*
  returns a bookmark for thd-local variables, creating if neccessary.
  returns null for non thd-local variables.
  Requires that a write lock is obtained on LOCK_system_variables_hash
*/
static st_bookmark *register_var(const char *plugin, const char *name,
                                 int flags)
{
  uint length= strlen(plugin) + strlen(name) + 3, size= 0, offset, new_size;
  st_bookmark *result;
  char *varname, *p;

  if (!(flags & PLUGIN_VAR_THDLOCAL))
    return NULL;

  switch (flags & PLUGIN_VAR_TYPEMASK) {
  case PLUGIN_VAR_BOOL:
    size= sizeof(my_bool);
    break;
  case PLUGIN_VAR_INT:
    size= sizeof(int);
    break;
  case PLUGIN_VAR_LONG:
    size= sizeof(long);
    break;
  case PLUGIN_VAR_LONGLONG:
    size= sizeof(ulonglong);
    break;
  case PLUGIN_VAR_STR:
    size= sizeof(char*);
    break;
  default:
    DBUG_ASSERT(0);
    return NULL;
  };

  varname= ((char*) my_alloca(length));
  strxmov(varname + 1, plugin, "_", name, NullS);
  for (p= varname + 1; *p; p++)
    if (*p == '-')
      *p= '_';

  if (!(result= find_bookmark(NULL, varname + 1, flags)))
  {
    result= (st_bookmark*) alloc_root(&plugin_mem_root,
                                      sizeof(struct st_bookmark) + length-1);
    varname[0]= flags & PLUGIN_VAR_TYPEMASK;
    memcpy(result->key, varname, length);
    result->name_len= length - 2;
    result->offset= -1;

    DBUG_ASSERT(size && !(size & (size-1))); /* must be power of 2 */

    offset= global_system_variables.dynamic_variables_size;
    offset= (offset + size - 1) & ~(size - 1);
    result->offset= (int) offset;

    new_size= (offset + size + 63) & ~63;

    if (new_size > global_variables_dynamic_size)
    {
      global_system_variables.dynamic_variables_ptr= (char*)
        my_realloc(global_system_variables.dynamic_variables_ptr, new_size,
                   MYF(MY_WME | MY_FAE | MY_ALLOW_ZERO_PTR));
      max_system_variables.dynamic_variables_ptr= (char*)
        my_realloc(max_system_variables.dynamic_variables_ptr, new_size,
                   MYF(MY_WME | MY_FAE | MY_ALLOW_ZERO_PTR));
      /*
        Clear the new variable value space. This is required for string
        variables. If their value is non-NULL, it must point to a valid
        string.
      */
      bzero(global_system_variables.dynamic_variables_ptr +
            global_variables_dynamic_size,
            new_size - global_variables_dynamic_size);
      bzero(max_system_variables.dynamic_variables_ptr +
            global_variables_dynamic_size,
            new_size - global_variables_dynamic_size);
      global_variables_dynamic_size= new_size;
    }

    global_system_variables.dynamic_variables_head= offset;
    max_system_variables.dynamic_variables_head= offset;
    global_system_variables.dynamic_variables_size= offset + size;
    max_system_variables.dynamic_variables_size= offset + size;
    global_system_variables.dynamic_variables_version++;
    max_system_variables.dynamic_variables_version++;

    result->version= global_system_variables.dynamic_variables_version;

    /* this should succeed because we have already checked if a dup exists */
    if (my_hash_insert(&bookmark_hash, (uchar*) result))
    {
      fprintf(stderr, "failed to add placeholder to hash");
      DBUG_ASSERT(0);
    }
  }
  my_afree(varname);
  return result;
}


/*
  returns a pointer to the memory which holds the thd-local variable or
  a pointer to the global variable if thd==null.
  If required, will sync with global variables if the requested variable
  has not yet been allocated in the current thread.
*/
static uchar *intern_sys_var_ptr(THD* thd, int offset, bool global_lock)
{
  DBUG_ASSERT(offset >= 0);
  DBUG_ASSERT((uint)offset <= global_system_variables.dynamic_variables_head);

  if (!thd)
    return (uchar*) global_system_variables.dynamic_variables_ptr + offset;

  /*
    dynamic_variables_head points to the largest valid offset
  */
  if (!thd->variables.dynamic_variables_ptr ||
      (uint)offset > thd->variables.dynamic_variables_head)
  {
    uint idx;

    rw_rdlock(&LOCK_system_variables_hash);

    thd->variables.dynamic_variables_ptr= (char*)
      my_realloc(thd->variables.dynamic_variables_ptr,
                 global_variables_dynamic_size,
                 MYF(MY_WME | MY_FAE | MY_ALLOW_ZERO_PTR));

    if (global_lock)
      pthread_mutex_lock(&LOCK_global_system_variables);

    safe_mutex_assert_owner(&LOCK_global_system_variables);

    memcpy(thd->variables.dynamic_variables_ptr +
             thd->variables.dynamic_variables_size,
           global_system_variables.dynamic_variables_ptr +
             thd->variables.dynamic_variables_size,
           global_system_variables.dynamic_variables_size -
             thd->variables.dynamic_variables_size);

    /*
      now we need to iterate through any newly copied 'defaults'
      and if it is a string type with MEMALLOC flag, we need to strdup
    */
    for (idx= 0; idx < bookmark_hash.records; idx++)
    {
      sys_var_pluginvar *pi;
      sys_var *var;
      st_bookmark *v= (st_bookmark*) hash_element(&bookmark_hash,idx);

      if (v->version <= thd->variables.dynamic_variables_version ||
          !(var= intern_find_sys_var(v->key + 1, v->name_len, true)) ||
          !(pi= var->cast_pluginvar()) ||
          v->key[0] != (pi->plugin_var->flags & PLUGIN_VAR_TYPEMASK))
        continue;

      /* Here we do anything special that may be required of the data types */

      if ((pi->plugin_var->flags & PLUGIN_VAR_TYPEMASK) == PLUGIN_VAR_STR &&
          pi->plugin_var->flags & PLUGIN_VAR_MEMALLOC)
      {
         char **pp= (char**) (thd->variables.dynamic_variables_ptr +
                             *(int*)(pi->plugin_var + 1));
         if ((*pp= *(char**) (global_system_variables.dynamic_variables_ptr +
                             *(int*)(pi->plugin_var + 1))))
           *pp= my_strdup(*pp, MYF(MY_WME|MY_FAE));
      }
    }

    if (global_lock)
      pthread_mutex_unlock(&LOCK_global_system_variables);

    thd->variables.dynamic_variables_version=
           global_system_variables.dynamic_variables_version;
    thd->variables.dynamic_variables_head=
           global_system_variables.dynamic_variables_head;
    thd->variables.dynamic_variables_size=
           global_system_variables.dynamic_variables_size;

    rw_unlock(&LOCK_system_variables_hash);
  }
  return (uchar*)thd->variables.dynamic_variables_ptr + offset;
}

static uchar *mysql_sys_var_ptr(void* a_thd, int offset)
{
  return intern_sys_var_ptr((THD *)a_thd, offset, true);
}


void plugin_thdvar_init(THD *thd)
{
  plugin_ref old_table_plugin= thd->variables.table_plugin;
  DBUG_ENTER("plugin_thdvar_init");
  
  thd->variables.table_plugin= NULL;
  cleanup_variables(thd, &thd->variables);
  
  thd->variables= global_system_variables;
  thd->variables.table_plugin= NULL;

  /* we are going to allocate these lazily */
  thd->variables.dynamic_variables_version= 0;
  thd->variables.dynamic_variables_size= 0;
  thd->variables.dynamic_variables_ptr= 0;

  pthread_mutex_lock(&LOCK_plugin);  
  thd->variables.table_plugin=
        my_intern_plugin_lock(NULL, global_system_variables.table_plugin);
  intern_plugin_unlock(NULL, old_table_plugin);
  pthread_mutex_unlock(&LOCK_plugin);
  DBUG_VOID_RETURN;
}


/*
  Unlocks all system variables which hold a reference
*/
static void unlock_variables(THD *thd, struct system_variables *vars)
{
  intern_plugin_unlock(NULL, vars->table_plugin);
  vars->table_plugin= NULL;
}


/*
  Frees memory used by system variables

  Unlike plugin_vars_free_values() it frees all variables of all plugins,
  it's used on shutdown.
*/
static void cleanup_variables(THD *thd, struct system_variables *vars)
{
  st_bookmark *v;
  sys_var_pluginvar *pivar;
  sys_var *var;
  int flags;
  uint idx;

  rw_rdlock(&LOCK_system_variables_hash);
  for (idx= 0; idx < bookmark_hash.records; idx++)
  {
    v= (st_bookmark*) hash_element(&bookmark_hash, idx);
    if (v->version > vars->dynamic_variables_version ||
        !(var= intern_find_sys_var(v->key + 1, v->name_len, true)) ||
        !(pivar= var->cast_pluginvar()) ||
        v->key[0] != (pivar->plugin_var->flags & PLUGIN_VAR_TYPEMASK))
      continue;

    flags= pivar->plugin_var->flags;

    if ((flags & PLUGIN_VAR_TYPEMASK) == PLUGIN_VAR_STR &&
        flags & PLUGIN_VAR_THDLOCAL && flags & PLUGIN_VAR_MEMALLOC)
    {
      char **ptr= (char**) pivar->real_value_ptr(thd, OPT_SESSION);
      my_free(*ptr, MYF(MY_WME | MY_FAE | MY_ALLOW_ZERO_PTR));
      *ptr= NULL;
    }
  }
  rw_unlock(&LOCK_system_variables_hash);

  DBUG_ASSERT(vars->table_plugin == NULL);

  my_free(vars->dynamic_variables_ptr, MYF(MY_ALLOW_ZERO_PTR));
  vars->dynamic_variables_ptr= NULL;
  vars->dynamic_variables_size= 0;
  vars->dynamic_variables_version= 0;
}


void plugin_thdvar_cleanup(THD *thd)
{
  uint idx;
  plugin_ref *list;
  DBUG_ENTER("plugin_thdvar_cleanup");

  pthread_mutex_lock(&LOCK_plugin);

  unlock_variables(thd, &thd->variables);
  cleanup_variables(thd, &thd->variables);

  if ((idx= thd->lex->plugins.elements))
  {
    list= ((plugin_ref*) thd->lex->plugins.buffer) + idx - 1;
    DBUG_PRINT("info",("unlocking %d plugins", idx));
    while ((uchar*) list >= thd->lex->plugins.buffer)
      intern_plugin_unlock(NULL, *list--);
  }

  reap_plugins();
  pthread_mutex_unlock(&LOCK_plugin);

  reset_dynamic(&thd->lex->plugins);

  DBUG_VOID_RETURN;
}


/**
  @brief Free values of thread variables of a plugin.

  This must be called before a plugin is deleted. Otherwise its
  variables are no longer accessible and the value space is lost. Note
  that only string values with PLUGIN_VAR_MEMALLOC are allocated and
  must be freed.

  @param[in]        vars        Chain of system variables of a plugin
*/

static void plugin_vars_free_values(sys_var *vars)
{
  DBUG_ENTER("plugin_vars_free_values");

  for (sys_var *var= vars; var; var= var->next)
  {
    sys_var_pluginvar *piv= var->cast_pluginvar();
    if (piv &&
        ((piv->plugin_var->flags & PLUGIN_VAR_TYPEMASK) == PLUGIN_VAR_STR) &&
        (piv->plugin_var->flags & PLUGIN_VAR_MEMALLOC))
    {
      /* Free the string from global_system_variables. */
      char **valptr= (char**) piv->real_value_ptr(NULL, OPT_GLOBAL);
      DBUG_PRINT("plugin", ("freeing value for: '%s'  addr: 0x%lx",
                            var->name, (long) valptr));
      my_free(*valptr, MYF(MY_WME | MY_FAE | MY_ALLOW_ZERO_PTR));
      *valptr= NULL;
    }
  }
  DBUG_VOID_RETURN;
}


bool sys_var_pluginvar::check_update_type(Item_result type)
{
  if (is_readonly())
    return 1;
  switch (plugin_var->flags & PLUGIN_VAR_TYPEMASK) {
  case PLUGIN_VAR_INT:
  case PLUGIN_VAR_LONG:
  case PLUGIN_VAR_LONGLONG:
    return type != INT_RESULT;
  case PLUGIN_VAR_STR:
    return type != STRING_RESULT;
  default:
    return 0;
  }
}


SHOW_TYPE sys_var_pluginvar::show_type()
{
  switch (plugin_var->flags & PLUGIN_VAR_TYPEMASK) {
  case PLUGIN_VAR_BOOL:
    return SHOW_MY_BOOL;
  case PLUGIN_VAR_INT:
    return SHOW_INT;
  case PLUGIN_VAR_LONG:
    return SHOW_LONG;
  case PLUGIN_VAR_LONGLONG:
    return SHOW_LONGLONG;
  case PLUGIN_VAR_STR:
    return SHOW_CHAR_PTR;
  case PLUGIN_VAR_ENUM:
  case PLUGIN_VAR_SET:
    return SHOW_CHAR;
  default:
    DBUG_ASSERT(0);
    return SHOW_UNDEF;
  }
}


uchar* sys_var_pluginvar::real_value_ptr(THD *thd, enum_var_type type)
{
  DBUG_ASSERT(thd || (type == OPT_GLOBAL));
  if (plugin_var->flags & PLUGIN_VAR_THDLOCAL)
  {
    if (type == OPT_GLOBAL)
      thd= NULL;

    return intern_sys_var_ptr(thd, *(int*) (plugin_var+1), false);
  }
  return *(uchar**) (plugin_var+1);
}


TYPELIB* sys_var_pluginvar::plugin_var_typelib(void)
{
  switch (plugin_var->flags & (PLUGIN_VAR_TYPEMASK | PLUGIN_VAR_THDLOCAL)) {
  case PLUGIN_VAR_ENUM:
    return ((sysvar_enum_t *)plugin_var)->typelib;
  case PLUGIN_VAR_SET:
    return ((sysvar_set_t *)plugin_var)->typelib;
  case PLUGIN_VAR_ENUM | PLUGIN_VAR_THDLOCAL:
    return ((thdvar_enum_t *)plugin_var)->typelib;
  case PLUGIN_VAR_SET | PLUGIN_VAR_THDLOCAL:
    return ((thdvar_set_t *)plugin_var)->typelib;
  default:
    return NULL;
  }
  return NULL;
}


uchar* sys_var_pluginvar::value_ptr(THD *thd, enum_var_type type,
                                   LEX_STRING *base)
{
  uchar* result;

  result= real_value_ptr(thd, type);

  if ((plugin_var->flags & PLUGIN_VAR_TYPEMASK) == PLUGIN_VAR_ENUM)
    result= (uchar*) get_type(plugin_var_typelib(), *(ulong*)result);
  else if ((plugin_var->flags & PLUGIN_VAR_TYPEMASK) == PLUGIN_VAR_SET)
  {
    char buffer[STRING_BUFFER_USUAL_SIZE];
    String str(buffer, sizeof(buffer), system_charset_info);
    TYPELIB *typelib= plugin_var_typelib();
    ulonglong mask= 1, value= *(ulonglong*) result;
    uint i;

    str.length(0);
    for (i= 0; i < typelib->count; i++, mask<<=1)
    {
      if (!(value & mask))
        continue;
      str.append(typelib->type_names[i], typelib->type_lengths[i]);
      str.append(',');
    }

    result= (uchar*) "";
    if (str.length())
      result= (uchar*) thd->strmake(str.ptr(), str.length()-1);
  }
  return result;
}


bool sys_var_pluginvar::check(THD *thd, set_var *var)
{
  st_item_value_holder value;
  DBUG_ASSERT(is_readonly() || plugin_var->check);

  value.value_type= item_value_type;
  value.val_str= item_val_str;
  value.val_int= item_val_int;
  value.val_real= item_val_real;
  value.item= var->value;

  return is_readonly() ||
         plugin_var->check(thd, plugin_var, &var->save_result, &value);
}


void sys_var_pluginvar::set_default(THD *thd, enum_var_type type)
{
  void *tgt, *src;

  DBUG_ASSERT(is_readonly() || plugin_var->update);

  if (is_readonly())
    return;

  tgt= real_value_ptr(thd, type);
  src= ((void **) (plugin_var + 1) + 1);

  if (plugin_var->flags & PLUGIN_VAR_THDLOCAL)
  {
    src= ((int*) (plugin_var + 1) + 1);
    if (type != OPT_GLOBAL)
      src= real_value_ptr(thd, OPT_GLOBAL);
  }

  /* thd must equal current_thd if PLUGIN_VAR_THDLOCAL flag is set */
  DBUG_ASSERT(!(plugin_var->flags & PLUGIN_VAR_THDLOCAL) ||
              thd == current_thd);

  if (!(plugin_var->flags & PLUGIN_VAR_THDLOCAL) || type == OPT_GLOBAL)
  {
    pthread_mutex_lock(&LOCK_plugin);
    plugin_var->update(thd, plugin_var, tgt, src);
    pthread_mutex_unlock(&LOCK_plugin);
  }
  else
    plugin_var->update(thd, plugin_var, tgt, src);
}


bool sys_var_pluginvar::update(THD *thd, set_var *var)
{
  void *tgt;

  DBUG_ASSERT(is_readonly() || plugin_var->update);

  /* thd must equal current_thd if PLUGIN_VAR_THDLOCAL flag is set */
  DBUG_ASSERT(!(plugin_var->flags & PLUGIN_VAR_THDLOCAL) ||
              thd == current_thd);

  if (is_readonly())
    return 1;

  pthread_mutex_lock(&LOCK_global_system_variables);
  tgt= real_value_ptr(thd, var->type);

  if (!(plugin_var->flags & PLUGIN_VAR_THDLOCAL) || var->type == OPT_GLOBAL)
  {
    /* variable we are updating has global scope, so we unlock after updating */
    plugin_var->update(thd, plugin_var, tgt, &var->save_result);
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
  {
    pthread_mutex_unlock(&LOCK_global_system_variables);
    plugin_var->update(thd, plugin_var, tgt, &var->save_result);
  }
 return 0;
}


#define OPTION_SET_LIMITS(type, options, opt) \
  options->var_type= type; \
  options->def_value= (opt)->def_val; \
  options->min_value= (opt)->min_val; \
  options->max_value= (opt)->max_val; \
  options->block_size= (long) (opt)->blk_sz


static void plugin_opt_set_limits(struct my_option *options,
                                  const struct st_mysql_sys_var *opt)
{
  switch (opt->flags & (PLUGIN_VAR_TYPEMASK |
                        PLUGIN_VAR_UNSIGNED | PLUGIN_VAR_THDLOCAL)) {
  /* global system variables */
  case PLUGIN_VAR_INT:
    OPTION_SET_LIMITS(GET_INT, options, (sysvar_int_t*) opt);
    break;
  case PLUGIN_VAR_INT | PLUGIN_VAR_UNSIGNED:
    OPTION_SET_LIMITS(GET_UINT, options, (sysvar_uint_t*) opt);
    break;
  case PLUGIN_VAR_LONG:
    OPTION_SET_LIMITS(GET_LONG, options, (sysvar_long_t*) opt);
    break;
  case PLUGIN_VAR_LONG | PLUGIN_VAR_UNSIGNED:
    OPTION_SET_LIMITS(GET_ULONG, options, (sysvar_ulong_t*) opt);
    break;
  case PLUGIN_VAR_LONGLONG:
    OPTION_SET_LIMITS(GET_LL, options, (sysvar_longlong_t*) opt);
    break;
  case PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED:
    OPTION_SET_LIMITS(GET_ULL, options, (sysvar_ulonglong_t*) opt);
    break;
  case PLUGIN_VAR_ENUM:
    options->var_type= GET_ENUM;
    options->typelib= ((sysvar_enum_t*) opt)->typelib;
    options->def_value= *(ulong*) ((int*) (opt + 1) + 1);
    options->min_value= options->block_size= 0;
    options->max_value= options->typelib->count - 1;
    break;
  case PLUGIN_VAR_SET:
    options->var_type= GET_SET;
    options->typelib= ((sysvar_set_t*) opt)->typelib;
    options->def_value= *(ulonglong*) ((int*) (opt + 1) + 1);
    options->min_value= options->block_size= 0;
    options->max_value= (ULL(1) << options->typelib->count) - 1;
    break;
  case PLUGIN_VAR_BOOL:
    options->var_type= GET_BOOL;
    options->def_value= *(my_bool*) ((void**)(opt + 1) + 1);
    break;
  case PLUGIN_VAR_STR:
    options->var_type= ((opt->flags & PLUGIN_VAR_MEMALLOC) ?
                        GET_STR_ALLOC : GET_STR);
    options->def_value= (ulonglong)(intptr) *((char**) ((void**) (opt + 1) + 1));
    break;
  /* threadlocal variables */
  case PLUGIN_VAR_INT | PLUGIN_VAR_THDLOCAL:
    OPTION_SET_LIMITS(GET_INT, options, (thdvar_int_t*) opt);
    break;
  case PLUGIN_VAR_INT | PLUGIN_VAR_UNSIGNED | PLUGIN_VAR_THDLOCAL:
    OPTION_SET_LIMITS(GET_UINT, options, (thdvar_uint_t*) opt);
    break;
  case PLUGIN_VAR_LONG | PLUGIN_VAR_THDLOCAL:
    OPTION_SET_LIMITS(GET_LONG, options, (thdvar_long_t*) opt);
    break;
  case PLUGIN_VAR_LONG | PLUGIN_VAR_UNSIGNED | PLUGIN_VAR_THDLOCAL:
    OPTION_SET_LIMITS(GET_ULONG, options, (thdvar_ulong_t*) opt);
    break;
  case PLUGIN_VAR_LONGLONG | PLUGIN_VAR_THDLOCAL:
    OPTION_SET_LIMITS(GET_LL, options, (thdvar_longlong_t*) opt);
    break;
  case PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED | PLUGIN_VAR_THDLOCAL:
    OPTION_SET_LIMITS(GET_ULL, options, (thdvar_ulonglong_t*) opt);
    break;
  case PLUGIN_VAR_ENUM | PLUGIN_VAR_THDLOCAL:
    options->var_type= GET_ENUM;
    options->typelib= ((thdvar_enum_t*) opt)->typelib;
    options->def_value= *(ulong*) ((int*) (opt + 1) + 1);
    options->min_value= options->block_size= 0;
    options->max_value= options->typelib->count - 1;
    break;
  case PLUGIN_VAR_SET | PLUGIN_VAR_THDLOCAL:
    options->var_type= GET_SET;
    options->typelib= ((thdvar_set_t*) opt)->typelib;
    options->def_value= *(ulonglong*) ((int*) (opt + 1) + 1);
    options->min_value= options->block_size= 0;
    options->max_value= (ULL(1) << options->typelib->count) - 1;
    break;
  case PLUGIN_VAR_BOOL | PLUGIN_VAR_THDLOCAL:
    options->var_type= GET_BOOL;
    options->def_value= *(my_bool*) ((int*) (opt + 1) + 1);
    break;
  case PLUGIN_VAR_STR | PLUGIN_VAR_THDLOCAL:
    options->var_type= ((opt->flags & PLUGIN_VAR_MEMALLOC) ?
                        GET_STR_ALLOC : GET_STR);
    options->def_value= (intptr) *((char**) ((void**) (opt + 1) + 1));
    break;
  default:
    DBUG_ASSERT(0);
  }
  options->arg_type= REQUIRED_ARG;
  if (opt->flags & PLUGIN_VAR_NOCMDARG)
    options->arg_type= NO_ARG;
  if (opt->flags & PLUGIN_VAR_OPCMDARG)
    options->arg_type= OPT_ARG;
}

extern "C" my_bool get_one_plugin_option(int optid, const struct my_option *,
                                         char *);

my_bool get_one_plugin_option(int optid __attribute__((unused)),
                              const struct my_option *opt,
                              char *argument)
{
  return 0;
}


static int construct_options(MEM_ROOT *mem_root, struct st_plugin_int *tmp,
                             my_option *options, my_bool **enabled,
                             bool can_disable)
{
  const char *plugin_name= tmp->plugin->name;
  uint namelen= strlen(plugin_name), optnamelen;
  uint buffer_length= namelen * 4 + (can_disable ? 75 : 10);
  char *name= (char*) alloc_root(mem_root, buffer_length) + 1;
  char *optname, *p;
  int index= 0, offset= 0;
  st_mysql_sys_var *opt, **plugin_option;
  st_bookmark *v;
  DBUG_ENTER("construct_options");
  DBUG_PRINT("plugin", ("plugin: '%s'  enabled: %d  can_disable: %d",
                        plugin_name, **enabled, can_disable));

  /* support --skip-plugin-foo syntax */
  memcpy(name, plugin_name, namelen + 1);
  my_casedn_str(&my_charset_latin1, name);
  strxmov(name + namelen + 1, "plugin-", name, NullS);
  /* Now we have namelen + 1 + 7 + namelen + 1 == namelen * 2 + 9. */

  for (p= name + namelen*2 + 8; p > name; p--)
    if (*p == '_')
      *p= '-';

  if (can_disable)
  {
    strxmov(name + namelen*2 + 10, "Enable ", plugin_name, " plugin. "
            "Disable with --skip-", name," (will save memory).", NullS);
    /*
      Now we have namelen * 2 + 10 (one char unused) + 7 + namelen + 9 +
      20 + namelen + 20 + 1 == namelen * 4 + 67.
    */

    options[0].comment= name + namelen*2 + 10;
  }

  /*
    NOTE: 'name' is one char above the allocated buffer!
    NOTE: This code assumes that 'my_bool' and 'char' are of same size.
  */
  *((my_bool *)(name -1))= **enabled;
  *enabled= (my_bool *)(name - 1);


  options[1].name= (options[0].name= name) + namelen + 1;
  options[0].id= options[1].id= 256; /* must be >255. dup id ok */
  options[0].var_type= options[1].var_type= GET_BOOL;
  options[0].arg_type= options[1].arg_type= NO_ARG;
  options[0].def_value= options[1].def_value= **enabled;
  options[0].value= options[0].u_max_value=
  options[1].value= options[1].u_max_value= (uchar**) (name - 1);
  options+= 2;

  /*
    Two passes as the 2nd pass will take pointer addresses for use
    by my_getopt and register_var() in the first pass uses realloc
  */

  for (plugin_option= tmp->plugin->system_vars;
       plugin_option && *plugin_option; plugin_option++, index++)
  {
    opt= *plugin_option;
    if (!(opt->flags & PLUGIN_VAR_THDLOCAL))
      continue;
    if (!(register_var(name, opt->name, opt->flags)))
      continue;
    switch (opt->flags & PLUGIN_VAR_TYPEMASK) {
    case PLUGIN_VAR_BOOL:
      SET_PLUGIN_VAR_RESOLVE((thdvar_bool_t *) opt);
      break;
    case PLUGIN_VAR_INT:
      SET_PLUGIN_VAR_RESOLVE((thdvar_int_t *) opt);
      break;
    case PLUGIN_VAR_LONG:
      SET_PLUGIN_VAR_RESOLVE((thdvar_long_t *) opt);
      break;
    case PLUGIN_VAR_LONGLONG:
      SET_PLUGIN_VAR_RESOLVE((thdvar_longlong_t *) opt);
      break;
    case PLUGIN_VAR_STR:
      SET_PLUGIN_VAR_RESOLVE((thdvar_str_t *) opt);
      break;
    case PLUGIN_VAR_ENUM:
      SET_PLUGIN_VAR_RESOLVE((thdvar_enum_t *) opt);
      break;
    case PLUGIN_VAR_SET:
      SET_PLUGIN_VAR_RESOLVE((thdvar_set_t *) opt);
      break;
    default:
      sql_print_error("Unknown variable type code 0x%x in plugin '%s'.",
                      opt->flags, plugin_name);
      DBUG_RETURN(-1);
    };
  }

  for (plugin_option= tmp->plugin->system_vars;
       plugin_option && *plugin_option; plugin_option++, index++)
  {
    switch ((opt= *plugin_option)->flags & PLUGIN_VAR_TYPEMASK) {
    case PLUGIN_VAR_BOOL:
      if (!opt->check)
        opt->check= check_func_bool;
      if (!opt->update)
        opt->update= update_func_bool;
      break;
    case PLUGIN_VAR_INT:
      if (!opt->check)
        opt->check= check_func_int;
      if (!opt->update)
        opt->update= update_func_int;
      break;
    case PLUGIN_VAR_LONG:
      if (!opt->check)
        opt->check= check_func_long;
      if (!opt->update)
        opt->update= update_func_long;
      break;
    case PLUGIN_VAR_LONGLONG:
      if (!opt->check)
        opt->check= check_func_longlong;
      if (!opt->update)
        opt->update= update_func_longlong;
      break;
    case PLUGIN_VAR_STR:
      if (!opt->check)
        opt->check= check_func_str;
      if (!opt->update)
      {
        opt->update= update_func_str;
        if (!(opt->flags & PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY))
        {
          opt->flags|= PLUGIN_VAR_READONLY;
          sql_print_warning("Server variable %s of plugin %s was forced "
                            "to be read-only: string variable without "
                            "update_func and PLUGIN_VAR_MEMALLOC flag",
                            opt->name, plugin_name);
        }
      }
      break;
    case PLUGIN_VAR_ENUM:
      if (!opt->check)
        opt->check= check_func_enum;
      if (!opt->update)
        opt->update= update_func_long;
      break;
    case PLUGIN_VAR_SET:
      if (!opt->check)
        opt->check= check_func_set;
      if (!opt->update)
        opt->update= update_func_longlong;
      break;
    default:
      sql_print_error("Unknown variable type code 0x%x in plugin '%s'.",
                      opt->flags, plugin_name);
      DBUG_RETURN(-1);
    }

    if (opt->flags & PLUGIN_VAR_NOCMDOPT)
      continue;

    if (!opt->name)
    {
      sql_print_error("Missing variable name in plugin '%s'.",
                      plugin_name);
      DBUG_RETURN(-1);
    }

    if (!(v= find_bookmark(name, opt->name, opt->flags)))
    {
      optnamelen= strlen(opt->name);
      optname= (char*) alloc_root(mem_root, namelen + optnamelen + 2);
      strxmov(optname, name, "-", opt->name, NullS);
      optnamelen= namelen + optnamelen + 1;
    }
    else
      optname= (char*) memdup_root(mem_root, v->key + 1, (optnamelen= v->name_len) + 1);

    /* convert '_' to '-' */
    for (p= optname; *p; p++)
      if (*p == '_')
        *p= '-';

    options->name= optname;
    options->comment= opt->comment;
    options->app_type= opt;
    options->id= (options-1)->id + 1;

    if (opt->flags & PLUGIN_VAR_THDLOCAL)
      *(int*)(opt + 1)= offset= v->offset;

    plugin_opt_set_limits(options, opt);

    if ((opt->flags & PLUGIN_VAR_TYPEMASK) != PLUGIN_VAR_ENUM &&
        (opt->flags & PLUGIN_VAR_TYPEMASK) != PLUGIN_VAR_SET)
    {
      if (opt->flags & PLUGIN_VAR_THDLOCAL)
        options->value= options->u_max_value= (uchar**)
          (global_system_variables.dynamic_variables_ptr + offset);
      else
        options->value= options->u_max_value= *(uchar***) (opt + 1);
    }

    options[1]= options[0];
    options[1].name= p= (char*) alloc_root(mem_root, optnamelen + 8);
    options[1].comment= 0; // hidden
    strxmov(p, "plugin-", optname, NullS);

    options+= 2;
  }

  DBUG_RETURN(0);
}


static my_option *construct_help_options(MEM_ROOT *mem_root,
                                         struct st_plugin_int *p)
{
  st_mysql_sys_var **opt;
  my_option *opts;
  my_bool dummy, can_disable;
  my_bool *dummy2= &dummy;
  uint count= EXTRA_OPTIONS;
  DBUG_ENTER("construct_help_options");

  for (opt= p->plugin->system_vars; opt && *opt; opt++, count+= 2);

  if (!(opts= (my_option*) alloc_root(mem_root, sizeof(my_option) * count)))
    DBUG_RETURN(NULL);

  bzero(opts, sizeof(my_option) * count);

  dummy= TRUE; /* plugin is enabled. */

  can_disable=
      my_strcasecmp(&my_charset_latin1, p->name.str, "MyISAM") &&
      my_strcasecmp(&my_charset_latin1, p->name.str, "MEMORY");

  if (construct_options(mem_root, p, opts, &dummy2, can_disable))
    DBUG_RETURN(NULL);

  DBUG_RETURN(opts);
}


/*
  SYNOPSIS
    test_plugin_options()
    tmp_root                    temporary scratch space
    plugin                      internal plugin structure
    argc                        user supplied arguments
    argv                        user supplied arguments
    default_enabled             default plugin enable status
  RETURNS:
    0 SUCCESS - plugin should be enabled/loaded
  NOTE:
    Requires that a write-lock is held on LOCK_system_variables_hash
*/
static int test_plugin_options(MEM_ROOT *tmp_root, struct st_plugin_int *tmp,
                               int *argc, char **argv, my_bool default_enabled)
{
  struct sys_var_chain chain= { NULL, NULL };
  my_bool enabled_saved= default_enabled, can_disable;
  my_bool *enabled= &default_enabled;
  MEM_ROOT *mem_root= alloc_root_inited(&tmp->mem_root) ?
                      &tmp->mem_root : &plugin_mem_root;
  st_mysql_sys_var **opt;
  my_option *opts= NULL;
  char *p, *varname;
  int error;
  st_mysql_sys_var *o;
  sys_var *v;
  struct st_bookmark *var;
  uint len, count= EXTRA_OPTIONS;
  DBUG_ENTER("test_plugin_options");
  DBUG_ASSERT(tmp->plugin && tmp->name.str);

  for (opt= tmp->plugin->system_vars; opt && *opt; opt++)
    count+= 2; /* --{plugin}-{optname} and --plugin-{plugin}-{optname} */

  can_disable=
      my_strcasecmp(&my_charset_latin1, tmp->name.str, "MyISAM") &&
      my_strcasecmp(&my_charset_latin1, tmp->name.str, "MEMORY");

  if (count > EXTRA_OPTIONS || (*argc > 1))
  {
    if (!(opts= (my_option*) alloc_root(tmp_root, sizeof(my_option) * count)))
    {
      sql_print_error("Out of memory for plugin '%s'.", tmp->name.str);
      DBUG_RETURN(-1);
    }
    bzero(opts, sizeof(my_option) * count);

    if (construct_options(tmp_root, tmp, opts, &enabled, can_disable))
    {
      sql_print_error("Bad options for plugin '%s'.", tmp->name.str);
      DBUG_RETURN(-1);
    }

    error= handle_options(argc, &argv, opts, get_one_plugin_option);
    (*argc)++; /* add back one for the program name */

    if (error)
    {
       sql_print_error("Parsing options for plugin '%s' failed.",
                       tmp->name.str);
       goto err;
    }
  }

  if (!*enabled && !can_disable)
  {
    sql_print_warning("Plugin '%s' cannot be disabled", tmp->name.str);
    *enabled= TRUE;
  }

  error= 1;

  if (*enabled)
  {
    for (opt= tmp->plugin->system_vars; opt && *opt; opt++)
    {
      if (((o= *opt)->flags & PLUGIN_VAR_NOSYSVAR))
        continue;

      if ((var= find_bookmark(tmp->name.str, o->name, o->flags)))
        v= new (mem_root) sys_var_pluginvar(var->key + 1, o);
      else
      {
        len= tmp->name.length + strlen(o->name) + 2;
        varname= (char*) alloc_root(mem_root, len);
        strxmov(varname, tmp->name.str, "-", o->name, NullS);
        my_casedn_str(&my_charset_latin1, varname);

        for (p= varname; *p; p++)
          if (*p == '-')
            *p= '_';

        v= new (mem_root) sys_var_pluginvar(varname, o);
      }
      DBUG_ASSERT(v); /* check that an object was actually constructed */

      /*
        Add to the chain of variables.
        Done like this for easier debugging so that the
        pointer to v is not lost on optimized builds.
      */
      v->chain_sys_var(&chain);
    }
    if (chain.first)
    {
      chain.last->next = NULL;
      if (mysql_add_sys_var_chain(chain.first, NULL))
      {
        sql_print_error("Plugin '%s' has conflicting system variables",
                        tmp->name.str);
        goto err;
      }
      tmp->system_vars= chain.first;
    }
    DBUG_RETURN(0);
  }

  if (enabled_saved && global_system_variables.log_warnings)
    sql_print_information("Plugin '%s' disabled by command line option",
                          tmp->name.str);
err:
  if (opts)
    my_cleanup_options(opts);
  DBUG_RETURN(error);
}


/****************************************************************************
  Help Verbose text with Plugin System Variables
****************************************************************************/

static int option_cmp(my_option *a, my_option *b)
{
  return my_strcasecmp(&my_charset_latin1, a->name, b->name);
}


void my_print_help_inc_plugins(my_option *main_options, uint size)
{
  DYNAMIC_ARRAY all_options;
  struct st_plugin_int *p;
  MEM_ROOT mem_root;
  my_option *opt;

  init_alloc_root(&mem_root, 4096, 4096);
  my_init_dynamic_array(&all_options, sizeof(my_option), size, size/4);

  if (initialized)
    for (uint idx= 0; idx < plugin_array.elements; idx++)
    {
      p= *dynamic_element(&plugin_array, idx, struct st_plugin_int **);

      if (!p->plugin->system_vars ||
          !(opt= construct_help_options(&mem_root, p)))
        continue;

      /* Only options with a non-NULL comment are displayed in help text */
      for (;opt->id; opt++)
        if (opt->comment)
          insert_dynamic(&all_options, (uchar*) opt);
    }

  for (;main_options->id; main_options++)
    insert_dynamic(&all_options, (uchar*) main_options);

  sort_dynamic(&all_options, (qsort_cmp) option_cmp);

  /* main_options now points to the empty option terminator */
  insert_dynamic(&all_options, (uchar*) main_options);

  my_print_help((my_option*) all_options.buffer);
  my_print_variables((my_option*) all_options.buffer);

  delete_dynamic(&all_options);
  free_root(&mem_root, MYF(0));
}

