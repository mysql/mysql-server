/*
   Copyright (c) 2005, 2013, Oracle and/or its affiliates.
   Copyright (c) 2010, 2014, SkySQL Ab.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "sql_priv.h"                         // SHOW_MY_BOOL
#include "unireg.h"
#include "my_global.h"                       // REQUIRED by m_string.h
#include "sql_class.h"                          // set_var.h: THD
#include "sys_vars_shared.h"
#include "sql_locale.h"
#include "sql_plugin.h"
#include "sql_parse.h"          // check_table_access
#include "sql_base.h"                           // close_mysql_tables
#include "key.h"                                // key_copy
#include "sql_show.h"           // remove_status_vars, add_status_vars
#include "strfunc.h"            // find_set
#include "sql_acl.h"                       // *_ACL
#include "records.h"          // init_read_record, end_read_record
#include <my_pthread.h>
#include <my_getopt.h>
#include "sql_audit.h"
#include <mysql/plugin_auth.h>
#include "lock.h"                               // MYSQL_LOCK_IGNORE_TIMEOUT
#include <mysql/plugin_auth.h>
#include "sql_plugin_compat.h"

#define REPORT_TO_LOG  1
#define REPORT_TO_USER 2

extern struct st_maria_plugin *mysql_optional_plugins[];
extern struct st_maria_plugin *mysql_mandatory_plugins[];

/**
  @note The order of the enumeration is critical.
  @see construct_options
*/
const char *global_plugin_typelib_names[]=
  { "OFF", "ON", "FORCE", "FORCE_PLUS_PERMANENT", NULL };
static TYPELIB global_plugin_typelib=
  { array_elements(global_plugin_typelib_names)-1,
    "", global_plugin_typelib_names, NULL };


char *opt_plugin_load= NULL;
char *opt_plugin_dir_ptr;
char opt_plugin_dir[FN_REFLEN];
ulong plugin_maturity;

/*
  not really needed now, this map will become essential when we add more
  maturity levels. We cannot change existing maturity constants,
  so the next value - even if it will be MariaDB_PLUGIN_MATURITY_VERY_BUGGY -
  will inevitably be larger than MariaDB_PLUGIN_MATURITY_STABLE.
  To be able to compare them we use this mapping array
*/
uint plugin_maturity_map[]=
{ 0, 1, 2, 3, 4, 5, 6 };

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
  { C_STRING_WITH_LEN("INFORMATION SCHEMA") },
  { C_STRING_WITH_LEN("AUDIT") },
  { C_STRING_WITH_LEN("REPLICATION") },
  { C_STRING_WITH_LEN("AUTHENTICATION") }
};

extern int initialize_schema_table(st_plugin_int *plugin);
extern int finalize_schema_table(st_plugin_int *plugin);

extern int initialize_audit_plugin(st_plugin_int *plugin);
extern int finalize_audit_plugin(st_plugin_int *plugin);

/*
  The number of elements in both plugin_type_initialize and
  plugin_type_deinitialize should equal to the number of plugins
  defined.
*/
plugin_type_init plugin_type_initialize[MYSQL_MAX_PLUGIN_TYPE_NUM]=
{
  0,ha_initialize_handlerton,0,0,initialize_schema_table,
  initialize_audit_plugin, 0, 0
};

plugin_type_init plugin_type_deinitialize[MYSQL_MAX_PLUGIN_TYPE_NUM]=
{
  0,ha_finalize_handlerton,0,0,finalize_schema_table,
  finalize_audit_plugin, 0, 0
};

#ifdef HAVE_DLOPEN
static const char *plugin_interface_version_sym=
                   "_mysql_plugin_interface_version_";
static const char *sizeof_st_plugin_sym=
                   "_mysql_sizeof_struct_st_plugin_";
static const char *plugin_declarations_sym= "_mysql_plugin_declarations_";
static int min_plugin_interface_version= MYSQL_PLUGIN_INTERFACE_VERSION & ~0xFF;
static const char *maria_plugin_interface_version_sym=
                   "_maria_plugin_interface_version_";
static const char *maria_sizeof_st_plugin_sym=
                   "_maria_sizeof_struct_st_plugin_";
static const char *maria_plugin_declarations_sym=
                   "_maria_plugin_declarations_";
static int min_maria_plugin_interface_version=
                   MARIA_PLUGIN_INTERFACE_VERSION & ~0xFF;
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
  MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION,
  MYSQL_AUDIT_INTERFACE_VERSION,
  MYSQL_REPLICATION_INTERFACE_VERSION,
  MIN_AUTHENTICATION_INTERFACE_VERSION
};
static int cur_plugin_info_interface_version[MYSQL_MAX_PLUGIN_TYPE_NUM]=
{
  0x0000, /* UDF: not implemented */
  MYSQL_HANDLERTON_INTERFACE_VERSION,
  MYSQL_FTPARSER_INTERFACE_VERSION,
  MYSQL_DAEMON_INTERFACE_VERSION,
  MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION,
  MYSQL_AUDIT_INTERFACE_VERSION,
  MYSQL_REPLICATION_INTERFACE_VERSION,
  MYSQL_AUTHENTICATION_INTERFACE_VERSION
};

static struct
{
  const char *plugin_name;
  enum enum_plugin_load_option override;
} override_plugin_load_policy[]={
  /*
    If the performance schema is compiled in,
    treat the storage engine plugin as 'mandatory',
    to suppress any plugin-level options such as '--performance-schema'.
    This is specific to the performance schema, and is done on purpose:
    the server-level option '--performance-schema' controls the overall
    performance schema initialization, which consists of much more that
    the underlying storage engine initialization.
    See mysqld.cc, set_vars.cc.
    Suppressing ways to interfere directly with the storage engine alone
    prevents awkward situations where:
    - the user wants the performance schema functionality, by using
      '--enable-performance-schema' (the server option),
    - yet disable explicitly a component needed for the functionality
      to work, by using '--skip-performance-schema' (the plugin)
  */
  { "performance_schema", PLUGIN_FORCE },

  /* we disable few other plugins by default */
  { "ndbcluster", PLUGIN_OFF },
  { "feedback", PLUGIN_OFF }
};

/* support for Services */

#include "sql_plugin_services.h"

/*
  A mutex LOCK_plugin must be acquired before accessing the
  following variables/structures.
  We are always manipulating ref count, so a rwlock here is unneccessary.
*/
mysql_mutex_t LOCK_plugin;
static DYNAMIC_ARRAY plugin_dl_array;
static DYNAMIC_ARRAY plugin_array;
static HASH plugin_hash[MYSQL_MAX_PLUGIN_TYPE_NUM];
static MEM_ROOT plugin_mem_root;
static bool reap_needed= false;
static int plugin_array_version=0;

static bool initialized= 0;

/*
  write-lock on LOCK_system_variables_hash is required before modifying
  the following variables/structures
*/
static MEM_ROOT plugin_vars_mem_root;
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

static SHOW_TYPE pluginvar_show_type(st_mysql_sys_var *plugin_var);


/*
  sys_var class for access to all plugin variables visible to the user
*/
class sys_var_pluginvar: public sys_var
{
public:
  struct st_plugin_int *plugin;
  struct st_mysql_sys_var *plugin_var;
  /**
    variable name from whatever is hard-coded in the plugin source
    and doesn't have pluginname- prefix is replaced by an allocated name
    with a plugin prefix. When plugin is uninstalled we need to restore the
    pointer to point to the hard-coded value, because plugin may be
    installed/uninstalled many times without reloading the shared object.
  */
  const char *orig_pluginvar_name;

  static void *operator new(size_t size, MEM_ROOT *mem_root)
  { return (void*) alloc_root(mem_root, size); }
  static void operator delete(void *ptr_arg,size_t size)
  { TRASH(ptr_arg, size); }

  sys_var_pluginvar(sys_var_chain *chain, const char *name_arg,
                    struct st_mysql_sys_var *plugin_var_arg,
                    struct st_plugin_int *plugin_arg)
    :sys_var(chain, name_arg, plugin_var_arg->comment,
             (plugin_var_arg->flags & PLUGIN_VAR_THDLOCAL ? SESSION : GLOBAL) |
             (plugin_var_arg->flags & PLUGIN_VAR_READONLY ? READONLY : 0),
             0, -1, NO_ARG, pluginvar_show_type(plugin_var_arg), 0, 0,
             VARIABLE_NOT_IN_BINLOG, NULL, NULL, NULL),
    plugin(plugin_arg), plugin_var(plugin_var_arg),
    orig_pluginvar_name(plugin_var_arg->name)
  { plugin_var->name= name_arg; }
  sys_var_pluginvar *cast_pluginvar() { return this; }
  bool check_update_type(Item_result type);
  SHOW_TYPE show_type();
  uchar* real_value_ptr(THD *thd, enum_var_type type);
  TYPELIB* plugin_var_typelib(void);
  uchar* do_value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  uchar* session_value_ptr(THD *thd, LEX_STRING *base)
  { return do_value_ptr(thd, OPT_SESSION, base); }
  uchar* global_value_ptr(THD *thd, LEX_STRING *base)
  { return do_value_ptr(thd, OPT_GLOBAL, base); }
  bool do_check(THD *thd, set_var *var);
  virtual void session_save_default(THD *thd, set_var *var) {}
  virtual void global_save_default(THD *thd, set_var *var) {}
  bool session_update(THD *thd, set_var *var);
  bool global_update(THD *thd, set_var *var);
};


/* prototypes */
static void plugin_load(MEM_ROOT *tmp_root);
static bool plugin_load_list(MEM_ROOT *, const char *);
static int test_plugin_options(MEM_ROOT *, struct st_plugin_int *,
                               int *, char **);
static bool register_builtin(struct st_maria_plugin *, struct st_plugin_int *,
                             struct st_plugin_int **);
static void unlock_variables(THD *thd, struct system_variables *vars);
static void cleanup_variables(THD *thd, struct system_variables *vars);
static void plugin_vars_free_values(sys_var *vars);
static void restore_pluginvar_names(sys_var *first);
static void plugin_opt_set_limits(struct my_option *,
                                  const struct st_mysql_sys_var *);
static plugin_ref intern_plugin_lock(LEX *lex, plugin_ref plugin);
static void intern_plugin_unlock(LEX *lex, plugin_ref plugin);
static void reap_plugins(void);

static void report_error(int where_to, uint error, ...)
{
  va_list args;
  DBUG_ASSERT(where_to & (REPORT_TO_USER | REPORT_TO_LOG));
  if (where_to & REPORT_TO_USER)
  {
    va_start(args, error);
    my_printv_error(error, ER(error), MYF(0), args);
    va_end(args);
  }
  if (where_to & REPORT_TO_LOG)
  {
    va_start(args, error);
    error_log_print(ERROR_LEVEL, ER_DEFAULT(error), args);
    va_end(args);
  }
}

/**
   Check if the provided path is valid in the sense that it does cause
   a relative reference outside the directory.

   @note Currently, this function only check if there are any
   characters in FN_DIRSEP in the string, but it might change in the
   future.

   @code
   check_valid_path("../foo.so") -> true
   check_valid_path("foo.so") -> false
   @endcode
 */
bool check_valid_path(const char *path, size_t len)
{
  size_t prefix= my_strcspn(files_charset_info, path, path + len, FN_DIRSEP);
  return  prefix < len;
}

static void fix_dl_name(MEM_ROOT *root, LEX_STRING *dl)
{
  const size_t so_ext_len= sizeof(SO_EXT) - 1;
  if (my_strcasecmp(&my_charset_latin1, dl->str + dl->length - so_ext_len,
                    SO_EXT))
  {
    char *s= (char*)alloc_root(root, dl->length + so_ext_len + 1);
    memcpy(s, dl->str, dl->length);
    strcpy(s + dl->length, SO_EXT);
    dl->str= s;
    dl->length+= so_ext_len;
  }
}


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
  return current_thd->strmake(res->ptr(), res->length());
}


static int item_val_int(struct st_mysql_value *value, long long *buf)
{
  Item *item= ((st_item_value_holder*)value)->item;
  *buf= item->val_int();
  if (item->is_null())
    return 1;
  return 0;
}

static int item_is_unsigned(struct st_mysql_value *value)
{
  Item *item= ((st_item_value_holder*)value)->item;
  return item->unsigned_flag;
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
  my_free(p->dl.str);
  if (p->allocated)
    my_free(p->plugins);
}


/**
  Reads data from mysql plugin interface

  @param plugin_dl       Structure where the data should be put
  @param sym             Reverence on version info
  @param dlpath          Path to the module
  @param report          What errors should be reported

  @retval FALSE OK
  @retval TRUE  ERROR
*/

#ifdef HAVE_DLOPEN
static my_bool read_mysql_plugin_info(struct st_plugin_dl *plugin_dl,
                                      void *sym, char *dlpath,
                                      int report)
{
  DBUG_ENTER("read_maria_plugin_info");
  /* Determine interface version */
  if (!sym)
  {
    free_plugin_mem(plugin_dl);
    report_error(report, ER_CANT_FIND_DL_ENTRY, plugin_interface_version_sym);
    DBUG_RETURN(TRUE);
  }
  plugin_dl->mariaversion= 0;
  plugin_dl->mysqlversion= *(int *)sym;
  /* Versioning */
  if (plugin_dl->mysqlversion < min_plugin_interface_version ||
      (plugin_dl->mysqlversion >> 8) > (MYSQL_PLUGIN_INTERFACE_VERSION >> 8))
  {
    free_plugin_mem(plugin_dl);
    report_error(report, ER_CANT_OPEN_LIBRARY, dlpath, 0,
                 "plugin interface version mismatch");
    DBUG_RETURN(TRUE);
  }
  /* Find plugin declarations */
  if (!(sym= dlsym(plugin_dl->handle, plugin_declarations_sym)))
  {
    free_plugin_mem(plugin_dl);
    report_error(report, ER_CANT_FIND_DL_ENTRY, plugin_declarations_sym);
    DBUG_RETURN(TRUE);
  }

  /* convert mysql declaration to maria one */
  {
    int i;
    uint sizeof_st_plugin;
    struct st_mysql_plugin *old;
    struct st_maria_plugin *cur;
    char *ptr= (char *)sym;

    if ((sym= dlsym(plugin_dl->handle, sizeof_st_plugin_sym)))
      sizeof_st_plugin= *(int *)sym;
    else
    {
      DBUG_ASSERT(min_plugin_interface_version == 0);
      sizeof_st_plugin= (int)offsetof(struct st_mysql_plugin, version);
    }

    for (i= 0;
         ((struct st_mysql_plugin *)(ptr + i * sizeof_st_plugin))->info;
         i++)
      /* no op */;

    cur= (struct st_maria_plugin*)
          my_malloc((i + 1) * sizeof(struct st_maria_plugin),
                    MYF(MY_ZEROFILL|MY_WME));
    if (!cur)
    {
      free_plugin_mem(plugin_dl);
      report_error(report, ER_OUTOFMEMORY,
                   static_cast<int>(plugin_dl->dl.length));
      DBUG_RETURN(TRUE);
    }
    /*
      All st_plugin fields not initialized in the plugin explicitly, are
      set to 0. It matches C standard behaviour for struct initializers that
      have less values than the struct definition.
    */
    for (i=0;
         (old= (struct st_mysql_plugin *)(ptr + i * sizeof_st_plugin))->info;
         i++)
    {

      cur[i].type= old->type;
      cur[i].info= old->info;
      cur[i].name= old->name;
      cur[i].author= old->author;
      cur[i].descr= old->descr;
      cur[i].license= old->license;
      cur[i].init= old->init;
      cur[i].deinit= old->deinit;
      cur[i].version= old->version;
      cur[i].status_vars= old->status_vars;
      cur[i].system_vars= old->system_vars;
      /*
        Something like this should be added to process
        new mysql plugin versions:
        if (plugin_dl->mysqlversion > 0x0101)
        {
           cur[i].newfield= CONSTANT_MEANS_UNKNOWN;
        }
        else
        {
           cur[i].newfield= old->newfield;
        }
      */
      /* Maria only fields */
      cur[i].version_info= "Unknown";
      cur[i].maturity= MariaDB_PLUGIN_MATURITY_UNKNOWN;
    }
    plugin_dl->allocated= true;
    plugin_dl->plugins= (struct st_maria_plugin *)cur;
  }

  DBUG_RETURN(FALSE);
}


/**
  Reads data from maria plugin interface

  @param plugin_dl       Structure where the data should be put
  @param sym             Reverence on version info
  @param dlpath          Path to the module
  @param report          what errors should be reported

  @retval FALSE OK
  @retval TRUE  ERROR
*/

static my_bool read_maria_plugin_info(struct st_plugin_dl *plugin_dl,
                                      void *sym, char *dlpath,
                                      int report)
{
  DBUG_ENTER("read_maria_plugin_info");

  /* Determine interface version */
  if (!(sym))
  {
    /*
      Actually this branch impossible because in case of absence of maria
      version we try mysql version.
    */
    free_plugin_mem(plugin_dl);
    report_error(report, ER_CANT_FIND_DL_ENTRY,
                 maria_plugin_interface_version_sym);
    DBUG_RETURN(TRUE);
  }
  plugin_dl->mariaversion= *(int *)sym;
  plugin_dl->mysqlversion= 0;
  /* Versioning */
  if (plugin_dl->mariaversion < min_maria_plugin_interface_version ||
      (plugin_dl->mariaversion >> 8) > (MARIA_PLUGIN_INTERFACE_VERSION >> 8))
  {
    free_plugin_mem(plugin_dl);
    report_error(report, ER_CANT_OPEN_LIBRARY, dlpath, ENOEXEC,
                 "plugin interface version mismatch");
    DBUG_RETURN(TRUE);
  }
  /* Find plugin declarations */
  if (!(sym= dlsym(plugin_dl->handle, maria_plugin_declarations_sym)))
  {
    free_plugin_mem(plugin_dl);
    report_error(report, ER_CANT_FIND_DL_ENTRY, maria_plugin_declarations_sym);
    DBUG_RETURN(TRUE);
  }
  if (plugin_dl->mariaversion != MARIA_PLUGIN_INTERFACE_VERSION)
  {
    uint sizeof_st_plugin;
    struct st_maria_plugin *old, *cur;
    char *ptr= (char *)sym;

    if ((sym= dlsym(plugin_dl->handle, maria_sizeof_st_plugin_sym)))
      sizeof_st_plugin= *(int *)sym;
    else
    {
      free_plugin_mem(plugin_dl);
      report_error(report, ER_CANT_FIND_DL_ENTRY, maria_sizeof_st_plugin_sym);
      DBUG_RETURN(TRUE);
    }

    if (sizeof_st_plugin != sizeof(st_mysql_plugin))
    {
      int i;
      for (i= 0;
           ((struct st_maria_plugin *)(ptr + i * sizeof_st_plugin))->info;
           i++)
        /* no op */;

      cur= (struct st_maria_plugin*)
        my_malloc((i + 1) * sizeof(struct st_maria_plugin),
                  MYF(MY_ZEROFILL|MY_WME));
      if (!cur)
      {
        free_plugin_mem(plugin_dl);
        report_error(report, ER_OUTOFMEMORY,
                     static_cast<int>(plugin_dl->dl.length));
        DBUG_RETURN(TRUE);
      }
      /*
        All st_plugin fields not initialized in the plugin explicitly, are
        set to 0. It matches C standard behaviour for struct initializers that
        have less values than the struct definition.
      */
      for (i=0;
           (old= (struct st_maria_plugin *)(ptr + i * sizeof_st_plugin))->info;
           i++)
        memcpy(cur + i, old, min(sizeof(cur[i]), sizeof_st_plugin));

      sym= cur;
      plugin_dl->allocated= true;
    }
    else
      sym= ptr;
  }
  plugin_dl->plugins= (struct st_maria_plugin *)sym;

  DBUG_RETURN(FALSE);
}
#endif /* HAVE_DLOPEN */

static st_plugin_dl *plugin_dl_add(const LEX_STRING *dl, int report)
{
#ifdef HAVE_DLOPEN
  char dlpath[FN_REFLEN];
  uint plugin_dir_len, dummy_errors, dlpathlen, i;
  struct st_plugin_dl *tmp, plugin_dl;
  void *sym;
  DBUG_ENTER("plugin_dl_add");
  DBUG_PRINT("enter", ("dl->str: '%s', dl->length: %d",
                       dl->str, (int) dl->length));
  plugin_dir_len= strlen(opt_plugin_dir);
  /*
    Ensure that the dll doesn't have a path.
    This is done to ensure that only approved libraries from the
    plugin directory are used (to make this even remotely secure).
  */
  if (check_valid_path(dl->str, dl->length) ||
      check_string_char_length((LEX_STRING *) dl, "", NAME_CHAR_LEN,
                               system_charset_info, 1) ||
      plugin_dir_len + dl->length + 1 >= FN_REFLEN)
  {
    report_error(report, ER_UDF_NO_PATHS);
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
  strxnmov(dlpath, sizeof(dlpath) - 1, opt_plugin_dir, "/", dl->str, NullS);
  (void) unpack_filename(dlpath, dlpath);
  plugin_dl.ref_count= 1;
  /* Open new dll handle */
  if (!(plugin_dl.handle= dlopen(dlpath, RTLD_NOW)))
  {
    const char *errmsg=dlerror();
    dlpathlen= strlen(dlpath);
    if (!strncmp(dlpath, errmsg, dlpathlen))
    { // if errmsg starts from dlpath, trim this prefix.
      errmsg+=dlpathlen;
      if (*errmsg == ':') errmsg++;
      if (*errmsg == ' ') errmsg++;
    }
    report_error(report, ER_CANT_OPEN_LIBRARY, dlpath, errno, errmsg);
    DBUG_RETURN(0);
  }

  /* Checks which plugin interface present and reads info */
  if (!(sym= dlsym(plugin_dl.handle, maria_plugin_interface_version_sym)))
  {
    if (read_mysql_plugin_info(&plugin_dl,
                               dlsym(plugin_dl.handle,
                                     plugin_interface_version_sym),
                               dlpath,
                               report))
      DBUG_RETURN(0);
  }
  else
  {
    if (read_maria_plugin_info(&plugin_dl, sym, dlpath, report))
      DBUG_RETURN(0);
  }

  /* link the services in */
  for (i= 0; i < array_elements(list_of_services); i++)
  {
    if ((sym= dlsym(plugin_dl.handle, list_of_services[i].name)))
    {
      uint ver= (uint)(intptr)*(void**)sym;
      if (ver > list_of_services[i].version ||
        (ver >> 8) < (list_of_services[i].version >> 8))
      {
        char buf[MYSQL_ERRMSG_SIZE];
        my_snprintf(buf, sizeof(buf),
                    "service '%s' interface version mismatch",
                    list_of_services[i].name);
        report_error(report, ER_CANT_OPEN_LIBRARY, dlpath, ENOEXEC, buf);
        DBUG_RETURN(0);
      }
      *(void**)sym= list_of_services[i].service;
    }
  }

  /* Duplicate and convert dll name */
  plugin_dl.dl.length= dl->length * files_charset_info->mbmaxlen + 1;
  if (! (plugin_dl.dl.str= (char*) my_malloc(plugin_dl.dl.length, MYF(0))))
  {
    free_plugin_mem(&plugin_dl);
    report_error(report, ER_OUTOFMEMORY,
                 static_cast<int>(plugin_dl.dl.length));
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
    report_error(report, ER_OUTOFMEMORY,
                 static_cast<int>(sizeof(struct st_plugin_dl)));
    DBUG_RETURN(0);
  }
  DBUG_RETURN(tmp);
#else
  DBUG_ENTER("plugin_dl_add");
  report_error(report, ER_FEATURE_DISABLED, "plugin", "HAVE_DLOPEN");
  DBUG_RETURN(0);
#endif
}


static void plugin_dl_del(const LEX_STRING *dl)
{
#ifdef HAVE_DLOPEN
  uint i;
  DBUG_ENTER("plugin_dl_del");

  mysql_mutex_assert_owner(&LOCK_plugin);

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

  mysql_mutex_assert_owner(&LOCK_plugin);

  if (type == MYSQL_ANY_PLUGIN)
  {
    for (i= 0; i < MYSQL_MAX_PLUGIN_TYPE_NUM; i++)
    {
      struct st_plugin_int *plugin= (st_plugin_int *)
        my_hash_search(&plugin_hash[i], (const uchar *)name->str, name->length);
      if (plugin)
        DBUG_RETURN(plugin);
    }
  }
  else
    DBUG_RETURN((st_plugin_int *)
        my_hash_search(&plugin_hash[type], (const uchar *)name->str,
                       name->length));
  DBUG_RETURN(0);
}


static SHOW_COMP_OPTION plugin_status(const LEX_STRING *name, int type)
{
  SHOW_COMP_OPTION rc= SHOW_OPTION_NO;
  struct st_plugin_int *plugin;
  DBUG_ENTER("plugin_is_ready");
  mysql_mutex_lock(&LOCK_plugin);
  if ((plugin= plugin_find_internal(name, type)))
  {
    rc= SHOW_OPTION_DISABLED;
    if (plugin->state == PLUGIN_IS_READY)
      rc= SHOW_OPTION_YES;
  }
  mysql_mutex_unlock(&LOCK_plugin);
  DBUG_RETURN(rc);
}


bool plugin_is_ready(const LEX_STRING *name, int type)
{
  bool rc= FALSE;
  if (plugin_status(name, type) == SHOW_OPTION_YES)
    rc= TRUE;
  return rc;
}


SHOW_COMP_OPTION plugin_status(const char *name, size_t len, int type)
{
  LEX_STRING plugin_name= { (char *) name, len };
  return plugin_status(&plugin_name, type);
}


static plugin_ref intern_plugin_lock(LEX *lex, plugin_ref rc)
{
  st_plugin_int *pi= plugin_ref_to_int(rc);
  DBUG_ENTER("intern_plugin_lock");

  mysql_mutex_assert_owner(&LOCK_plugin);

  if (pi->state & (PLUGIN_IS_READY | PLUGIN_IS_UNINITIALIZED))
  {
    plugin_ref plugin;
#ifdef DBUG_OFF
    /*
      In optimized builds we don't do reference counting for built-in
      (plugin->plugin_dl == 0) plugins.
    */
    if (!pi->plugin_dl)
      DBUG_RETURN(pi);

    plugin= pi;
#else
    /*
      For debugging, we do an additional malloc which allows the
      memory manager and/or valgrind to track locked references and
      double unlocks to aid resolving reference counting problems.
    */
    if (!(plugin= (plugin_ref) my_malloc(sizeof(pi), MYF(MY_WME))))
      DBUG_RETURN(NULL);

    *plugin= pi;
#endif
    pi->ref_count++;
    DBUG_PRINT("lock",("thd: 0x%lx  plugin: \"%s\" LOCK ref_count: %d",
                       (long) current_thd, pi->name.str, pi->ref_count));

    if (lex)
      insert_dynamic(&lex->plugins, (uchar*)&plugin);
    DBUG_RETURN(plugin);
  }
  DBUG_RETURN(NULL);
}


plugin_ref plugin_lock(THD *thd, plugin_ref ptr)
{
  LEX *lex= thd ? thd->lex : 0;
  plugin_ref rc;
  DBUG_ENTER("plugin_lock");

#ifdef DBUG_OFF
  /*
    In optimized builds we don't do reference counting for built-in
    (plugin->plugin_dl == 0) plugins.

    Note that we access plugin->plugin_dl outside of LOCK_plugin, and for
    dynamic plugins a 'plugin' could correspond to plugin that was unloaded
    meanwhile!  But because st_plugin_int is always allocated on
    plugin_mem_root, the pointer can never be invalid - the memory is never
    freed.
    Of course, the memory that 'plugin' points to can be overwritten by
    another plugin being loaded, but plugin->plugin_dl can never change
    from zero to non-zero or vice versa.
    That is, it's always safe to check for plugin->plugin_dl==0 even
    without a mutex.
  */
  if (! plugin_dlib(ptr))
  {
    plugin_ref_to_int(ptr)->locks_total++;
    DBUG_RETURN(ptr);
  }
#endif
  mysql_mutex_lock(&LOCK_plugin);
  plugin_ref_to_int(ptr)->locks_total++;
  rc= intern_plugin_lock(lex, ptr);
  mysql_mutex_unlock(&LOCK_plugin);
  DBUG_RETURN(rc);
}


plugin_ref plugin_lock_by_name(THD *thd, const LEX_STRING *name, int type)
{
  LEX *lex= thd ? thd->lex : 0;
  plugin_ref rc= NULL;
  st_plugin_int *plugin;
  DBUG_ENTER("plugin_lock_by_name");
  mysql_mutex_lock(&LOCK_plugin);
  if ((plugin= plugin_find_internal(name, type)))
    rc= intern_plugin_lock(lex, plugin_int_to_ref(plugin));
  mysql_mutex_unlock(&LOCK_plugin);
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
                       const LEX_STRING *name, LEX_STRING *dl, int report)
{
  struct st_plugin_int tmp;
  struct st_maria_plugin *plugin;
  uint oks= 0, errs= 0;
  DBUG_ENTER("plugin_add");
  if (name->str && plugin_find_internal(name, MYSQL_ANY_PLUGIN))
  {
    report_error(report, ER_UDF_EXISTS, name->str);
    DBUG_RETURN(TRUE);
  }
  /* Clear the whole struct to catch future extensions. */
  bzero((char*) &tmp, sizeof(tmp));
  fix_dl_name(tmp_root, dl);
  if (! (tmp.plugin_dl= plugin_dl_add(dl, report)))
    DBUG_RETURN(TRUE);
  /* Find plugin by name */
  for (plugin= tmp.plugin_dl->plugins; plugin->info; plugin++)
  {
    tmp.name.str= (char *)plugin->name;
    tmp.name.length= strlen(plugin->name);

    if (plugin->type < 0 || plugin->type >= MYSQL_MAX_PLUGIN_TYPE_NUM)
      continue; // invalid plugin

    if (name->str && my_strnncoll(system_charset_info,
                                  (const uchar *)name->str, name->length,
                                  (const uchar *)tmp.name.str, tmp.name.length))
      continue; // plugin name doesn't match

    if (!name->str && plugin_find_internal(&tmp.name, MYSQL_ANY_PLUGIN))
      continue; // already installed

      struct st_plugin_int *tmp_plugin_ptr;
      if (*(int*)plugin->info <
          min_plugin_info_interface_version[plugin->type] ||
          ((*(int*)plugin->info) >> 8) >
          (cur_plugin_info_interface_version[plugin->type] >> 8))
      {
        char buf[256];
        strxnmov(buf, sizeof(buf) - 1, "API version for ",
                 plugin_type_names[plugin->type].str,
                 " plugin ", tmp.name.str,
                 " not supported by this version of the server", NullS);
        report_error(report, ER_CANT_OPEN_LIBRARY, dl->str, ENOEXEC, buf);
        goto err;
      }
      if (plugin_maturity_map[plugin->maturity] < plugin_maturity)
      {
        char buf[256];
        strxnmov(buf, sizeof(buf) - 1, "Loading of ",
                 plugin_maturity_names[plugin->maturity],
                 " plugin ", tmp.name.str,
                 " is prohibited by --plugin-maturity=",
                 plugin_maturity_names[plugin_maturity],
                 NullS);
        report_error(report, ER_CANT_OPEN_LIBRARY, dl->str, EPERM, buf);
        goto err;
      }
      tmp.plugin= plugin;
      tmp.ref_count= 0;
      tmp.state= PLUGIN_IS_UNINITIALIZED;
      tmp.load_option= PLUGIN_ON;

      if (!(tmp_plugin_ptr= plugin_insert_or_reuse(&tmp)))
        goto err;
      plugin_array_version++;
      if (my_hash_insert(&plugin_hash[plugin->type], (uchar*)tmp_plugin_ptr))
        tmp_plugin_ptr->state= PLUGIN_IS_FREED;
      init_alloc_root(&tmp_plugin_ptr->mem_root, 4096, 4096);

    if (name->str)
      DBUG_RETURN(FALSE); // all done

    oks++;
    tmp.plugin_dl->ref_count++;
    continue; // otherwise - go on

err:
    errs++;
    if (name->str)
      break;
  }

  if (errs == 0 && oks == 0) // no plugin was found
    report_error(report, ER_CANT_FIND_DL_ENTRY, name->str);

  plugin_dl_del(dl);
  DBUG_RETURN(errs > 0 || oks == 0);
}


static void plugin_deinitialize(struct st_plugin_int *plugin, bool ref_check)
{
  /*
    we don't want to hold the LOCK_plugin mutex as it may cause
    deinitialization to deadlock if plugins have worker threads
    with plugin locks
  */
  mysql_mutex_assert_not_owner(&LOCK_plugin);

  if (plugin->plugin->status_vars)
  {
#ifdef FIX_LATER
    /**
      @todo
      unfortunately, status variables were introduced without a
      pluginname_ namespace, that is pluginname_ was not added automatically
      to status variable names. It should be fixed together with the next
      incompatible API change.
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

  /* maintain the obsolete @@have_innodb variable */
  if (!my_strcasecmp(&my_charset_latin1, plugin->name.str, "InnoDB"))
    have_innodb= SHOW_OPTION_DISABLED;

  /*
    We do the check here because NDB has a worker THD which doesn't
    exit until NDB is shut down.
  */
  if (ref_check && plugin->ref_count)
    sql_print_error("Plugin '%s' has ref_count=%d after deinitialization.",
                    plugin->name.str, plugin->ref_count);

  restore_pluginvar_names(plugin->system_vars);
}

static void plugin_del(struct st_plugin_int *plugin)
{
  DBUG_ENTER("plugin_del");
  mysql_mutex_assert_owner(&LOCK_plugin);
  /* Free allocated strings before deleting the plugin. */
  plugin_vars_free_values(plugin->system_vars);
  my_hash_delete(&plugin_hash[plugin->plugin->type], (uchar*)plugin);
  if (plugin->plugin_dl)
    plugin_dl_del(&plugin->plugin_dl->dl);
  plugin->state= PLUGIN_IS_FREED;
  plugin_array_version++;
  free_root(&plugin->mem_root, MYF(0));
  DBUG_VOID_RETURN;
}

static void reap_plugins(void)
{
  uint count, idx;
  struct st_plugin_int *plugin, **reap, **list;

  mysql_mutex_assert_owner(&LOCK_plugin);

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

  mysql_mutex_unlock(&LOCK_plugin);

  list= reap;
  while ((plugin= *(--list)))
    plugin_deinitialize(plugin, true);

  mysql_mutex_lock(&LOCK_plugin);

  while ((plugin= *(--reap)))
    plugin_del(plugin);

  my_afree(reap);
}

static void intern_plugin_unlock(LEX *lex, plugin_ref plugin)
{
  int i;
  st_plugin_int *pi;
  DBUG_ENTER("intern_plugin_unlock");

  mysql_mutex_assert_owner(&LOCK_plugin);

  if (!plugin)
    DBUG_VOID_RETURN;

  pi= plugin_ref_to_int(plugin);

#ifdef DBUG_OFF
  if (!pi->plugin_dl)
    DBUG_VOID_RETURN;
#else
  my_free(plugin);
#endif

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

  DBUG_PRINT("lock",("thd: 0x%lx  plugin: \"%s\" UNLOCK ref_count: %d",
                     (long) current_thd, pi->name.str, pi->ref_count));

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
  mysql_mutex_lock(&LOCK_plugin);
  intern_plugin_unlock(lex, plugin);
  reap_plugins();
  mysql_mutex_unlock(&LOCK_plugin);
  DBUG_VOID_RETURN;
}


void plugin_unlock_list(THD *thd, plugin_ref *list, uint count)
{
  LEX *lex= thd ? thd->lex : 0;
  DBUG_ENTER("plugin_unlock_list");
  if (count == 0)
    DBUG_VOID_RETURN;

  DBUG_ASSERT(list);
  mysql_mutex_lock(&LOCK_plugin);
  while (count--)
    intern_plugin_unlock(lex, *list++);
  reap_plugins();
  mysql_mutex_unlock(&LOCK_plugin);
  DBUG_VOID_RETURN;
}


static int plugin_initialize(MEM_ROOT *tmp_root, struct st_plugin_int *plugin,
                             int *argc, char **argv, bool options_only)
{
  int ret= 1;
  DBUG_ENTER("plugin_initialize");

  mysql_mutex_assert_owner(&LOCK_plugin);
  uint state= plugin->state;
  DBUG_ASSERT(state == PLUGIN_IS_UNINITIALIZED);

  mysql_mutex_unlock(&LOCK_plugin);

  mysql_rwlock_wrlock(&LOCK_system_variables_hash);
  if (test_plugin_options(tmp_root, plugin, argc, argv))
    state= PLUGIN_IS_DISABLED;
  mysql_rwlock_unlock(&LOCK_system_variables_hash);

  if (options_only || state == PLUGIN_IS_DISABLED)
  {
    ret= 0;
    goto err;
  }

  if (plugin->plugin_dl && global_system_variables.log_warnings >= 9)
  {
    void *sym= dlsym(plugin->plugin_dl->handle,
                     plugin->plugin_dl->mariaversion ?
                       maria_plugin_declarations_sym : plugin_declarations_sym);
    DBUG_ASSERT(sym);
    sql_print_information("Plugin %s loaded at %p",
                          plugin->name.str, sym);
  }

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
  state= PLUGIN_IS_READY; // plugin->init() succeeded

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
    if (add_status_vars(plugin->plugin->status_vars))
      goto err;
#endif /* FIX_LATER */
  }

  ret= 0;

err:
  if (ret)
    restore_pluginvar_names(plugin->system_vars);

  mysql_mutex_lock(&LOCK_plugin);
  plugin->state= state;

  /* maintain the obsolete @@have_innodb variable */
  if (!my_strcasecmp(&my_charset_latin1, plugin->name.str, "InnoDB"))
    have_innodb= state & PLUGIN_IS_READY ? SHOW_OPTION_YES
                                         : SHOW_OPTION_DISABLED;

  DBUG_RETURN(ret);
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

static inline void convert_dash_to_underscore(char *str, int len)
{
  for (char *p= str; p <= str+len; p++)
    if (*p == '-')
      *p= '_';
}

static inline void convert_underscore_to_dash(char *str, int len)
{
  for (char *p= str; p <= str+len; p++)
    if (*p == '_')
      *p= '-';
}

#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_key key_LOCK_plugin;

static PSI_mutex_info all_plugin_mutexes[]=
{
  { &key_LOCK_plugin, "LOCK_plugin", PSI_FLAG_GLOBAL}
};

static void init_plugin_psi_keys(void)
{
  const char* category= "sql";
  int count;

  if (PSI_server == NULL)
    return;

  count= array_elements(all_plugin_mutexes);
  PSI_server->register_mutex(category, all_plugin_mutexes, count);
}
#endif /* HAVE_PSI_INTERFACE */

/*
  The logic is that we first load and initialize all compiled in plugins.
  From there we load up the dynamic types (assuming we have not been told to
  skip this part).

  Finally we initialize everything, aka the dynamic that have yet to initialize.
*/
int plugin_init(int *argc, char **argv, int flags)
{
  uint i;
  bool is_myisam;
  struct st_maria_plugin **builtins;
  struct st_maria_plugin *plugin;
  struct st_plugin_int tmp, *plugin_ptr, **reap;
  MEM_ROOT tmp_root;
  bool reaped_mandatory_plugin= false;
  bool mandatory= true;
  DBUG_ENTER("plugin_init");

  if (initialized)
    DBUG_RETURN(0);

#ifdef HAVE_PSI_INTERFACE
  init_plugin_psi_keys();
#endif

  init_alloc_root(&plugin_mem_root, 4096, 4096);
  init_alloc_root(&plugin_vars_mem_root, 4096, 4096);
  init_alloc_root(&tmp_root, 4096, 4096);

  if (my_hash_init(&bookmark_hash, &my_charset_bin, 16, 0, 0,
                   get_bookmark_hash_key, NULL, HASH_UNIQUE))
      goto err;


  mysql_mutex_init(key_LOCK_plugin, &LOCK_plugin, MY_MUTEX_INIT_FAST);

  if (my_init_dynamic_array(&plugin_dl_array,
                            sizeof(struct st_plugin_dl *),16,16) ||
      my_init_dynamic_array(&plugin_array,
                            sizeof(struct st_plugin_int *),16,16))
    goto err;

  for (i= 0; i < MYSQL_MAX_PLUGIN_TYPE_NUM; i++)
  {
    if (my_hash_init(&plugin_hash[i], system_charset_info, 16, 0, 0,
                     get_plugin_hash_key, NULL, HASH_UNIQUE))
      goto err;
  }

  /* prepare debug_sync service */
  DBUG_ASSERT(strcmp(list_of_services[5].name, "debug_sync_service") == 0);
  list_of_services[5].service= *(void**)&debug_sync_C_callback_ptr;

  mysql_mutex_lock(&LOCK_plugin);

  initialized= 1;

  /*
    First we register builtin plugins
  */
  for (builtins= mysql_mandatory_plugins; *builtins || mandatory; builtins++)
  {
    if (!*builtins)
    {
      builtins= mysql_optional_plugins;
      mandatory= false;
      if (!*builtins)
        break;
    }
    for (plugin= *builtins; plugin->info; plugin++)
    {
      if (opt_ignore_builtin_innodb &&
          !my_strnncoll(&my_charset_latin1, (const uchar*) plugin->name,
                        6, (const uchar*) "InnoDB", 6))
        continue;

      bzero(&tmp, sizeof(tmp));
      tmp.plugin= plugin;
      tmp.name.str= (char *)plugin->name;
      tmp.name.length= strlen(plugin->name);
      tmp.state= 0;
      tmp.load_option= mandatory ? PLUGIN_FORCE : PLUGIN_ON;

      for (i=0; i < array_elements(override_plugin_load_policy); i++)
      {
        if (!my_strcasecmp(&my_charset_latin1, plugin->name,
                           override_plugin_load_policy[i].plugin_name))
        {
          tmp.load_option= override_plugin_load_policy[i].override;
          break;
        }
      }

      free_root(&tmp_root, MYF(MY_MARK_BLOCKS_FREE));
      tmp.state= PLUGIN_IS_UNINITIALIZED;
      if (register_builtin(plugin, &tmp, &plugin_ptr))
        goto err_unlock;

      is_myisam= !my_strcasecmp(&my_charset_latin1, plugin->name, "MyISAM");

      /*
        strictly speaking, we should to initialize all plugins,
        even for mysqld --help, because important subsystems
        may be disabled otherwise, and the help will be incomplete.
        For example, if the mysql.plugin table is not MyISAM.
        But for now it's an unlikely corner case, and to optimize
        mysqld --help for all other users, we will only initialize
        MyISAM here.
      */
      if (plugin_initialize(&tmp_root, plugin_ptr, argc, argv, !is_myisam &&
                            (flags & PLUGIN_INIT_SKIP_INITIALIZATION)))
      {
        if (plugin_ptr->load_option == PLUGIN_FORCE)
          goto err_unlock;
        plugin_ptr->state= PLUGIN_IS_DISABLED;
      }

      /*
        initialize the global default storage engine so that it may
        not be null in any child thread.
      */
      if (is_myisam)
      {
        DBUG_ASSERT(!global_system_variables.table_plugin);
        global_system_variables.table_plugin=
          intern_plugin_lock(NULL, plugin_int_to_ref(plugin_ptr));
        DBUG_ASSERT(plugin_ptr->ref_count == 1);
      }
    }
  }

  /* should now be set to MyISAM storage engine */
  DBUG_ASSERT(global_system_variables.table_plugin);

  mysql_mutex_unlock(&LOCK_plugin);

  /* Register all dynamic plugins */
  if (!(flags & PLUGIN_INIT_SKIP_DYNAMIC_LOADING))
  {
    if (opt_plugin_load)
      plugin_load_list(&tmp_root, opt_plugin_load);
    if (!(flags & PLUGIN_INIT_SKIP_PLUGIN_TABLE))
      plugin_load(&tmp_root);
  }

  /*
    Now we initialize all remaining plugins
  */

  mysql_mutex_lock(&LOCK_plugin);
  reap= (st_plugin_int **) my_alloca((plugin_array.elements+1) * sizeof(void*));
  *(reap++)= NULL;

  for (i= 0; i < plugin_array.elements; i++)
  {
    plugin_ptr= *dynamic_element(&plugin_array, i, struct st_plugin_int **);
    if (plugin_ptr->plugin_dl && plugin_ptr->state == PLUGIN_IS_UNINITIALIZED)
    {
      if (plugin_initialize(&tmp_root, plugin_ptr, argc, argv,
                            (flags & PLUGIN_INIT_SKIP_INITIALIZATION)))
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
    mysql_mutex_unlock(&LOCK_plugin);
    if (plugin_ptr->load_option == PLUGIN_FORCE ||
        plugin_ptr->load_option == PLUGIN_FORCE_PLUS_PERMANENT)
      reaped_mandatory_plugin= TRUE;
    plugin_deinitialize(plugin_ptr, true);
    mysql_mutex_lock(&LOCK_plugin);
    plugin_del(plugin_ptr);
  }

  mysql_mutex_unlock(&LOCK_plugin);
  my_afree(reap);
  if (reaped_mandatory_plugin)
    goto err;

  free_root(&tmp_root, MYF(0));

  DBUG_RETURN(0);

err_unlock:
  mysql_mutex_unlock(&LOCK_plugin);
err:
  free_root(&tmp_root, MYF(0));
  DBUG_RETURN(1);
}


static bool register_builtin(struct st_maria_plugin *plugin,
                             struct st_plugin_int *tmp,
                             struct st_plugin_int **ptr)
{
  DBUG_ENTER("register_builtin");
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


/*
  called only by plugin_init()
*/
static void plugin_load(MEM_ROOT *tmp_root)
{
  THD thd;
  TABLE_LIST tables;
  TABLE *table;
  READ_RECORD read_record_info;
  int error;
  THD *new_thd= &thd;
  bool result;
#ifdef EMBEDDED_LIBRARY
  No_such_table_error_handler error_handler;
#endif /* EMBEDDED_LIBRARY */
  DBUG_ENTER("plugin_load");

  new_thd->thread_stack= (char*) &tables;
  new_thd->store_globals();
  new_thd->db= my_strdup("mysql", MYF(0));
  new_thd->db_length= 5;
  bzero((char*) &thd.net, sizeof(thd.net));
  tables.init_one_table("mysql", 5, "plugin", 6, "plugin", TL_READ);

#ifdef EMBEDDED_LIBRARY
  /*
    When building an embedded library, if the mysql.plugin table
    does not exist, we silently ignore the missing table
  */
  new_thd->push_internal_handler(&error_handler);
#endif /* EMBEDDED_LIBRARY */

  result= open_and_lock_tables(new_thd, &tables, FALSE, MYSQL_LOCK_IGNORE_TIMEOUT);

#ifdef EMBEDDED_LIBRARY
  new_thd->pop_internal_handler();
  if (error_handler.safely_trapped_errors())
    goto end;
#endif /* EMBEDDED_LIBRARY */

  if (result)
  {
    DBUG_PRINT("error",("Can't open plugin table"));
    if (!opt_help)
      sql_print_error("Can't open the mysql.plugin table. Please "
                      "run mysql_upgrade to create it.");
    else
      sql_print_warning("Could not open mysql.plugin table. Some options may be missing from the help text");
    goto end;
  }
  table= tables.table;
  if (init_read_record(&read_record_info, new_thd, table, NULL, 1, 0, FALSE))
  {
    sql_print_error("Could not initialize init_read_record; Plugins not "
                    "loaded");
    goto end;
  }
  table->use_all_columns();
  while (!(error= read_record_info.read_record(&read_record_info)))
  {
    DBUG_PRINT("info", ("init plugin record"));
    String str_name, str_dl;
    get_field(tmp_root, table->field[0], &str_name);
    get_field(tmp_root, table->field[1], &str_dl);

    LEX_STRING name= {(char *)str_name.ptr(), str_name.length()};
    LEX_STRING dl= {(char *)str_dl.ptr(), str_dl.length()};

    /*
      there're no other threads running yet, so we don't need a mutex.
      but plugin_add() before is designed to work in multi-threaded
      environment, and it uses mysql_mutex_assert_owner(), so we lock
      the mutex here to satisfy the assert
    */
    mysql_mutex_lock(&LOCK_plugin);
    if (plugin_add(tmp_root, &name, &dl, REPORT_TO_LOG))
      sql_print_warning("Couldn't load plugin named '%s' with soname '%s'.",
                        str_name.c_ptr(), str_dl.c_ptr());
    free_root(tmp_root, MYF(MY_MARK_BLOCKS_FREE));
    mysql_mutex_unlock(&LOCK_plugin);
  }
  if (error > 0)
    sql_print_error(ER(ER_GET_ERRNO), my_errno);
  end_read_record(&read_record_info);
  table->m_needs_reopen= TRUE;                  // Force close to free memory
  close_mysql_tables(new_thd);
end:
  /* Remember that we don't have a THD */
  my_pthread_setspecific_ptr(THR_THD, 0);
  DBUG_VOID_RETURN;
}


/*
  called only by plugin_init()
*/
static bool plugin_load_list(MEM_ROOT *tmp_root, const char *list)
{
  char buffer[FN_REFLEN];
  LEX_STRING name= {buffer, 0}, dl= {NULL, 0}, *str= &name;
  char *p= buffer;
  DBUG_ENTER("plugin_load_list");
  while (list)
  {
    if (p == buffer + sizeof(buffer) - 1)
    {
      sql_print_error("plugin-load parameter too long");
      DBUG_RETURN(TRUE);
    }

    switch ((*(p++)= *(list++))) {
    case '\0':
      list= NULL; /* terminate the loop */
      /* fall through */
#ifndef __WIN__
    case ':':     /* can't use this as delimiter as it may be drive letter */
#endif
    case ';':
      str->str[str->length]= '\0';
      if (str == &name)  // load all plugins in named module
      {
        if (!name.length)
        {
          p--;    /* reset pointer */
          continue;
        }

        dl= name;
        mysql_mutex_lock(&LOCK_plugin);
        free_root(tmp_root, MYF(MY_MARK_BLOCKS_FREE));
        name.str= 0; // load everything
        if (plugin_add(tmp_root, &name, &dl, REPORT_TO_LOG))
          goto error;
      }
      else
      {
        free_root(tmp_root, MYF(MY_MARK_BLOCKS_FREE));
        mysql_mutex_lock(&LOCK_plugin);
        if (plugin_add(tmp_root, &name, &dl, REPORT_TO_LOG))
          goto error;
      }
      mysql_mutex_unlock(&LOCK_plugin);
      name.length= dl.length= 0;
      dl.str= NULL; name.str= p= buffer;
      str= &name;
      continue;
    case '=':
    case '#':
      if (str == &name)
      {
        name.str[name.length]= '\0';
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
  mysql_mutex_unlock(&LOCK_plugin);
  if (name.str)
    sql_print_error("Couldn't load plugin '%s' from '%s'.",
                    name.str, dl.str);
  else
    sql_print_error("Couldn't load plugins from '%s'.", dl.str);
  DBUG_RETURN(TRUE);
}


void plugin_shutdown(void)
{
  uint i, count= plugin_array.elements;
  struct st_plugin_int **plugins, *plugin;
  struct st_plugin_dl **dl;
  DBUG_ENTER("plugin_shutdown");

  if (initialized)
  {
    mysql_mutex_lock(&LOCK_plugin);

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
      for (i= 0; i < count; i++)
      {
        plugin= *dynamic_element(&plugin_array, i, struct st_plugin_int **);
        if (plugin->state == PLUGIN_IS_READY)
        {
          plugin->state= PLUGIN_IS_DELETED;
          reap_needed= true;
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
    mysql_mutex_unlock(&LOCK_plugin);

    /*
      We loop through all plugins and call deinit() if they have one.
    */
    for (i= 0; i < count; i++)
      if (!(plugins[i]->state & (PLUGIN_IS_UNINITIALIZED | PLUGIN_IS_FREED |
                                 PLUGIN_IS_DISABLED)))
      {
        sql_print_warning("Plugin '%s' will be forced to shutdown",
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
      use mysql_mutex_assert_owner(), so we lock the mutex to satisfy it
    */
    mysql_mutex_lock(&LOCK_plugin);

    /*
      We defer checking ref_counts until after all plugins are deinitialized
      as some may have worker threads holding on to plugin references.
    */
    for (i= 0; i < count; i++)
    {
      if (plugins[i]->ref_count)
        sql_print_error("Plugin '%s' has ref_count=%d after shutdown.",
                        plugins[i]->name.str, plugins[i]->ref_count);
      if (plugins[i]->state & PLUGIN_IS_UNINITIALIZED ||
          plugins[i]->state & PLUGIN_IS_DISABLED)
        plugin_del(plugins[i]);
    }

    /*
      Now we can deallocate all memory.
    */

    cleanup_variables(NULL, &global_system_variables);
    cleanup_variables(NULL, &max_system_variables);
    mysql_mutex_unlock(&LOCK_plugin);

    initialized= 0;
    mysql_mutex_destroy(&LOCK_plugin);

    my_afree(plugins);
  }

  /* Dispose of the memory */

  for (i= 0; i < MYSQL_MAX_PLUGIN_TYPE_NUM; i++)
    my_hash_free(&plugin_hash[i]);
  delete_dynamic(&plugin_array);

  count= plugin_dl_array.elements;
  dl= (struct st_plugin_dl **)my_alloca(sizeof(void*) * count);
  for (i= 0; i < count; i++)
    dl[i]= *dynamic_element(&plugin_dl_array, i, struct st_plugin_dl **);
  for (i= 0; i < plugin_dl_array.elements; i++)
    free_plugin_mem(dl[i]);
  my_afree(dl);
  delete_dynamic(&plugin_dl_array);

  my_hash_free(&bookmark_hash);
  free_root(&plugin_mem_root, MYF(0));
  free_root(&plugin_vars_mem_root, MYF(0));

  global_variables_dynamic_size= 0;

  DBUG_VOID_RETURN;
}

/**
  complete plugin installation (after plugin_add).

  That is, initialize it, and update mysql.plugin table
*/
static bool finalize_install(THD *thd, TABLE *table, const LEX_STRING *name,
                             int *argc, char **argv)
{
  struct st_plugin_int *tmp= plugin_find_internal(name, MYSQL_ANY_PLUGIN);
  int error;
  DBUG_ASSERT(tmp);
  mysql_mutex_assert_owner(&LOCK_plugin); // because of tmp->state

  if (tmp->state != PLUGIN_IS_UNINITIALIZED)
  {
    /* already installed */
    return 0;
  }
  else
  {
    if (plugin_initialize(thd->mem_root, tmp, argc, argv, false))
    {
      report_error(REPORT_TO_USER, ER_CANT_INITIALIZE_UDF, name->str,
                   "Plugin initialization function failed.");
      tmp->state= PLUGIN_IS_DELETED;
      return 1;
    }
  }
  if (tmp->state == PLUGIN_IS_DISABLED)
  {
    if (global_system_variables.log_warnings)
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                          ER_CANT_INITIALIZE_UDF, ER(ER_CANT_INITIALIZE_UDF),
                          name->str, "Plugin is disabled");
  }

  /*
    We do not replicate the INSTALL PLUGIN statement. Disable binlogging
    of the insert into the plugin table, so that it is not replicated in
    row based mode.
  */
  tmp_disable_binlog(thd);
  table->use_all_columns();
  restore_record(table, s->default_values);
  table->field[0]->store(name->str, name->length, system_charset_info);
  table->field[1]->store(tmp->plugin_dl->dl.str, tmp->plugin_dl->dl.length,
                         files_charset_info);
  error= table->file->ha_write_row(table->record[0]);
  reenable_binlog(thd);
  if (error)
  {
    table->file->print_error(error, MYF(0));
    tmp->state= PLUGIN_IS_DELETED;
    return 1;
  }
  return 0;
}

bool mysql_install_plugin(THD *thd, const LEX_STRING *name,
                          const LEX_STRING *dl_arg)
{
  TABLE_LIST tables;
  TABLE *table;
  LEX_STRING dl= *dl_arg;
  bool error;
  int argc=orig_argc;
  char **argv=orig_argv;
  DBUG_ENTER("mysql_install_plugin");

  if (opt_noacl)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    DBUG_RETURN(TRUE);
  }

  tables.init_one_table("mysql", 5, "plugin", 6, "plugin", TL_WRITE);
  if (check_table_access(thd, INSERT_ACL, &tables, FALSE, 1, FALSE))
    DBUG_RETURN(TRUE);

  /* need to open before acquiring LOCK_plugin or it will deadlock */
  if (! (table = open_ltable(thd, &tables, TL_WRITE,
                             MYSQL_LOCK_IGNORE_TIMEOUT)))
    DBUG_RETURN(TRUE);

  if (my_load_defaults(MYSQL_CONFIG_NAME, load_default_groups, &argc, &argv, NULL))
  {
    report_error(REPORT_TO_USER, ER_PLUGIN_IS_NOT_LOADED, name->str);
    DBUG_RETURN(TRUE);
  }

  /*
    Pre-acquire audit plugins for events that may potentially occur
    during [UN]INSTALL PLUGIN.

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

    See also mysql_uninstall_plugin() and initialize_audit_plugin()
  */
  unsigned long event_class_mask[MYSQL_AUDIT_CLASS_MASK_SIZE] =
  { MYSQL_AUDIT_GENERAL_CLASSMASK };
  mysql_audit_acquire_plugins(thd, event_class_mask);

  mysql_mutex_lock(&LOCK_plugin);
  error= plugin_add(thd->mem_root, name, &dl, REPORT_TO_USER);
  if (error)
    goto err;

  if (name->str)
    error= finalize_install(thd, table, name, &argc, argv);
  else
  {
    st_plugin_dl *plugin_dl= plugin_dl_find(&dl);
    struct st_maria_plugin *plugin;
    for (plugin= plugin_dl->plugins; plugin->info; plugin++)
    {
      LEX_STRING str= { const_cast<char*>(plugin->name), strlen(plugin->name) };
      error|= finalize_install(thd, table, &str, &argc, argv);
    }
  }

  if (error)
  {
    reap_needed= true;
    reap_plugins();
  }
err:
  mysql_mutex_unlock(&LOCK_plugin);
  if (argv)
    free_defaults(argv);
  DBUG_RETURN(error);
}


static bool do_uninstall(THD *thd, TABLE *table, const LEX_STRING *name)
{
  struct st_plugin_int *plugin;
  mysql_mutex_assert_owner(&LOCK_plugin);

  if (!(plugin= plugin_find_internal(name, MYSQL_ANY_PLUGIN)) ||
      plugin->state & (PLUGIN_IS_UNINITIALIZED | PLUGIN_IS_DYING))
  {
    my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "PLUGIN", name->str);
    return 1;
  }
  if (!plugin->plugin_dl)
  {
    my_error(ER_PLUGIN_DELETE_BUILTIN, MYF(0));
    return 1;
  }
  if (plugin->load_option == PLUGIN_FORCE_PLUS_PERMANENT)
  {
    my_error(ER_PLUGIN_IS_PERMANENT, MYF(0), name->str);
    return 1;
  }

  plugin->state= PLUGIN_IS_DELETED;
  if (plugin->ref_count)
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                 WARN_PLUGIN_BUSY, ER(WARN_PLUGIN_BUSY));
  else
    reap_needed= true;

  uchar user_key[MAX_KEY_LENGTH];
  table->use_all_columns();
  table->field[0]->store(name->str, name->length, system_charset_info);
  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);
  if (! table->file->ha_index_read_idx_map(table->record[0], 0, user_key,
                                           HA_WHOLE_KEY, HA_READ_KEY_EXACT))
  {
    int error;
    /*
      We do not replicate the UNINSTALL PLUGIN statement. Disable binlogging
      of the delete from the plugin table, so that it is not replicated in
      row based mode.
    */
    tmp_disable_binlog(thd);
    error= table->file->ha_delete_row(table->record[0]);
    reenable_binlog(thd);
    if (error)
    {
      table->file->print_error(error, MYF(0));
      return 1;
    }
  }
  return 0;
}


bool mysql_uninstall_plugin(THD *thd, const LEX_STRING *name,
                            const LEX_STRING *dl_arg)
{
  TABLE *table;
  TABLE_LIST tables;
  LEX_STRING dl= *dl_arg;
  bool error= false;
  DBUG_ENTER("mysql_uninstall_plugin");

  if (opt_noacl)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    DBUG_RETURN(TRUE);
  }

  tables.init_one_table("mysql", 5, "plugin", 6, "plugin", TL_WRITE);

  if (check_table_access(thd, DELETE_ACL, &tables, FALSE, 1, FALSE))
    DBUG_RETURN(TRUE);

  /* need to open before acquiring LOCK_plugin or it will deadlock */
  if (! (table= open_ltable(thd, &tables, TL_WRITE, MYSQL_LOCK_IGNORE_TIMEOUT)))
    DBUG_RETURN(TRUE);

  /*
    Pre-acquire audit plugins for events that may potentially occur
    during [UN]INSTALL PLUGIN.

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

    See also mysql_install_plugin() and initialize_audit_plugin()
  */
  unsigned long event_class_mask[MYSQL_AUDIT_CLASS_MASK_SIZE] =
  { MYSQL_AUDIT_GENERAL_CLASSMASK };
  mysql_audit_acquire_plugins(thd, event_class_mask);

  mysql_mutex_lock(&LOCK_plugin);

  if (name->str)
    error= do_uninstall(thd, table, name);
  else
  {
    fix_dl_name(thd->mem_root, &dl);
    st_plugin_dl *plugin_dl= plugin_dl_find(&dl);
    if (plugin_dl)
    {
      for (struct st_maria_plugin *plugin= plugin_dl->plugins;
           plugin->info; plugin++)
      {
        LEX_STRING str= { const_cast<char*>(plugin->name), strlen(plugin->name) };
        error|= do_uninstall(thd, table, &str);
      }
    }
    else
    {
      my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "SONAME", dl.str);
      error= true;
    }
  }
  reap_plugins();

  mysql_mutex_unlock(&LOCK_plugin);
  DBUG_RETURN(error);
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

  mysql_mutex_lock(&LOCK_plugin);
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
      plugin= (struct st_plugin_int *) my_hash_element(hash, idx);
      plugins[idx]= !(plugin->state & state_mask) ? plugin : NULL;
    }
  }
  mysql_mutex_unlock(&LOCK_plugin);

  for (idx= 0; idx < total; idx++)
  {
    if (unlikely(version != plugin_array_version))
    {
      mysql_mutex_lock(&LOCK_plugin);
      for (uint i=idx; i < total; i++)
        if (plugins[i] && plugins[i]->state & state_mask)
          plugins[i]=0;
      mysql_mutex_unlock(&LOCK_plugin);
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
#define PLUGIN_VAR_TYPEMASK 0x7f
#define BOOKMARK_MEMALLOC   0x80

static inline char plugin_var_bookmark_key(uint flags)
{
  return (flags & PLUGIN_VAR_TYPEMASK) |
         (flags & PLUGIN_VAR_MEMALLOC ? BOOKMARK_MEMALLOC : 0);
}

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
typedef DECLARE_MYSQL_SYSVAR_SIMPLE(sysvar_double_t, double);

typedef DECLARE_MYSQL_THDVAR_SIMPLE(thdvar_int_t, int);
typedef DECLARE_MYSQL_THDVAR_SIMPLE(thdvar_long_t, long);
typedef DECLARE_MYSQL_THDVAR_SIMPLE(thdvar_longlong_t, longlong);
typedef DECLARE_MYSQL_THDVAR_SIMPLE(thdvar_uint_t, uint);
typedef DECLARE_MYSQL_THDVAR_SIMPLE(thdvar_ulong_t, ulong);
typedef DECLARE_MYSQL_THDVAR_SIMPLE(thdvar_ulonglong_t, ulonglong);
typedef DECLARE_MYSQL_THDVAR_SIMPLE(thdvar_double_t, double);


/****************************************************************************
  default variable data check and update functions
****************************************************************************/

static int check_func_bool(THD *thd, struct st_mysql_sys_var *var,
                           void *save, st_mysql_value *value)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *str;
  int result, length;
  long long tmp;

  if (value->value_type(value) == MYSQL_VALUE_TYPE_STRING)
  {
    length= sizeof(buff);
    if (!(str= value->val_str(value, buff, &length)) ||
        (result= find_type(&bool_typelib, str, length, 1)-1) < 0)
      goto err;
  }
  else
  {
    if (value->val_int(value, &tmp) < 0)
      goto err;
    if (tmp != 0 && tmp != 1)
      goto err;
    result= (int) tmp;
  }
  *(my_bool *) save= result ? 1 : 0;
  return 0;
err:
  return 1;
}


static int check_func_int(THD *thd, struct st_mysql_sys_var *var,
                          void *save, st_mysql_value *value)
{
  my_bool fixed1, fixed2;
  long long orig, val;
  struct my_option options;
  value->val_int(value, &orig);
  val= orig;
  plugin_opt_set_limits(&options, var);

  if (var->flags & PLUGIN_VAR_UNSIGNED)
  {
    if ((fixed1= (!value->is_unsigned(value) && val < 0)))
      val=0;
    *(uint *)save= (uint) getopt_ull_limit_value((ulonglong) val, &options,
                                                   &fixed2);
  }
  else
  {
    if ((fixed1= (value->is_unsigned(value) && val < 0)))
      val=LONGLONG_MAX;
    *(int *)save= (int) getopt_ll_limit_value(val, &options, &fixed2);
  }

  return throw_bounds_warning(thd, var->name, fixed1 || fixed2,
                              value->is_unsigned(value), (longlong) orig);
}


static int check_func_long(THD *thd, struct st_mysql_sys_var *var,
                          void *save, st_mysql_value *value)
{
  my_bool fixed1, fixed2;
  long long orig, val;
  struct my_option options;
  value->val_int(value, &orig);
  val= orig;
  plugin_opt_set_limits(&options, var);

  if (var->flags & PLUGIN_VAR_UNSIGNED)
  {
    if ((fixed1= (!value->is_unsigned(value) && val < 0)))
      val=0;
    *(ulong *)save= (ulong) getopt_ull_limit_value((ulonglong) val, &options,
                                                   &fixed2);
  }
  else
  {
    if ((fixed1= (value->is_unsigned(value) && val < 0)))
      val=LONGLONG_MAX;
    *(long *)save= (long) getopt_ll_limit_value(val, &options, &fixed2);
  }

  return throw_bounds_warning(thd, var->name, fixed1 || fixed2,
                              value->is_unsigned(value), (longlong) orig);
}


static int check_func_longlong(THD *thd, struct st_mysql_sys_var *var,
                               void *save, st_mysql_value *value)
{
  my_bool fixed1, fixed2;
  long long orig, val;
  struct my_option options;
  value->val_int(value, &orig);
  val= orig;
  plugin_opt_set_limits(&options, var);

  if (var->flags & PLUGIN_VAR_UNSIGNED)
  {
    if ((fixed1= (!value->is_unsigned(value) && val < 0)))
      val=0;
    *(ulonglong *)save= getopt_ull_limit_value((ulonglong) val, &options,
                                               &fixed2);
  }
  else
  {
    if ((fixed1= (value->is_unsigned(value) && val < 0)))
      val=LONGLONG_MAX;
    *(longlong *)save= getopt_ll_limit_value(val, &options, &fixed2);
  }

  return throw_bounds_warning(thd, var->name, fixed1 || fixed2,
                              value->is_unsigned(value), (longlong) orig);
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
  const char *str;
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
    if ((result= (long)find_type(typelib, str, length, 0) - 1) < 0)
      goto err;
  }
  else
  {
    if (value->val_int(value, &tmp))
      goto err;
    if (tmp < 0 || tmp >= typelib->count)
      goto err;
    result= (long) tmp;
  }
  *(long*)save= result;
  return 0;
err:
  return 1;
}


static int check_func_set(THD *thd, struct st_mysql_sys_var *var,
                          void *save, st_mysql_value *value)
{
  char buff[STRING_BUFFER_USUAL_SIZE], *error= 0;
  const char *str;
  TYPELIB *typelib;
  ulonglong result;
  uint error_len= 0;                            // init as only set on error
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
      goto err;
  }
  else
  {
    if (value->val_int(value, (long long *)&result))
      goto err;
    if (unlikely((result >= (1ULL << typelib->count)) &&
                 (typelib->count < sizeof(long)*8)))
      goto err;
  }
  *(ulonglong*)save= result;
  return 0;
err:
  return 1;
}

static int check_func_double(THD *thd, struct st_mysql_sys_var *var,
                             void *save, st_mysql_value *value)
{
  double v;
  my_bool fixed;
  struct my_option option;

  value->val_real(value, &v);
  plugin_opt_set_limits(&option, var);
  *(double *) save= getopt_double_limit_value(v, &option, &fixed);

  return throw_bounds_warning(thd, var->name, fixed, v);
}


static void update_func_bool(THD *thd, struct st_mysql_sys_var *var,
                             void *tgt, const void *save)
{
  *(my_bool *) tgt= *(my_bool *) save ? 1 : 0;
}


static void update_func_int(THD *thd, struct st_mysql_sys_var *var,
                             void *tgt, const void *save)
{
  *(int *)tgt= *(int *) save;
}


static void update_func_long(THD *thd, struct st_mysql_sys_var *var,
                             void *tgt, const void *save)
{
  *(long *)tgt= *(long *) save;
}


static void update_func_longlong(THD *thd, struct st_mysql_sys_var *var,
                             void *tgt, const void *save)
{
  *(longlong *)tgt= *(ulonglong *) save;
}


static void update_func_str(THD *thd, struct st_mysql_sys_var *var,
                             void *tgt, const void *save)
{
  char *value= *(char**) save;
  if (var->flags & PLUGIN_VAR_MEMALLOC)
  {
    char *old= *(char**) tgt;
    if (value)
      *(char**) tgt= my_strdup(value, MYF(0));
    else
      *(char**) tgt= 0;
    my_free(old);
  }
  else
    *(char**) tgt= value;
}

static void update_func_double(THD *thd, struct st_mysql_sys_var *var,
                               void *tgt, const void *save)
{
  *(double *) tgt= *(double *) save;
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

  mysql_mutex_lock(&LOCK_plugin);
  mysql_rwlock_rdlock(&LOCK_system_variables_hash);
  if ((var= intern_find_sys_var(str, length)) &&
      (pi= var->cast_pluginvar()))
  {
    mysql_rwlock_unlock(&LOCK_system_variables_hash);
    LEX *lex= thd ? thd->lex : 0;
    if (!(plugin= intern_plugin_lock(lex, plugin_int_to_ref(pi->plugin))))
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
    mysql_rwlock_unlock(&LOCK_system_variables_hash);
  mysql_mutex_unlock(&LOCK_plugin);

  if (!var)
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

  varname[0]= plugin_var_bookmark_key(flags);

  result= (st_bookmark*) my_hash_search(&bookmark_hash,
                                        (const uchar*) varname, length - 1);

  my_afree(varname);
  return result;
}


static size_t var_storage_size(int flags)
{
  switch (flags & PLUGIN_VAR_TYPEMASK) {
  case PLUGIN_VAR_BOOL:         return sizeof(my_bool);
  case PLUGIN_VAR_INT:          return sizeof(int);
  case PLUGIN_VAR_LONG:         return sizeof(long);
  case PLUGIN_VAR_ENUM:         return sizeof(long);
  case PLUGIN_VAR_LONGLONG:     return sizeof(ulonglong);
  case PLUGIN_VAR_SET:          return sizeof(ulonglong);
  case PLUGIN_VAR_STR:          return sizeof(char*);
  case PLUGIN_VAR_DOUBLE:       return sizeof(double);
  default: DBUG_ASSERT(0);      return 0;
  }
}


/*
  returns a bookmark for thd-local variables, creating if neccessary.
  returns null for non thd-local variables.
  Requires that a write lock is obtained on LOCK_system_variables_hash
*/
static st_bookmark *register_var(const char *plugin, const char *name,
                                 int flags)
{
  uint length= strlen(plugin) + strlen(name) + 3, size, offset, new_size;
  st_bookmark *result;
  char *varname, *p;

  DBUG_ASSERT(flags & PLUGIN_VAR_THDLOCAL);

  size= var_storage_size(flags);
  varname= ((char*) my_alloca(length));
  strxmov(varname + 1, plugin, "_", name, NullS);
  for (p= varname + 1; *p; p++)
    if (*p == '-')
      *p= '_';

  if (!(result= find_bookmark(NULL, varname + 1, flags)))
  {
    result= (st_bookmark*) alloc_root(&plugin_vars_mem_root,
                                      sizeof(struct st_bookmark) + length-1);
    varname[0]= plugin_var_bookmark_key(flags);
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

static void restore_pluginvar_names(sys_var *first)
{
  mysql_del_sys_var_chain(first);
  for (sys_var *var= first; var; var= var->next)
  {
    sys_var_pluginvar *pv= var->cast_pluginvar();
    pv->plugin_var->name= pv->orig_pluginvar_name;
  }
}


/*
  returns a pointer to the memory which holds the thd-local variable or
  a pointer to the global variable if thd==null.
  If required, will sync with global variables if the requested variable
  has not yet been allocated in the current thread.
*/
static uchar *intern_sys_var_ptr(THD* thd, int offset, bool global_lock)
{
  DBUG_ENTER("intern_sys_var_ptr");
  DBUG_ASSERT(offset >= 0);
  DBUG_ASSERT((uint)offset <= global_system_variables.dynamic_variables_head);

  if (!thd)
    DBUG_RETURN((uchar*) global_system_variables.dynamic_variables_ptr + offset);

  mysql_mutex_assert_not_owner(&LOCK_open);

  /*
    dynamic_variables_head points to the largest valid offset
  */
  if (!thd->variables.dynamic_variables_ptr ||
      (uint)offset > thd->variables.dynamic_variables_head)
  {
    mysql_rwlock_rdlock(&LOCK_system_variables_hash);
    sync_dynamic_session_variables(thd, global_lock);
    mysql_rwlock_unlock(&LOCK_system_variables_hash);
  }
  DBUG_RETURN((uchar*)thd->variables.dynamic_variables_ptr + offset);
}


/**
  For correctness and simplicity's sake, a pointer to a function
  must be compatible with pointed-to type, that is, the return and
  parameters types must be the same. Thus, a callback function is
  defined for each scalar type. The functions are assigned in
  construct_options to their respective types.
*/

static char *mysql_sys_var_char(THD* thd, int offset)
{
  return (char *) intern_sys_var_ptr(thd, offset, true);
}

static int *mysql_sys_var_int(THD* thd, int offset)
{
  return (int *) intern_sys_var_ptr(thd, offset, true);
}

static long *mysql_sys_var_long(THD* thd, int offset)
{
  return (long *) intern_sys_var_ptr(thd, offset, true);
}

static unsigned long *mysql_sys_var_ulong(THD* thd, int offset)
{
  return (unsigned long *) intern_sys_var_ptr(thd, offset, true);
}

static long long *mysql_sys_var_longlong(THD* thd, int offset)
{
  return (long long *) intern_sys_var_ptr(thd, offset, true);
}

static unsigned long long *mysql_sys_var_ulonglong(THD* thd, int offset)
{
  return (unsigned long long *) intern_sys_var_ptr(thd, offset, true);
}

static char **mysql_sys_var_str(THD* thd, int offset)
{
  return (char **) intern_sys_var_ptr(thd, offset, true);
}

static double *mysql_sys_var_double(THD* thd, int offset)
{
  return (double *) intern_sys_var_ptr(thd, offset, true);
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

  mysql_mutex_lock(&LOCK_plugin);
  thd->variables.table_plugin=
        intern_plugin_lock(NULL, global_system_variables.table_plugin);
  intern_plugin_unlock(NULL, old_table_plugin);
  mysql_mutex_unlock(&LOCK_plugin);
  DBUG_VOID_RETURN;
}



void sync_dynamic_session_variables(THD* thd, bool global_lock)
{
  uint idx;

  thd->variables.dynamic_variables_ptr= (char*)
    my_realloc(thd->variables.dynamic_variables_ptr,
               global_variables_dynamic_size,
               MYF(MY_WME | MY_FAE | MY_ALLOW_ZERO_PTR));

  if (global_lock)
    mysql_mutex_lock(&LOCK_global_system_variables);

  mysql_mutex_assert_owner(&LOCK_global_system_variables);

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
    st_bookmark *v= (st_bookmark*) my_hash_element(&bookmark_hash,idx);

    if (v->version <= thd->variables.dynamic_variables_version)
      continue; /* already in thd->variables */

    /* Here we do anything special that may be required of the data types */

    if ((v->key[0] & PLUGIN_VAR_TYPEMASK) == PLUGIN_VAR_STR &&
         v->key[0] & BOOKMARK_MEMALLOC)
    {
      char **pp= (char**) (thd->variables.dynamic_variables_ptr + v->offset);
      if (*pp)
        *pp= my_strdup(*pp, MYF(MY_WME|MY_FAE));
    }
  }

  if (global_lock)
    mysql_mutex_unlock(&LOCK_global_system_variables);

  thd->variables.dynamic_variables_version=
         global_system_variables.dynamic_variables_version;
  thd->variables.dynamic_variables_head=
         global_system_variables.dynamic_variables_head;
  thd->variables.dynamic_variables_size=
         global_system_variables.dynamic_variables_size;
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
  uint idx;

  mysql_rwlock_rdlock(&LOCK_system_variables_hash);
  for (idx= 0; idx < bookmark_hash.records; idx++)
  {
    v= (st_bookmark*) my_hash_element(&bookmark_hash, idx);

    if (v->version > vars->dynamic_variables_version)
      continue; /* not in vars */

    DBUG_ASSERT((uint)v->offset <= vars->dynamic_variables_head);

    if ((v->key[0] & PLUGIN_VAR_TYPEMASK) == PLUGIN_VAR_STR &&
         v->key[0] & BOOKMARK_MEMALLOC)
    {
      char **ptr= (char**)(vars->dynamic_variables_ptr + v->offset);
      my_free(*ptr);
      *ptr= NULL;
    }
  }
  mysql_rwlock_unlock(&LOCK_system_variables_hash);

  DBUG_ASSERT(vars->table_plugin == NULL);

  my_free(vars->dynamic_variables_ptr);
  vars->dynamic_variables_ptr= NULL;
  vars->dynamic_variables_size= 0;
  vars->dynamic_variables_version= 0;
}


void plugin_thdvar_cleanup(THD *thd)
{
  uint idx;
  plugin_ref *list;
  DBUG_ENTER("plugin_thdvar_cleanup");

  mysql_mutex_lock(&LOCK_plugin);

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
  mysql_mutex_unlock(&LOCK_plugin);

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
                            var->name.str, (long) valptr));
      my_free(*valptr);
      *valptr= NULL;
    }
  }
  DBUG_VOID_RETURN;
}

static SHOW_TYPE pluginvar_show_type(st_mysql_sys_var *plugin_var)
{
  switch (plugin_var->flags & (PLUGIN_VAR_TYPEMASK | PLUGIN_VAR_UNSIGNED)) {
  case PLUGIN_VAR_BOOL:
    return SHOW_MY_BOOL;
  case PLUGIN_VAR_INT:
    return SHOW_SINT;
  case PLUGIN_VAR_INT | PLUGIN_VAR_UNSIGNED:
    return SHOW_UINT;
  case PLUGIN_VAR_LONG:
    return SHOW_SLONG;
  case PLUGIN_VAR_LONG | PLUGIN_VAR_UNSIGNED:
    return SHOW_ULONG;
  case PLUGIN_VAR_LONGLONG:
    return SHOW_SLONGLONG;
  case PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED:
    return SHOW_ULONGLONG;
  case PLUGIN_VAR_STR:
    return SHOW_CHAR_PTR;
  case PLUGIN_VAR_ENUM:
  case PLUGIN_VAR_SET:
    return SHOW_CHAR;
  case PLUGIN_VAR_DOUBLE:
    return SHOW_DOUBLE;
  default:
    DBUG_ASSERT(0);
    return SHOW_UNDEF;
  }
}


bool sys_var_pluginvar::check_update_type(Item_result type)
{
  switch (plugin_var->flags & PLUGIN_VAR_TYPEMASK) {
  case PLUGIN_VAR_INT:
  case PLUGIN_VAR_LONG:
  case PLUGIN_VAR_LONGLONG:
    return type != INT_RESULT;
  case PLUGIN_VAR_STR:
    return type != STRING_RESULT;
  case PLUGIN_VAR_ENUM:
  case PLUGIN_VAR_BOOL:
  case PLUGIN_VAR_SET:
    return type != STRING_RESULT && type != INT_RESULT;
  case PLUGIN_VAR_DOUBLE:
    return type != INT_RESULT && type != REAL_RESULT && type != DECIMAL_RESULT;
  default:
    return true;
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
  return NULL;	/* Keep compiler happy */
}


uchar* sys_var_pluginvar::do_value_ptr(THD *thd, enum_var_type type,
                                       LEX_STRING *base)
{
  uchar* result;

  result= real_value_ptr(thd, type);

  if ((plugin_var->flags & PLUGIN_VAR_TYPEMASK) == PLUGIN_VAR_ENUM)
    result= (uchar*) get_type(plugin_var_typelib(), *(ulong*)result);
  else if ((plugin_var->flags & PLUGIN_VAR_TYPEMASK) == PLUGIN_VAR_SET)
    result= (uchar*) set_to_string(thd, 0, *(ulonglong*) result,
                                   plugin_var_typelib()->type_names);
  return result;
}

bool sys_var_pluginvar::do_check(THD *thd, set_var *var)
{
  st_item_value_holder value;
  DBUG_ASSERT(!is_readonly());
  DBUG_ASSERT(plugin_var->check);

  value.value_type= item_value_type;
  value.val_str= item_val_str;
  value.val_int= item_val_int;
  value.val_real= item_val_real;
  value.is_unsigned= item_is_unsigned;
  value.item= var->value;

  return plugin_var->check(thd, plugin_var, &var->save_result, &value);
}

bool sys_var_pluginvar::session_update(THD *thd, set_var *var)
{
  DBUG_ASSERT(!is_readonly());
  DBUG_ASSERT(plugin_var->flags & PLUGIN_VAR_THDLOCAL);
  DBUG_ASSERT(thd == current_thd);

  mysql_mutex_lock(&LOCK_global_system_variables);
  void *tgt= real_value_ptr(thd, var->type);
  const void *src= var->value ? (void*)&var->save_result
                              : (void*)real_value_ptr(thd, OPT_GLOBAL);
  mysql_mutex_unlock(&LOCK_global_system_variables);

  plugin_var->update(thd, plugin_var, tgt, src);
 
  return false;
}

static const void *var_def_ptr(st_mysql_sys_var *pv)
{
    switch (pv->flags & (PLUGIN_VAR_TYPEMASK | PLUGIN_VAR_THDLOCAL)) {
    case PLUGIN_VAR_INT:
      return &((sysvar_uint_t*) pv)->def_val;
    case PLUGIN_VAR_LONG:
      return &((sysvar_ulong_t*) pv)->def_val;
    case PLUGIN_VAR_LONGLONG:
      return &((sysvar_ulonglong_t*) pv)->def_val;
    case PLUGIN_VAR_ENUM:
      return &((sysvar_enum_t*) pv)->def_val;
    case PLUGIN_VAR_SET:
      return &((sysvar_set_t*) pv)->def_val;
    case PLUGIN_VAR_BOOL:
      return &((sysvar_bool_t*) pv)->def_val;
    case PLUGIN_VAR_STR:
      return &((sysvar_str_t*) pv)->def_val;
    case PLUGIN_VAR_DOUBLE:
      return &((sysvar_double_t*) pv)->def_val;
    case PLUGIN_VAR_INT | PLUGIN_VAR_THDLOCAL:
      return &((thdvar_uint_t*) pv)->def_val;
    case PLUGIN_VAR_LONG | PLUGIN_VAR_THDLOCAL:
      return &((thdvar_ulong_t*) pv)->def_val;
    case PLUGIN_VAR_LONGLONG | PLUGIN_VAR_THDLOCAL:
      return &((thdvar_ulonglong_t*) pv)->def_val;
    case PLUGIN_VAR_ENUM | PLUGIN_VAR_THDLOCAL:
      return &((thdvar_enum_t*) pv)->def_val;
    case PLUGIN_VAR_SET | PLUGIN_VAR_THDLOCAL:
      return &((thdvar_set_t*) pv)->def_val;
    case PLUGIN_VAR_BOOL | PLUGIN_VAR_THDLOCAL:
      return &((thdvar_bool_t*) pv)->def_val;
    case PLUGIN_VAR_STR | PLUGIN_VAR_THDLOCAL:
      return &((thdvar_str_t*) pv)->def_val;
    case PLUGIN_VAR_DOUBLE | PLUGIN_VAR_THDLOCAL:
      return &((thdvar_double_t*) pv)->def_val;
    default:
      DBUG_ASSERT(0);
      return NULL;
    }
}


bool sys_var_pluginvar::global_update(THD *thd, set_var *var)
{
  DBUG_ASSERT(!is_readonly());
  mysql_mutex_assert_owner(&LOCK_global_system_variables);

  void *tgt= real_value_ptr(thd, var->type);
  const void *src= &var->save_result;

  if (!var->value)
    src= var_def_ptr(plugin_var);

  plugin_var->update(thd, plugin_var, tgt, src);
  return false;
}


#define OPTION_SET_LIMITS(type, options, opt) \
  options->var_type= type; \
  options->def_value= (opt)->def_val; \
  options->min_value= (opt)->min_val; \
  options->max_value= (opt)->max_val; \
  options->block_size= (long) (opt)->blk_sz

#define OPTION_SET_LIMITS_DOUBLE(options, opt) \
  options->var_type= GET_DOUBLE; \
  options->def_value= (longlong) getopt_double2ulonglong((opt)->def_val); \
  options->min_value= (longlong) getopt_double2ulonglong((opt)->min_val); \
  options->max_value= getopt_double2ulonglong((opt)->max_val); \
  options->block_size= (long) (opt)->blk_sz;


static void plugin_opt_set_limits(struct my_option *options,
                                  const struct st_mysql_sys_var *opt)
{
  options->sub_size= 0;

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
    options->def_value= ((sysvar_enum_t*) opt)->def_val;
    options->min_value= options->block_size= 0;
    options->max_value= options->typelib->count - 1;
    break;
  case PLUGIN_VAR_SET:
    options->var_type= GET_SET;
    options->typelib= ((sysvar_set_t*) opt)->typelib;
    options->def_value= ((sysvar_set_t*) opt)->def_val;
    options->min_value= options->block_size= 0;
    options->max_value= (1ULL << options->typelib->count) - 1;
    break;
  case PLUGIN_VAR_BOOL:
    options->var_type= GET_BOOL;
    options->def_value= ((sysvar_bool_t*) opt)->def_val;
    break;
  case PLUGIN_VAR_STR:
    options->var_type= ((opt->flags & PLUGIN_VAR_MEMALLOC) ?
                        GET_STR_ALLOC : GET_STR);
    options->def_value= (intptr) ((sysvar_str_t*) opt)->def_val;
    break;
  case PLUGIN_VAR_DOUBLE:
    OPTION_SET_LIMITS_DOUBLE(options, (sysvar_double_t*) opt);
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
  case PLUGIN_VAR_DOUBLE | PLUGIN_VAR_THDLOCAL:
    OPTION_SET_LIMITS_DOUBLE(options, (thdvar_double_t*) opt);
    break;
  case PLUGIN_VAR_ENUM | PLUGIN_VAR_THDLOCAL:
    options->var_type= GET_ENUM;
    options->typelib= ((thdvar_enum_t*) opt)->typelib;
    options->def_value= ((thdvar_enum_t*) opt)->def_val;
    options->min_value= options->block_size= 0;
    options->max_value= options->typelib->count - 1;
    break;
  case PLUGIN_VAR_SET | PLUGIN_VAR_THDLOCAL:
    options->var_type= GET_SET;
    options->typelib= ((thdvar_set_t*) opt)->typelib;
    options->def_value= ((thdvar_set_t*) opt)->def_val;
    options->min_value= options->block_size= 0;
    options->max_value= (1ULL << options->typelib->count) - 1;
    break;
  case PLUGIN_VAR_BOOL | PLUGIN_VAR_THDLOCAL:
    options->var_type= GET_BOOL;
    options->def_value= ((thdvar_bool_t*) opt)->def_val;
    break;
  case PLUGIN_VAR_STR | PLUGIN_VAR_THDLOCAL:
    options->var_type= ((opt->flags & PLUGIN_VAR_MEMALLOC) ?
                        GET_STR_ALLOC : GET_STR);
    options->def_value= (intptr) ((thdvar_str_t*) opt)->def_val;
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


/**
  Creates a set of my_option objects associated with a specified plugin-
  handle.

  @param mem_root Memory allocator to be used.
  @param tmp A pointer to a plugin handle
  @param[out] options A pointer to a pre-allocated static array

  The set is stored in the pre-allocated static array supplied to the function.
  The size of the array is calculated as (number_of_plugin_varaibles*2+3). The
  reason is that each option can have a prefix '--plugin-' in addtion to the
  shorter form '--&lt;plugin-name&gt;'. There is also space allocated for
  terminating NULL pointers.

  @return
    @retval -1 An error occurred
    @retval 0 Success
*/

static int construct_options(MEM_ROOT *mem_root, struct st_plugin_int *tmp,
                             my_option *options)
{
  const char *plugin_name= tmp->plugin->name;
  const LEX_STRING plugin_dash = { C_STRING_WITH_LEN("plugin-") };
  uint plugin_name_len= strlen(plugin_name);
  uint optnamelen;
  const int max_comment_len= 180;
  char *comment= (char *) alloc_root(mem_root, max_comment_len + 1);
  char *optname;

  int index= 0, offset= 0;
  st_mysql_sys_var *opt, **plugin_option;
  st_bookmark *v;

  /** Used to circumvent the const attribute on my_option::name */
  char *plugin_name_ptr, *plugin_name_with_prefix_ptr;

  DBUG_ENTER("construct_options");

  plugin_name_ptr= (char*) alloc_root(mem_root, plugin_name_len + 1);
  strcpy(plugin_name_ptr, plugin_name);
  my_casedn_str(&my_charset_latin1, plugin_name_ptr);
  convert_underscore_to_dash(plugin_name_ptr, plugin_name_len);
  plugin_name_with_prefix_ptr= (char*) alloc_root(mem_root,
                                                  plugin_name_len +
                                                  plugin_dash.length + 1);
  strxmov(plugin_name_with_prefix_ptr, plugin_dash.str, plugin_name_ptr, NullS);

  if (tmp->load_option != PLUGIN_FORCE &&
      tmp->load_option != PLUGIN_FORCE_PLUS_PERMANENT)
  {
    /* support --skip-plugin-foo syntax */
    options[0].name= plugin_name_ptr;
    options[1].name= plugin_name_with_prefix_ptr;
    options[0].id= options[1].id= 0;
    options[0].var_type= options[1].var_type= GET_ENUM;
    options[0].arg_type= options[1].arg_type= OPT_ARG;
    options[0].def_value= options[1].def_value= 1; /* ON */
    options[0].typelib= options[1].typelib= &global_plugin_typelib;

    strxnmov(comment, max_comment_len, "Enable or disable ", plugin_name,
            " plugin. Possible values are ON, OFF, FORCE (don't start "
            "if the plugin fails to load).", NullS);
    options[0].comment= comment;
    /*
      Allocate temporary space for the value of the tristate.
      This option will have a limited lifetime and is not used beyond
      server initialization.
      GET_ENUM value is an unsigned long integer.
    */
    options[0].value= options[1].value=
                      (uchar **)alloc_root(mem_root, sizeof(ulong));
    *((ulong*) options[0].value)= (ulong) options[0].def_value;

    options+= 2;
  }

  if (!my_strcasecmp(&my_charset_latin1, plugin_name_ptr, "NDBCLUSTER"))
  {
    plugin_name_ptr= const_cast<char*>("ndb"); // Use legacy "ndb" prefix
    plugin_name_len= 3;
  }

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
    if (!(register_var(plugin_name_ptr, opt->name, opt->flags)))
      continue;
    switch (opt->flags & PLUGIN_VAR_TYPEMASK) {
    case PLUGIN_VAR_BOOL:
      ((thdvar_bool_t *) opt)->resolve= mysql_sys_var_char;
      break;
    case PLUGIN_VAR_INT:
      ((thdvar_int_t *) opt)->resolve= mysql_sys_var_int;
      break;
    case PLUGIN_VAR_LONG:
      ((thdvar_long_t *) opt)->resolve= mysql_sys_var_long;
      break;
    case PLUGIN_VAR_LONGLONG:
      ((thdvar_longlong_t *) opt)->resolve= mysql_sys_var_longlong;
      break;
    case PLUGIN_VAR_STR:
      ((thdvar_str_t *) opt)->resolve= mysql_sys_var_str;
      break;
    case PLUGIN_VAR_ENUM:
      ((thdvar_enum_t *) opt)->resolve= mysql_sys_var_ulong;
      break;
    case PLUGIN_VAR_SET:
      ((thdvar_set_t *) opt)->resolve= mysql_sys_var_ulonglong;
      break;
    case PLUGIN_VAR_DOUBLE:
      ((thdvar_double_t *) opt)->resolve= mysql_sys_var_double;
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
        if (!(opt->flags & (PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY)))
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
    case PLUGIN_VAR_DOUBLE:
      if (!opt->check)
        opt->check= check_func_double;
      if (!opt->update)
        opt->update= update_func_double;
      break;
    default:
      sql_print_error("Unknown variable type code 0x%x in plugin '%s'.",
                      opt->flags, plugin_name);
      DBUG_RETURN(-1);
    }

    if ((opt->flags & (PLUGIN_VAR_NOCMDOPT | PLUGIN_VAR_THDLOCAL))
                    == PLUGIN_VAR_NOCMDOPT)
      continue;

    if (!opt->name)
    {
      sql_print_error("Missing variable name in plugin '%s'.",
                      plugin_name);
      DBUG_RETURN(-1);
    }

    if (!(opt->flags & PLUGIN_VAR_THDLOCAL))
    {
      optnamelen= strlen(opt->name);
      optname= (char*) alloc_root(mem_root, plugin_name_len + optnamelen + 2);
      strxmov(optname, plugin_name_ptr, "-", opt->name, NullS);
      optnamelen= plugin_name_len + optnamelen + 1;
    }
    else
    {
      /* this should not fail because register_var should create entry */
      if (!(v= find_bookmark(plugin_name_ptr, opt->name, opt->flags)))
      {
        sql_print_error("Thread local variable '%s' not allocated "
                        "in plugin '%s'.", opt->name, plugin_name);
        DBUG_RETURN(-1);
      }

      *(int*)(opt + 1)= offset= v->offset;

      if (opt->flags & PLUGIN_VAR_NOCMDOPT)
      {
        char *val= global_system_variables.dynamic_variables_ptr + offset;
        if (((opt->flags & PLUGIN_VAR_TYPEMASK) == PLUGIN_VAR_STR) &&
             (opt->flags & PLUGIN_VAR_MEMALLOC))
        {
          char *def_val= *(char**)var_def_ptr(opt);
          *(char**)val= def_val ? my_strdup(def_val, MYF(0)) : NULL;
        }
        else
          memcpy(val, var_def_ptr(opt), var_storage_size(opt->flags));
        continue;
      }

      optname= (char*) memdup_root(mem_root, v->key + 1, 
                                   (optnamelen= v->name_len) + 1);
    }

    convert_underscore_to_dash(optname, optnamelen);

    options->name= optname;
    options->comment= opt->comment;
    options->app_type= opt;
    options->id= 0;

    plugin_opt_set_limits(options, opt);

    if (opt->flags & PLUGIN_VAR_THDLOCAL)
      options->value= options->u_max_value= (uchar**)
        (global_system_variables.dynamic_variables_ptr + offset);
    else
      options->value= options->u_max_value= *(uchar***) (opt + 1);

    char *option_name_ptr;
    options[1]= options[0];
    options[1].name= option_name_ptr= (char*) alloc_root(mem_root,
                                                        plugin_dash.length +
                                                        optnamelen + 1);
    options[1].comment= 0; /* Hidden from the help text */
    strxmov(option_name_ptr, plugin_dash.str, optname, NullS);

    options+= 2;
  }

  DBUG_RETURN(0);
}


static my_option *construct_help_options(MEM_ROOT *mem_root,
                                         struct st_plugin_int *p)
{
  st_mysql_sys_var **opt;
  my_option *opts;
  uint count= EXTRA_OPTIONS;
  DBUG_ENTER("construct_help_options");

  for (opt= p->plugin->system_vars; opt && *opt; opt++, count+= 2)
    ;

  if (!(opts= (my_option*) alloc_root(mem_root, sizeof(my_option) * count)))
    DBUG_RETURN(NULL);

  bzero(opts, sizeof(my_option) * count);

  /**
    some plugin variables (those that don't have PLUGIN_VAR_NOSYSVAR flag)
    have their names prefixed with the plugin name. Restore the names here
    to get the correct (not double-prefixed) help text.
    We won't need @@sysvars anymore and don't care about their proper names.
  */
  restore_pluginvar_names(p->system_vars);

  if (construct_options(mem_root, p, opts))
    DBUG_RETURN(NULL);

  DBUG_RETURN(opts);
}

/**
  Create and register system variables supplied from the plugin and
  assigns initial values from corresponding command line arguments.

  @param tmp_root Temporary scratch space
  @param[out] plugin Internal plugin structure
  @param argc Number of command line arguments
  @param argv Command line argument vector

  The plugin will be updated with a policy on how to handle errors during
  initialization.

  @note Requires that a write-lock is held on LOCK_system_variables_hash

  @return How initialization of the plugin should be handled.
    @retval  0 Initialization should proceed.
    @retval  1 Plugin is disabled.
    @retval -1 An error has occurred.
*/

static int test_plugin_options(MEM_ROOT *tmp_root, struct st_plugin_int *tmp,
                               int *argc, char **argv)
{
  struct sys_var_chain chain= { NULL, NULL };
  bool disable_plugin;
  enum_plugin_load_option plugin_load_option= tmp->load_option;

  MEM_ROOT *mem_root= alloc_root_inited(&tmp->mem_root) ?
                      &tmp->mem_root : &plugin_vars_mem_root;
  st_mysql_sys_var **opt;
  my_option *opts= NULL;
  LEX_STRING plugin_name;
  char *varname;
  int error;
  sys_var *v __attribute__((unused));
  struct st_bookmark *var;
  uint len, count= EXTRA_OPTIONS;
  DBUG_ENTER("test_plugin_options");
  DBUG_ASSERT(tmp->plugin && tmp->name.str);

  for (opt= tmp->plugin->system_vars; opt && *opt; opt++)
    count+= 2; /* --{plugin}-{optname} and --plugin-{plugin}-{optname} */

  if (count > EXTRA_OPTIONS || (*argc > 1))
  {
    if (!(opts= (my_option*) alloc_root(tmp_root, sizeof(my_option) * count)))
    {
      sql_print_error("Out of memory for plugin '%s'.", tmp->name.str);
      DBUG_RETURN(-1);
    }
    bzero(opts, sizeof(my_option) * count);

    if (construct_options(tmp_root, tmp, opts))
    {
      sql_print_error("Bad options for plugin '%s'.", tmp->name.str);
      DBUG_RETURN(-1);
    }

    /*
      We adjust the default value to account for the hardcoded exceptions
      we have set for the federated and ndbcluster storage engines.
    */
    if (tmp->load_option != PLUGIN_FORCE &&
        tmp->load_option != PLUGIN_FORCE_PLUS_PERMANENT)
      opts[0].def_value= opts[1].def_value= plugin_load_option;

    error= handle_options(argc, &argv, opts, NULL);
    (*argc)++; /* add back one for the program name */

    if (error)
    {
       sql_print_error("Parsing options for plugin '%s' failed.",
                       tmp->name.str);
       goto err;
    }
    /*
     Set plugin loading policy from option value. First element in the option
     list is always the <plugin name> option value.
    */
    if (tmp->load_option != PLUGIN_FORCE &&
        tmp->load_option != PLUGIN_FORCE_PLUS_PERMANENT)
      plugin_load_option= (enum_plugin_load_option) *(ulong*) opts[0].value;
  }

  disable_plugin= (plugin_load_option == PLUGIN_OFF);
  tmp->load_option= plugin_load_option;

  /*
    If the plugin is disabled it should not be initialized.
  */
  if (disable_plugin)
  {
    if (global_system_variables.log_warnings)
      sql_print_information("Plugin '%s' is disabled.",
                            tmp->name.str);
    if (opts)
      my_cleanup_options(opts);
    DBUG_RETURN(1);
  }

  if (!my_strcasecmp(&my_charset_latin1, tmp->name.str, "NDBCLUSTER"))
  {
    plugin_name.str= const_cast<char*>("ndb"); // Use legacy "ndb" prefix
    plugin_name.length= 3;
  }
  else
    plugin_name= tmp->name;

  error= 1;
  for (opt= tmp->plugin->system_vars; opt && *opt; opt++)
  {
    st_mysql_sys_var *o= *opt;

    /*
      PLUGIN_VAR_STR command-line options without PLUGIN_VAR_MEMALLOC, point
      directly to values in the argv[] array. For plugins started at the
      server startup, argv[] array is allocated with load_defaults(), and
      freed when the server is shut down.  But for plugins loaded with
      INSTALL PLUGIN, the memory allocated with load_defaults() is freed with
      freed() at the end of mysql_install_plugin(). Which means we cannot
      allow any pointers into that area.
      Thus, for all plugins loaded after the server was started,
      we copy string values to a plugin's memroot.
    */
    if (mysqld_server_started &&
        ((o->flags & (PLUGIN_VAR_TYPEMASK | PLUGIN_VAR_NOCMDOPT |
                       PLUGIN_VAR_MEMALLOC)) == PLUGIN_VAR_STR))
    {
      sysvar_str_t* str= (sysvar_str_t *)o;
      if (*str->value)
        *str->value= strdup_root(mem_root, *str->value);
    }

    var= find_bookmark(plugin_name.str, o->name, o->flags);
    if (o->flags & PLUGIN_VAR_NOSYSVAR)
      continue;
    if (var)
      v= new (mem_root) sys_var_pluginvar(&chain, var->key + 1, o, tmp);
    else
    {
      len= plugin_name.length + strlen(o->name) + 2;
      varname= (char*) alloc_root(mem_root, len);
      strxmov(varname, plugin_name.str, "-", o->name, NullS);
      my_casedn_str(&my_charset_latin1, varname);
      convert_dash_to_underscore(varname, len-1);
      v= new (mem_root) sys_var_pluginvar(&chain, varname, o, tmp);
    }
    DBUG_ASSERT(v); /* check that an object was actually constructed */
  } /* end for */
  if (chain.first)
  {
    chain.last->next = NULL;
    if (mysql_add_sys_var_chain(chain.first))
    {
      sql_print_error("Plugin '%s' has conflicting system variables",
                      tmp->name.str);
      goto err;
    }
    tmp->system_vars= chain.first;
  }
  DBUG_RETURN(0);
  
err:
  if (opts)
    my_cleanup_options(opts);
  DBUG_RETURN(error);
}


/****************************************************************************
  Help Verbose text with Plugin System Variables
****************************************************************************/


void add_plugin_options(DYNAMIC_ARRAY *options, MEM_ROOT *mem_root)
{
  struct st_plugin_int *p;
  my_option *opt;

  if (!initialized)
    return;

  for (uint idx= 0; idx < plugin_array.elements; idx++)
  {
    p= *dynamic_element(&plugin_array, idx, struct st_plugin_int **);

    if (!(opt= construct_help_options(mem_root, p)))
      continue;

    /* Only options with a non-NULL comment are displayed in help text */
    for (;opt->name; opt++)
      if (opt->comment)
        insert_dynamic(options, (uchar*) opt);
  }
}

