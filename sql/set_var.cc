/* Copyright (c) 2002, 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation
#endif

/* variable declarations are in sys_vars.cc now !!! */

#include "my_global.h"                          /* NO_EMBEDDED_ACCESS_CHECKS */
#include "sql_class.h"                   // set_var.h: session_var_ptr
#include "set_var.h"
#include "sql_priv.h"
#include "unireg.h"
#include "mysqld.h"                             // lc_messages_dir
#include "sys_vars_shared.h"
#include "transaction.h"
#include "sql_locale.h"                         // my_locale_by_number,
                                                // my_locale_by_name
#include "strfunc.h"      // find_set_from_flags, find_set
#include "sql_parse.h"    // check_global_access
#include "sql_table.h"  // reassign_keycache_tables
#include "sql_time.h"   // date_time_format_copy,
                        // date_time_format_make
#include "derror.h"
#include "tztime.h"     // my_tz_find, my_tz_SYSTEM, struct Time_zone
#include "sql_acl.h"    // SUPER_ACL
#include "sql_select.h" // free_underlaid_joins
#include "sql_show.h"   // make_default_log_name
#include "sql_view.h"   // updatable_views_with_limit_typelib
#include "lock.h"                               // lock_global_read_lock,
                                                // make_global_read_lock_block_commit,
                                                // unlock_global_read_lock

static HASH system_variable_hash;
static PolyLock_mutex PLock_global_system_variables(&LOCK_global_system_variables);

/**
  Return variable name and length for hashing of variables.
*/

static uchar *get_sys_var_length(const sys_var *var, size_t *length,
                                 my_bool first)
{
  *length= var->name.length;
  return (uchar*) var->name.str;
}

sys_var_chain all_sys_vars = { NULL, NULL };

int sys_var_init()
{
  DBUG_ENTER("sys_var_init");

  /* Must be already initialized. */
  DBUG_ASSERT(system_charset_info != NULL);

  if (my_hash_init(&system_variable_hash, system_charset_info, 100, 0,
                   0, (my_hash_get_key) get_sys_var_length, 0, HASH_UNIQUE))
    goto error;

  if (mysql_add_sys_var_chain(all_sys_vars.first))
    goto error;

  DBUG_RETURN(0);

error:
  fprintf(stderr, "failed to initialize System variables");
  DBUG_RETURN(1);
}

int sys_var_add_options(DYNAMIC_ARRAY *long_options, int parse_flags)
{
  uint saved_elements= long_options->elements;

  DBUG_ENTER("sys_var_add_options");

  for (sys_var *var=all_sys_vars.first; var; var= var->next)
  {
    if (var->register_option(long_options, parse_flags))
      goto error;
  }

  DBUG_RETURN(0);

error:
  fprintf(stderr, "failed to initialize System variables");
  long_options->elements= saved_elements;
  DBUG_RETURN(1);
}

void sys_var_end()
{
  DBUG_ENTER("sys_var_end");

  my_hash_free(&system_variable_hash);

  for (sys_var *var=all_sys_vars.first; var; var= var->next)
    var->cleanup();

  DBUG_VOID_RETURN;
}

/**
  sys_var constructor

  @param chain     variables are linked into chain for mysql_add_sys_var_chain()
  @param name_arg  the name of the variable. Must be 0-terminated and exist
                   for the liftime of the sys_var object. @sa my_option::name
  @param comment   shown in mysqld --help, @sa my_option::comment
  @param flags_arg or'ed flag_enum values
  @param off       offset of the global variable value from the
                   &global_system_variables.
  @param getopt_id -1 for no command-line option, otherwise @sa my_option::id
  @param getopt_arg_type @sa my_option::arg_type
  @param show_val_type_arg what value_ptr() returns for sql_show.cc
  @param def_val   default value, @sa my_option::def_value
  @param lock      mutex or rw_lock that protects the global variable
                   *in addition* to LOCK_global_system_variables.
  @param binlog_status_enum @sa binlog_status_enum
  @param on_check_func a function to be called at the end of sys_var::check,
                   put your additional checks here
  @param on_update_func a function to be called at the end of sys_var::update,
                   any post-update activity should happen here
  @param deprecated_version if not 0 - when this variable will go away
  @param substitute if not 0 - what one should use instead when this
                   deprecated variable
  @param parse_flag either PARSE_EARLY or PARSE_NORMAL
*/
sys_var::sys_var(sys_var_chain *chain, const char *name_arg,
                 const char *comment, int flags_arg, ptrdiff_t off,
                 int getopt_id, enum get_opt_arg_type getopt_arg_type,
                 SHOW_TYPE show_val_type_arg, longlong def_val,
                 PolyLock *lock, enum binlog_status_enum binlog_status_arg,
                 on_check_function on_check_func,
                 on_update_function on_update_func,
                 uint deprecated_version, const char *substitute,
                 int parse_flag) :
  next(0),
  binlog_status(binlog_status_arg),
  flags(flags_arg), m_parse_flag(parse_flag), show_val_type(show_val_type_arg),
  guard(lock), offset(off), on_check(on_check_func), on_update(on_update_func),
  is_os_charset(FALSE)
{
  /*
    There is a limitation in handle_options() related to short options:
    - either all short options should be declared when parsing in multiple stages,
    - or none should be declared.
    Because a lot of short options are used in the normal parsing phase
    for mysqld, we enforce here that no short option is present
    in the first (PARSE_EARLY) stage.
    See handle_options() for details.
  */
  DBUG_ASSERT(parse_flag == PARSE_NORMAL || getopt_id <= 0 || getopt_id >= 255);

  name.str= name_arg;     // ER_NO_DEFAULT relies on 0-termination of name_arg
  name.length= strlen(name_arg);                // and so does this.
  DBUG_ASSERT(name.length <= NAME_CHAR_LEN);

  bzero(&option, sizeof(option));
  option.name= name_arg;
  option.id= getopt_id;
  option.comment= comment;
  option.arg_type= getopt_arg_type;
  option.value= (uchar **)global_var_ptr();
  option.def_value= def_val;

  deprecated.version= deprecated_version;
  deprecated.substitute= substitute;
  DBUG_ASSERT((deprecated_version != 0) || (substitute == 0));
  DBUG_ASSERT(deprecated_version % 100 == 0);
  DBUG_ASSERT(!deprecated_version || MYSQL_VERSION_ID < deprecated_version);

  if (chain->last)
    chain->last->next= this;
  else
    chain->first= this;
  chain->last= this;
}

bool sys_var::update(THD *thd, set_var *var)
{
  enum_var_type type= var->type;
  if (type == OPT_GLOBAL || scope() == GLOBAL)
  {
    /*
      Yes, both locks need to be taken before an update, just as
      both are taken to get a value. If we'll take only 'guard' here,
      then value_ptr() for strings won't be safe in SHOW VARIABLES anymore,
      to make it safe we'll need value_ptr_unlock().
    */
    AutoWLock lock1(&PLock_global_system_variables);
    AutoWLock lock2(guard);
    return global_update(thd, var) ||
      (on_update && on_update(this, thd, OPT_GLOBAL));
  }
  else
    return session_update(thd, var) ||
      (on_update && on_update(this, thd, OPT_SESSION));
}

uchar *sys_var::session_value_ptr(THD *thd, LEX_STRING *base)
{
  return session_var_ptr(thd);
}

uchar *sys_var::global_value_ptr(THD *thd, LEX_STRING *base)
{
  return global_var_ptr();
}

bool sys_var::check(THD *thd, set_var *var)
{
  do_deprecated_warning(thd);
  if ((var->value && do_check(thd, var))
      || (on_check && on_check(this, thd, var)))
  {
    if (!thd->is_error())
    {
      char buff[STRING_BUFFER_USUAL_SIZE];
      String str(buff, sizeof(buff), system_charset_info), *res;

      if (!var->value)
      {
        str.set(STRING_WITH_LEN("DEFAULT"), &my_charset_latin1);
        res= &str;
      }
      else if (!(res=var->value->val_str(&str)))
      {
        str.set(STRING_WITH_LEN("NULL"), &my_charset_latin1);
        res= &str;
      }
      ErrConvString err(res);
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name.str, err.ptr());
    }
    return true;
  }
  return false;
}

uchar *sys_var::value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
{
  if (type == OPT_GLOBAL || scope() == GLOBAL)
  {
    mysql_mutex_assert_owner(&LOCK_global_system_variables);
    AutoRLock lock(guard);
    return global_value_ptr(thd, base);
  }
  else
    return session_value_ptr(thd, base);
}

bool sys_var::set_default(THD *thd, enum_var_type type)
{
  LEX_STRING empty={0,0};
  set_var var(type, 0, &empty, 0);

  if (type == OPT_GLOBAL || scope() == GLOBAL)
    global_save_default(thd, &var);
  else
    session_save_default(thd, &var);

  return check(thd, &var) || update(thd, &var);
}

void sys_var::do_deprecated_warning(THD *thd)
{
  if (deprecated.version)
  {
    char buf1[NAME_CHAR_LEN + 3], buf2[10];
    strxnmov(buf1, sizeof(buf1)-1, "@@", name.str, 0);
    my_snprintf(buf2, sizeof(buf2), "%d.%d", deprecated.version/100/100,
                deprecated.version/100%100);
    uint errmsg= deprecated.substitute
                        ? ER_WARN_DEPRECATED_SYNTAX_WITH_VER
                        : ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT;
    if (thd)
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                          ER_WARN_DEPRECATED_SYNTAX, ER(errmsg),
                          buf1, buf2, deprecated.substitute);
    else
      sql_print_warning(ER_DEFAULT(errmsg), buf1, buf2, deprecated.substitute);
  }
}

/**
  Throw warning (error in STRICT mode) if value for variable needed bounding.
  Plug-in interface also uses this.

  @param thd         thread handle
  @param name        variable's name
  @param fixed       did we have to correct the value? (throw warn/err if so)
  @param is_unsigned is value's type unsigned?
  @param v           variable's value

  @retval         true on error, false otherwise (warning or ok)
 */
bool throw_bounds_warning(THD *thd, const char *name,
                          bool fixed, bool is_unsigned, longlong v)
{
  if (fixed || (!is_unsigned && v < 0))
  {
    char buf[22];

    if (is_unsigned)
      ullstr((ulonglong) v, buf);
    else
      llstr(v, buf);

    if (thd->variables.sql_mode & MODE_STRICT_ALL_TABLES)
    {
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, buf);
      return true;
    }
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE,
                        ER(ER_TRUNCATED_WRONG_VALUE), name, buf);
  }
  return false;
}

bool throw_bounds_warning(THD *thd, const char *name, bool fixed, double v)
{
  if (fixed)
  {
    char buf[64];

    my_gcvt(v, MY_GCVT_ARG_DOUBLE, sizeof(buf) - 1, buf, NULL);

    if (thd->variables.sql_mode & MODE_STRICT_ALL_TABLES)
    {
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, buf);
      return true;
    }
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE,
                        ER(ER_TRUNCATED_WRONG_VALUE), name, buf);
  }
  return false;
}

CHARSET_INFO *sys_var::charset(THD *thd)
{
  return is_os_charset ? thd->variables.character_set_filesystem :
    system_charset_info;
}

typedef struct old_names_map_st
{
  const char *old_name;
  const char *new_name;
} my_old_conv;

static my_old_conv old_conv[]=
{
  {     "cp1251_koi8"           ,       "cp1251"        },
  {     "cp1250_latin2"         ,       "cp1250"        },
  {     "kam_latin2"            ,       "keybcs2"       },
  {     "mac_latin2"            ,       "MacRoman"      },
  {     "macce_latin2"          ,       "MacCE"         },
  {     "pc2_latin2"            ,       "pclatin2"      },
  {     "vga_latin2"            ,       "pclatin1"      },
  {     "koi8_cp1251"           ,       "koi8r"         },
  {     "win1251ukr_koi8_ukr"   ,       "win1251ukr"    },
  {     "koi8_ukr_win1251ukr"   ,       "koi8u"         },
  {     NULL                    ,       NULL            }
};

CHARSET_INFO *get_old_charset_by_name(const char *name)
{
  my_old_conv *conv;

  for (conv= old_conv; conv->old_name; conv++)
  {
    if (!my_strcasecmp(&my_charset_latin1, name, conv->old_name))
      return get_charset_by_csname(conv->new_name, MY_CS_PRIMARY, MYF(0));
  }
  return NULL;
}

/****************************************************************************
  Main handling of variables:
  - Initialisation
  - Searching during parsing
  - Update loop
****************************************************************************/

/**
  Add variables to the dynamic hash of system variables

  @param first       Pointer to first system variable to add

  @retval
    0           SUCCESS
  @retval
    otherwise   FAILURE
*/


int mysql_add_sys_var_chain(sys_var *first)
{
  sys_var *var;

  /* A write lock should be held on LOCK_system_variables_hash */

  for (var= first; var; var= var->next)
  {
    /* this fails if there is a conflicting variable name. see HASH_UNIQUE */
    if (my_hash_insert(&system_variable_hash, (uchar*) var))
    {
      fprintf(stderr, "*** duplicate variable name '%s' ?\n", var->name.str);
      goto error;
    }
  }
  return 0;

error:
  for (; first != var; first= first->next)
    my_hash_delete(&system_variable_hash, (uchar*) first);
  return 1;
}


/*
  Remove variables to the dynamic hash of system variables

  SYNOPSIS
    mysql_del_sys_var_chain()
    first       Pointer to first system variable to remove

  RETURN VALUES
    0           SUCCESS
    otherwise   FAILURE
*/

int mysql_del_sys_var_chain(sys_var *first)
{
  int result= 0;

  /* A write lock should be held on LOCK_system_variables_hash */

  for (sys_var *var= first; var; var= var->next)
    result|= my_hash_delete(&system_variable_hash, (uchar*) var);

  return result;
}


static int show_cmp(SHOW_VAR *a, SHOW_VAR *b)
{
  return strcmp(a->name, b->name);
}


/**
  Constructs an array of system variables for display to the user.

  @param thd       current thread
  @param sorted    If TRUE, the system variables should be sorted
  @param type      OPT_GLOBAL or OPT_SESSION for SHOW GLOBAL|SESSION VARIABLES

  @retval
    pointer     Array of SHOW_VAR elements for display
  @retval
    NULL        FAILURE
*/

SHOW_VAR* enumerate_sys_vars(THD *thd, bool sorted, enum enum_var_type type)
{
  int count= system_variable_hash.records, i;
  int size= sizeof(SHOW_VAR) * (count + 1);
  SHOW_VAR *result= (SHOW_VAR*) thd->alloc(size);

  if (result)
  {
    SHOW_VAR *show= result;

    for (i= 0; i < count; i++)
    {
      sys_var *var= (sys_var*) my_hash_element(&system_variable_hash, i);

      // don't show session-only variables in SHOW GLOBAL VARIABLES
      if (type == OPT_GLOBAL && var->check_type(type))
        continue;

      show->name= var->name.str;
      show->value= (char*) var;
      show->type= SHOW_SYS;
      show++;
    }

    /* sort into order */
    if (sorted)
      my_qsort(result, show-result, sizeof(SHOW_VAR),
               (qsort_cmp) show_cmp);

    /* make last element empty */
    bzero(show, sizeof(SHOW_VAR));
  }
  return result;
}

/**
  Find a user set-table variable.

  @param str       Name of system variable to find
  @param length    Length of variable.  zero means that we should use strlen()
                   on the variable

  @retval
    pointer     pointer to variable definitions
  @retval
    0           Unknown variable (error message is given)
*/

sys_var *intern_find_sys_var(const char *str, uint length)
{
  sys_var *var;

  /*
    This function is only called from the sql_plugin.cc.
    A lock on LOCK_system_variable_hash should be held
  */
  var= (sys_var*) my_hash_search(&system_variable_hash,
                              (uchar*) str, length ? length : strlen(str));
  return var;
}


/**
  Execute update of all variables.

  First run a check of all variables that all updates will go ok.
  If yes, then execute all updates, returning an error if any one failed.

  This should ensure that in all normal cases none all or variables are
  updated.

  @param THD            Thread id
  @param var_list       List of variables to update

  @retval
    0   ok
  @retval
    1   ERROR, message sent (normally no variables was updated)
  @retval
    -1  ERROR, message not sent
*/

int sql_set_variables(THD *thd, List<set_var_base> *var_list)
{
  int error;
  List_iterator_fast<set_var_base> it(*var_list);
  DBUG_ENTER("sql_set_variables");

  set_var_base *var;
  while ((var=it++))
  {
    if ((error= var->check(thd)))
      goto err;
  }
  if (!(error= test(thd->is_error())))
  {
    it.rewind();
    while ((var= it++))
      error|= var->update(thd);         // Returns 0, -1 or 1
  }

err:
  free_underlaid_joins(thd, &thd->lex->select_lex);
  DBUG_RETURN(error);
}

/*****************************************************************************
  Functions to handle SET mysql_internal_variable=const_expr
*****************************************************************************/

/**
  Verify that the supplied value is correct.

  @param thd Thread handler

  @return status code
   @retval -1 Failure
   @retval 0 Success
 */

int set_var::check(THD *thd)
{
  if (var->is_readonly())
  {
    my_error(ER_INCORRECT_GLOBAL_LOCAL_VAR, MYF(0), var->name.str, "read only");
    return -1;
  }
  if (var->check_type(type))
  {
    int err= type == OPT_GLOBAL ? ER_LOCAL_VARIABLE : ER_GLOBAL_VARIABLE;
    my_error(err, MYF(0), var->name.str);
    return -1;
  }
  if ((type == OPT_GLOBAL && check_global_access(thd, SUPER_ACL)))
    return 1;
  /* value is a NULL pointer if we are using SET ... = DEFAULT */
  if (!value)
    return 0;

  if ((!value->fixed &&
       value->fix_fields(thd, &value)) || value->check_cols(1))
    return -1;
  if (var->check_update_type(value->result_type()))
  {
    my_error(ER_WRONG_TYPE_FOR_VAR, MYF(0), var->name.str);
    return -1;
  }
  return var->check(thd, this) ? -1 : 0;
}


/**
  Check variable, but without assigning value (used by PS).

  @param thd            thread handler

  @retval
    0   ok
  @retval
    1   ERROR, message sent (normally no variables was updated)
  @retval
    -1   ERROR, message not sent
*/
int set_var::light_check(THD *thd)
{
  if (var->check_type(type))
  {
    int err= type == OPT_GLOBAL ? ER_LOCAL_VARIABLE : ER_GLOBAL_VARIABLE;
    my_error(err, MYF(0), var->name);
    return -1;
  }
  if (type == OPT_GLOBAL && check_global_access(thd, SUPER_ACL))
    return 1;

  if (value && ((!value->fixed && value->fix_fields(thd, &value)) ||
                value->check_cols(1)))
    return -1;
  return 0;
}

/**
  Update variable

  @param   thd    thread handler
  @returns 0|1    ok or ERROR

  @note ERROR can be only due to abnormal operations involving
  the server's execution evironment such as
  out of memory, hard disk failure or the computer blows up.
  Consider set_var::check() method if there is a need to return
  an error due to logics.
*/
int set_var::update(THD *thd)
{
  return value ? var->update(thd, this) : var->set_default(thd, type);
}


/*****************************************************************************
  Functions to handle SET @user_variable=const_expr
*****************************************************************************/

int set_var_user::check(THD *thd)
{
  /*
    Item_func_set_user_var can't substitute something else on its place =>
    0 can be passed as last argument (reference on item)
  */
  return (user_var_item->fix_fields(thd, (Item**) 0) ||
          user_var_item->check(0)) ? -1 : 0;
}


/**
  Check variable, but without assigning value (used by PS).

  @param thd            thread handler

  @retval
    0   ok
  @retval
    1   ERROR, message sent (normally no variables was updated)
  @retval
    -1   ERROR, message not sent
*/
int set_var_user::light_check(THD *thd)
{
  /*
    Item_func_set_user_var can't substitute something else on its place =>
    0 can be passed as last argument (reference on item)
  */
  return (user_var_item->fix_fields(thd, (Item**) 0));
}


int set_var_user::update(THD *thd)
{
  if (user_var_item->update())
  {
    /* Give an error if it's not given already */
    my_message(ER_SET_CONSTANTS_ONLY, ER(ER_SET_CONSTANTS_ONLY), MYF(0));
    return -1;
  }
  return 0;
}


/*****************************************************************************
  Functions to handle SET PASSWORD
*****************************************************************************/

int set_var_password::check(THD *thd)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (!user->host.str)
  {
    DBUG_ASSERT(thd->security_ctx->priv_host);
    if (*thd->security_ctx->priv_host != 0)
    {
      user->host.str= (char *) thd->security_ctx->priv_host;
      user->host.length= strlen(thd->security_ctx->priv_host);
    }
    else
    {
      user->host.str= (char *)"%";
      user->host.length= 1;
    }
  }
  if (!user->user.str)
  {
    DBUG_ASSERT(thd->security_ctx->user);
    user->user.str= (char *) thd->security_ctx->user;
    user->user.length= strlen(thd->security_ctx->user);
  }
  /* Returns 1 as the function sends error to client */
  return check_change_password(thd, user->host.str, user->user.str,
                               password, strlen(password)) ? 1 : 0;
#else
  return 0;
#endif
}

int set_var_password::update(THD *thd)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  /* Returns 1 as the function sends error to client */
  return change_password(thd, user->host.str, user->user.str, password) ?
          1 : 0;
#else
  return 0;
#endif
}

/*****************************************************************************
  Functions to handle SET NAMES and SET CHARACTER SET
*****************************************************************************/

int set_var_collation_client::check(THD *thd)
{
  /* Currently, UCS-2 cannot be used as a client character set */
  if (!is_supported_parser_charset(character_set_client))
  {
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), "character_set_client",
             character_set_client->csname);
    return 1;
  }
  return 0;
}

int set_var_collation_client::update(THD *thd)
{
  thd->variables.character_set_client= character_set_client;
  thd->variables.character_set_results= character_set_results;
  thd->variables.collation_connection= collation_connection;
  thd->update_charset();
  thd->protocol_text.init(thd);
  thd->protocol_binary.init(thd);
  return 0;
}

