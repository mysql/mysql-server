/* Copyright (c) 2002, 2017, Oracle and/or its affiliates. All rights reserved.

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

/* variable declarations are in sys_vars.cc now !!! */

#include "set_var.h"

#include "hash.h"                // HASH
#include "auth_common.h"         // SUPER_ACL
#include "log.h"                 // sql_print_warning
#include "mysqld.h"              // system_charset_info
#include "sql_class.h"           // THD
#include "sql_parse.h"           // is_supported_parser_charset
#include "sql_select.h"          // free_underlaid_joins
#include "sql_show.h"            // append_identifier
#include "sys_vars_shared.h"     // PolyLock_mutex
#include "sql_audit.h"           // mysql_audit

static HASH system_variable_hash;
static PolyLock_mutex PLock_global_system_variables(&LOCK_global_system_variables);
ulonglong system_variable_hash_version= 0;

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
                   0, (my_hash_get_key) get_sys_var_length, 0, HASH_UNIQUE,
                   PSI_INSTRUMENT_ME))
    goto error;

  if (mysql_add_sys_var_chain(all_sys_vars.first))
    goto error;

  DBUG_RETURN(0);

error:
  my_message_local(ERROR_LEVEL, "failed to initialize system variables");
  DBUG_RETURN(1);
}

int sys_var_add_options(std::vector<my_option> *long_options, int parse_flags)
{
  DBUG_ENTER("sys_var_add_options");

  for (sys_var *var=all_sys_vars.first; var; var= var->next)
  {
    if (var->register_option(long_options, parse_flags))
      goto error;
  }

  DBUG_RETURN(0);

error:
  my_message_local(ERROR_LEVEL, "failed to initialize system variables");
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
  @param substitute If non-NULL, this variable is deprecated and the
  string describes what one should use instead. If an empty string,
  the variable is deprecated but no replacement is offered.
  @param parse_flag either PARSE_EARLY or PARSE_NORMAL
*/
sys_var::sys_var(sys_var_chain *chain, const char *name_arg,
                 const char *comment, int flags_arg, ptrdiff_t off,
                 int getopt_id, enum get_opt_arg_type getopt_arg_type,
                 SHOW_TYPE show_val_type_arg, longlong def_val,
                 PolyLock *lock, enum binlog_status_enum binlog_status_arg,
                 on_check_function on_check_func,
                 on_update_function on_update_func,
                 const char *substitute, int parse_flag) :
  next(0),
  binlog_status(binlog_status_arg),
  flags(flags_arg), m_parse_flag(parse_flag), show_val_type(show_val_type_arg),
  guard(lock), offset(off), on_check(on_check_func), on_update(on_update_func),
  deprecation_substitute(substitute),
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

  memset(&option, 0, sizeof(option));
  option.name= name_arg;
  option.id= getopt_id;
  option.comment= comment;
  option.arg_type= getopt_arg_type;
  option.value= (uchar **)global_var_ptr();
  option.def_value= def_val;

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
  {
    bool locked= false;
    if (!show_compatibility_56)
    {
      /* Block reads from other threads. */
      mysql_mutex_lock(&thd->LOCK_thd_sysvar);
      locked= true;
    }
  
    bool ret= session_update(thd, var) ||
      (on_update && on_update(this, thd, OPT_SESSION));
  
    if (locked)
      mysql_mutex_unlock(&thd->LOCK_thd_sysvar);

    /*
      Make sure we don't session-track variables that are not actually
      part of the session. tx_isolation and and tx_read_only for example
      exist as GLOBAL, SESSION, and one-shot ("for next transaction only").
    */
    if ((var->type == OPT_SESSION) || !is_trilevel())
    {
      if ((!ret) &&
          thd->session_tracker.get_tracker(SESSION_SYSVARS_TRACKER)->is_enabled())
        thd->session_tracker.get_tracker(SESSION_SYSVARS_TRACKER)->mark_as_changed(thd, &(var->var->name));

      if ((!ret) &&
          thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)->is_enabled())
        thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)->mark_as_changed(thd, &var->var->name);
    }

    return ret;
  }
}

uchar *sys_var::session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
{
  return session_var_ptr(target_thd);
}

uchar *sys_var::global_value_ptr(THD *thd, LEX_STRING *base)
{
  return global_var_ptr();
}

uchar *sys_var::session_var_ptr(THD *thd)
{ return ((uchar*)&(thd->variables)) + offset; }

uchar *sys_var::global_var_ptr()
{ return ((uchar*)&global_system_variables) + offset; }


bool sys_var::check(THD *thd, set_var *var)
{
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

uchar *sys_var::value_ptr(THD *running_thd, THD *target_thd, enum_var_type type, LEX_STRING *base)
{
  if (type == OPT_GLOBAL || scope() == GLOBAL)
  {
    mysql_mutex_assert_owner(&LOCK_global_system_variables);
    AutoRLock lock(guard);
    return global_value_ptr(running_thd, base);
  }
  else
    return session_value_ptr(running_thd, target_thd, base);
}

uchar *sys_var::value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
{
  return value_ptr(thd, thd, type, base);
}

bool sys_var::set_default(THD *thd, set_var* var)
{
  DBUG_ENTER("sys_var::set_default");
  if (var->type == OPT_GLOBAL || scope() == GLOBAL)
    global_save_default(thd, var);
  else
    session_save_default(thd, var);

  bool ret= check(thd, var) || update(thd, var);
  DBUG_RETURN(ret);
}

void sys_var::do_deprecated_warning(THD *thd)
{
  if (deprecation_substitute != NULL)
  {
    char buf1[NAME_CHAR_LEN + 3];
    strxnmov(buf1, sizeof(buf1)-1, "@@", name.str, 0);

    /* 
       if deprecation_substitute is an empty string,
       there is no replacement for the syntax
    */
    uint errmsg= deprecation_substitute[0] == '\0'
      ? ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT
      : ER_WARN_DEPRECATED_SYNTAX;
    if (thd)
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_WARN_DEPRECATED_SYNTAX, ER(errmsg),
                          buf1, deprecation_substitute);
    else
      sql_print_warning(ER_DEFAULT(errmsg), buf1, deprecation_substitute);
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
  if (fixed)
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
    push_warning_printf(thd, Sql_condition::SL_WARNING,
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

    my_gcvt(v, MY_GCVT_ARG_DOUBLE, static_cast<int>(sizeof(buf)) - 1, buf, NULL);

    if (thd->variables.sql_mode & MODE_STRICT_ALL_TABLES)
    {
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, buf);
      return true;
    }
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_TRUNCATED_WRONG_VALUE,
                        ER(ER_TRUNCATED_WRONG_VALUE), name, buf);
  }
  return false;
}

const CHARSET_INFO *sys_var::charset(THD *thd)
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

const CHARSET_INFO *get_old_charset_by_name(const char *name)
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
      my_message_local(ERROR_LEVEL, "duplicate variable name '%s'!?",
                       var->name.str);
      goto error;
    }
  }

  /* Update system_variable_hash version. */
  system_variable_hash_version++;
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

  /* Update system_variable_hash version. */
  system_variable_hash_version++;

  return result;
}

/*
  Comparison function for std::sort.
  @param a  SHOW_VAR element
  @param b  SHOW_VAR element
  
  @retval
    True if a < b.
  @retval
    False if a >= b.
*/
static int show_cmp(const void *a, const void *b)
{
  return strcmp(((SHOW_VAR *)a)->name, ((SHOW_VAR *)b)->name);
}

/*
  Number of records in the system_variable_hash.
  Requires lock on LOCK_system_variables_hash.
*/
ulong get_system_variable_hash_records(void)
{
  return (system_variable_hash.records);
}

/*
  Current version of the system_variable_hash.
  Requires lock on LOCK_system_variables_hash.
*/
ulonglong get_system_variable_hash_version(void)
{
  return (system_variable_hash_version);
}

/**
  Constructs an array of system variables for display to the user.

  @param thd            Current thread
  @param show_var_array Prealloced_array of SHOW_VAR elements for display 
  @param sort           If TRUE, the system variables should be sorted
  @param query_scope    OPT_GLOBAL or OPT_SESSION for SHOW GLOBAL|SESSION VARIABLES
  @param strict         Use strict scope checking
  @retval               True on error, false otherwise
*/
bool enumerate_sys_vars(THD *thd, Show_var_array *show_var_array,
                        bool sort,
                        enum enum_var_type query_scope,
                        bool strict)
{
  DBUG_ASSERT(show_var_array != NULL);
  DBUG_ASSERT(query_scope == OPT_SESSION || query_scope == OPT_GLOBAL);
  int count= system_variable_hash.records;
  
  /* Resize array if necessary. */
  if (show_var_array->reserve(count+1))
    return true;

  if (show_var_array)
  {
    for (int i= 0; i < count; i++)
    {
      sys_var *sysvar= (sys_var*) my_hash_element(&system_variable_hash, i);

      if (strict)
      {
        /*
          Strict scope match (5.7). Success if this is a:
            - global query and the variable scope is GLOBAL or SESSION, OR
            - session query and the variable scope is SESSION or ONLY_SESSION.
        */
        if (!sysvar->check_scope(query_scope))
          continue;
      }
      else
      {
        /*
          Non-strict scope match (5.6). Success if this is a:
            - global query and the variable scope is GLOBAL or SESSION, OR
            - session query and the variable scope is GLOBAL, SESSION or ONLY_SESSION.
        */
        if (query_scope == OPT_GLOBAL && !sysvar->check_scope(query_scope))
          continue;
      }

      /* Don't show non-visible variables. */
      if (sysvar->not_visible())
        continue;

      SHOW_VAR show_var;
      show_var.name=  sysvar->name.str;
      show_var.value= (char*)sysvar;
      show_var.type=  SHOW_SYS;
      show_var.scope= SHOW_SCOPE_UNDEF; /* not used for sys vars */
      show_var_array->push_back(show_var);
    }

    if (sort)
      std::qsort(show_var_array->begin(), show_var_array->size(),
                 show_var_array->element_size(), show_cmp);

    /* Make last element empty. */
    show_var_array->push_back(st_mysql_show_var());
  }
  
  return false;
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

sys_var *intern_find_sys_var(const char *str, size_t length)
{
  sys_var *var;

  /*
    This function is only called from the sql_plugin.cc.
    A lock on LOCK_system_variable_hash should be held
  */
  var= (sys_var*) my_hash_search(&system_variable_hash,
                              (uchar*) str, length ? length : strlen(str));

  /* Don't show non-visible variables. */
  if (var && var->not_visible())
    return NULL;

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
  if (!(error= MY_TEST(thd->is_error())))
  {
    it.rewind();
    while ((var= it++))
      error|= var->update(thd);         // Returns 0, -1 or 1
  }

err:
  free_underlaid_joins(thd, thd->lex->select_lex);
  DBUG_RETURN(error);
}

/**
  This function is used to check if key management UDFs like
  keying_key_generate/store/remove should proceed or not. If global
  variable @@keyring_operations is OFF then above said udfs will fail.

  @return Operation status
    @retval 0 OK
    @retval 1 ERROR, keyring operations are not allowed

  @sa Sys_keyring_operations
*/
bool keyring_access_test()
{
  bool keyring_operations;
  mysql_mutex_lock(&LOCK_keyring_operations);
  keyring_operations= !opt_keyring_operations;
  mysql_mutex_unlock(&LOCK_keyring_operations);
  return keyring_operations;
}

/*****************************************************************************
  Functions to handle SET mysql_internal_variable=const_expr
*****************************************************************************/

set_var::set_var(enum_var_type type_arg, sys_var *var_arg,
                 const LEX_STRING *base_name_arg, Item *value_arg)
  :var(var_arg), type(type_arg), base(*base_name_arg)
{
  /*
    If the set value is a field, change it to a string to allow things like
    SET table_type=MYISAM;
  */
  if (value_arg && value_arg->type() == Item::FIELD_ITEM)
  {
    Item_field *item= (Item_field*) value_arg;
    if (item->field_name)
    {
      if (!(value= new Item_string(item->field_name,
                                   strlen(item->field_name),
                                   system_charset_info))) // names are utf8
        value= value_arg;			/* Give error message later */
    }
    else
    {
      /* Both Item_field and Item_insert_value will return the type as
         Item::FIELD_ITEM. If the item->field_name is NULL, we assume the
         object to be Item_insert_value. */
      value= value_arg;
    }
  }
  else
    value= value_arg;
}

/**
  Verify that the supplied value is correct.

  @param thd Thread handler

  @return status code
   @retval -1 Failure
   @retval 0 Success
 */

int set_var::check(THD *thd)
{
  DBUG_ENTER("set_var::check");
  var->do_deprecated_warning(thd);
  if (var->is_readonly())
  {
    my_error(ER_INCORRECT_GLOBAL_LOCAL_VAR, MYF(0), var->name.str, "read only");
    DBUG_RETURN(-1);
  }
  if (!var->check_scope(type))
  {
    int err= type == OPT_GLOBAL ? ER_LOCAL_VARIABLE : ER_GLOBAL_VARIABLE;
    my_error(err, MYF(0), var->name.str);
    DBUG_RETURN(-1);
  }
  if ((type == OPT_GLOBAL && check_global_access(thd, SUPER_ACL)))
    DBUG_RETURN(1);
  /* value is a NULL pointer if we are using SET ... = DEFAULT */
  if (!value)
    DBUG_RETURN(0);

  if ((!value->fixed &&
       value->fix_fields(thd, &value)) || value->check_cols(1))
    DBUG_RETURN(-1);
  if (var->check_update_type(value->result_type()))
  {
    my_error(ER_WRONG_TYPE_FOR_VAR, MYF(0), var->name.str);
    DBUG_RETURN(-1);
  }
  int ret= var->check(thd, this) ? -1 : 0;

#ifndef EMBEDDED_LIBRARY
  if (!ret && type == OPT_GLOBAL)
  {
    ret= mysql_audit_notify(thd, AUDIT_EVENT(MYSQL_AUDIT_GLOBAL_VARIABLE_SET),
                            var->name.str,
                            value->item_name.ptr(),
                            value->item_name.length());
  }
#endif

  DBUG_RETURN(ret);
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
  if (!var->check_scope(type))
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
  return value ? var->update(thd, this) : var->set_default(thd, this);
}

/**
  Self-print assignment

  @param   str    string buffer to append the partial assignment to
*/
void set_var::print(THD *thd, String *str)
{
  str->append(type == OPT_GLOBAL ? "GLOBAL " : "SESSION ");
  if (base.length)
  {
    str->append(base.str, base.length);
    str->append(STRING_WITH_LEN("."));
  }
  str->append(var->name.str,var->name.length);
  str->append(STRING_WITH_LEN("="));
  if (value)
    value->print(str, QT_ORDINARY);
  else
    str->append(STRING_WITH_LEN("DEFAULT"));
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
  if (thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)->is_enabled())
    thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)->mark_as_changed(thd, NULL);
  return 0;
}


void set_var_user::print(THD *thd, String *str)
{
  user_var_item->print_assignment(str, QT_ORDINARY);
}


/*****************************************************************************
  Functions to handle SET PASSWORD
*****************************************************************************/

/**
  Check the validity of the SET PASSWORD request

  @param  thd  The current thread
  @return      status code
  @retval 0    failure
  @retval 1    success
*/
int set_var_password::check(THD *thd)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
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

void set_var_password::print(THD *thd, String *str)
{
  if (user->user.str != NULL && user->user.length > 0)
  {
    str->append(STRING_WITH_LEN("PASSWORD FOR "));
    append_identifier(thd, str, user->user.str, user->user.length);
    if (user->host.str != NULL && user->host.length > 0)
    {
      str->append(STRING_WITH_LEN("@"));
      append_identifier(thd, str, user->host.str, user->host.length);
    }
    str->append(STRING_WITH_LEN("="));
  }
  else
    str->append(STRING_WITH_LEN("PASSWORD FOR CURRENT_USER()="));
  str->append(STRING_WITH_LEN("<secret>"));
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

  /* Mark client collation variables as changed */
  if (thd->session_tracker.get_tracker(SESSION_SYSVARS_TRACKER)->is_enabled())
  {
    LEX_CSTRING cs_client= {"character_set_client",
                            sizeof("character_set_client") - 1};
    thd->session_tracker.get_tracker(SESSION_SYSVARS_TRACKER)->mark_as_changed(thd, &cs_client);
    LEX_CSTRING cs_results= {"character_set_results",
                             sizeof("character_set_results") -1};
    thd->session_tracker.get_tracker(SESSION_SYSVARS_TRACKER)->mark_as_changed(thd, &cs_results);
    LEX_CSTRING cs_connection= {"character_set_connection",
                                sizeof("character_set_connection") - 1};
    thd->session_tracker.get_tracker(SESSION_SYSVARS_TRACKER)->mark_as_changed(thd, &cs_connection);
  }
  if (thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)->is_enabled())
    thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)->mark_as_changed(thd, NULL);
  thd->protocol_text.init(thd);
  thd->protocol_binary.init(thd);
  return 0;
}

void set_var_collation_client::print(THD *thd, String *str)
{
  str->append((set_cs_flags & SET_CS_NAMES) ? "NAMES " : "CHARACTER SET ");
  if (set_cs_flags & SET_CS_DEFAULT)
    str->append("DEFAULT");
  else
  {
    str->append("'");
    str->append(character_set_client->csname);
    str->append("'");
    if (set_cs_flags & SET_CS_COLLATE)
    {
      str->append(" COLLATE '");
      str->append(collation_connection->name);
      str->append("'");
    }
  }
}
