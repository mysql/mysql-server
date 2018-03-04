/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/persisted_variable.h"

#include "my_config.h"

#include <algorithm>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <memory>
#include <new>
#include <utility>

#include "lex_string.h"
#include "m_ctype.h"
#include "m_string.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_default.h"                 // check_file_permissions
#include "my_getopt.h"
#include "my_io.h"
#include "my_loglevel.h"
#include "my_macros.h"
#include "my_sys.h"
#include "my_thread.h"
#include "mysql/components/services/log_shared.h"
#include "mysql/components/services/psi_file_bits.h"
#include "mysql/components/services/psi_memory_bits.h"
#include "mysql/components/services/psi_mutex_bits.h"
#include "mysql/plugin.h"
#include "mysql/psi/mysql_memory.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/psi_base.h"
#include "mysql/udf_registration_types.h"
#include "mysql_version.h"
#include "mysqld_error.h"
#include "pfs_mutex_provider.h"
#include "prealloced_array.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/current_thd.h"
#include "sql/derror.h"       // ER_THD
#include "sql/item.h"
#include "sql/json_dom.h"
#include "sql/log.h"
#include "sql/mysqld.h"
#include "sql/psi_memory_key.h"
#include "sql/set_var.h"
#include "sql/sql_class.h"
#include "sql/sql_error.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_servers.h"
#include "sql/sql_show.h"
#include "sql/sql_table.h"
#include "sql/sys_vars_shared.h"
#include "sql_string.h"
#include "thr_mutex.h"
#include "typelib.h"

PSI_file_key key_persist_file_cnf;

#ifdef HAVE_PSI_FILE_INTERFACE
static PSI_file_info all_persist_files[]=
{
  { &key_persist_file_cnf, "cnf", 0, 0, PSI_DOCUMENT_ME}
};
#endif /* HAVE_PSI_FILE_INTERFACE */

PSI_mutex_key key_persist_file, key_persist_variables;

#ifdef HAVE_PSI_MUTEX_INTERFACE
static PSI_mutex_info all_persist_mutexes[]=
{
  { &key_persist_file, "m_LOCK_persist_file", 0, 0, PSI_DOCUMENT_ME},
  { &key_persist_variables, "m_LOCK_persist_variables", 0, 0, PSI_DOCUMENT_ME}
};
#endif /* HAVE_PSI_MUTEX_INTERFACE */

PSI_memory_key key_memory_persisted_variables;

#ifdef HAVE_PSI_MEMORY_INTERFACE
static PSI_memory_info all_options[]=
{
  {&key_memory_persisted_variables, "persisted_options_root", 0, PSI_FLAG_ONLY_GLOBAL_STAT, PSI_DOCUMENT_ME}
};
#endif /* HAVE_PSI_MEMORY_INTERFACE */

#ifdef HAVE_PSI_INTERFACE
void my_init_persist_psi_keys(void)
{
  const char* category MY_ATTRIBUTE((unused))= "persist";
  int count MY_ATTRIBUTE((unused));

#ifdef HAVE_PSI_FILE_INTERFACE
  count= sizeof(all_persist_files)/sizeof(all_persist_files[0]);
  mysql_file_register(category, all_persist_files, count);
#endif

#ifdef HAVE_PSI_MUTEX_INTERFACE
  count= static_cast<int>(array_elements(all_persist_mutexes));
  mysql_mutex_register(category, all_persist_mutexes, count);
#endif

#ifdef HAVE_PSI_MEMORY_INTERFACE
  count= static_cast<int>(array_elements(all_options));
  mysql_memory_register(category, all_options, count);
#endif
}
#endif

Persisted_variables_cache* Persisted_variables_cache::m_instance= NULL;

/**
  Initialize class members. This function reads datadir if present in
  config file or set at command line, in order to know from where to
  load this config file. If datadir is not set then read from MYSQL_DATADIR.

   @param [in] argc                      Pointer to argc of original program
   @param [in] argv                      Pointer to argv of original program

   @return 0 Success
   @return 1 Failure

*/
int Persisted_variables_cache::init(int *argc, char ***argv)
{
#ifdef HAVE_PSI_INTERFACE
   my_init_persist_psi_keys();
#endif

  int temp_argc= *argc;
  MEM_ROOT alloc;
  char *ptr, **res, *datadir= NULL;
  char dir[FN_REFLEN]= { 0 };
  const char *dirs= NULL;
  bool persist_load= true;

  my_option persist_options[]= {
    {"persisted_globals_load", 0, "", &persist_load, &persist_load,
      0, GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},
    {"datadir", 0, "", &datadir, 0, 0, GET_STR, OPT_ARG,
      0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
  };

  /* create temporary args list and pass it to handle_options */
  init_alloc_root(key_memory_persisted_variables, &alloc, 512, 0);
  if (!(ptr= (char *) alloc_root(&alloc, sizeof(alloc) +
    (*argc + 1) * sizeof(char *))))
    return 1;
  memset(ptr, 0, (sizeof(char *) * (*argc + 1)));
  res= (char **) (ptr);
  memcpy((uchar *) res, (char *) (*argv), (*argc) * sizeof(char *));

  my_getopt_skip_unknown= TRUE;
  if (my_handle_options(&temp_argc, &res, persist_options,
                        NULL, NULL, TRUE))
  {
    free_root(&alloc, MYF(0));
    return 1;
  }
  my_getopt_skip_unknown= 0;
  free_root(&alloc, MYF(0));

  persisted_globals_load= persist_load;
  /*
    if datadir is set then search in this data dir else search in
    MYSQL_DATADIR
  */
  dirs= ((datadir) ? datadir : MYSQL_DATADIR);
  /* expand path if it is a relative path */
  if (dirs[0] == FN_CURLIB && my_getwd(dir, sizeof(dir), MYF(0)))
    return 1;
  if (fn_format(datadir_buffer, dirs, dir, "",
      MY_UNPACK_FILENAME | MY_SAFE_PATH | MY_RELATIVE_PATH) == NULL)
    return 1;
  unpack_dirname(datadir_buffer, datadir_buffer);
  m_persist_filename= string(datadir_buffer) + MYSQL_PERSIST_CONFIG_NAME + ".cnf";

  mysql_mutex_init(key_persist_variables,
    &m_LOCK_persist_variables, MY_MUTEX_INIT_FAST);

  mysql_mutex_init(key_persist_file,
    &m_LOCK_persist_file, MY_MUTEX_INIT_FAST);

  m_instance= this;
  ro_persisted_argv= NULL;
  ro_persisted_plugin_argv= NULL;
  return 0;
}

/**
  Return a singleton object
*/
Persisted_variables_cache* Persisted_variables_cache::get_instance()
{
  DBUG_ASSERT(m_instance != NULL);
  return m_instance;
}

/**
  Retrieve variables name/value and update the in-memory copy with
  this new values. If value is default then remove this entry from
  in-memory copy, else update existing key with new value

   @param [in] thd           Pointer to connection handler
   @param [in] setvar        Pointer to set_var which is being SET

   @return void
*/
void Persisted_variables_cache::set_variable(THD *thd, set_var *setvar)
{
  char val_buf[1024]= { 0 };
  String str(val_buf, sizeof(val_buf), system_charset_info), *res;
  String utf8_str;

  struct st_persist_var tmp_var;
  sys_var *system_var= setvar->var;

  const char* var_name=
    Persisted_variables_cache::get_variable_name(system_var);
  const char* var_value= val_buf;
  if (setvar->type == OPT_PERSIST_ONLY && setvar->value)
  {
    res= setvar->value->val_str(&str);
    if (res && res->length())
    {
      /*
        value held by Item class can be of different charset,
        so convert to utf8mb4
      */
      const CHARSET_INFO *tocs= &my_charset_utf8mb4_bin;
      uint dummy_err;
      utf8_str.copy(res->ptr(), res->length(), res->charset(), tocs,
        &dummy_err);
      var_value= utf8_str.c_ptr_quick();
    }
  }
  else
  {
    Persisted_variables_cache::get_variable_value(thd, system_var, &utf8_str);
    var_value= utf8_str.c_ptr_quick();
  }

  /* structured variables may have basename if specified */
  tmp_var.key= (setvar->base.str ?
                string(setvar->base.str).append(".").append(var_name) :
                string(var_name));
  tmp_var.value= var_value;

  /* modification to in-memory must be thread safe */
  mysql_mutex_lock(&m_LOCK_persist_variables);
  /* if present update variable with new value else insert into hash */
  if ((setvar->type == OPT_PERSIST_ONLY && setvar->var->is_readonly()) ||
    setvar->var->is_plugin_var_read_only())
    m_persist_ro_variables[tmp_var.key]= tmp_var.value;
  else
  {
    /*
     if element is present remove from current position and insert
     at end of vector to restore insertion order.
    */
    string str= tmp_var.key;
    auto it= std::find_if(m_persist_variables.begin(), m_persist_variables.end(),
      [str](st_persist_var const& s) { return s.key == str; });
    if (it != m_persist_variables.end())
      m_persist_variables.erase(it);
    m_persist_variables.push_back(tmp_var);
  }
  mysql_mutex_unlock(&m_LOCK_persist_variables);
}

/**
  Retrieve variables value from sys_var

   @param [in] thd           Pointer to connection handler
   @param [in] system_var    Pointer to sys_var which is being SET
   @param [in] str           Pointer to String instance into which value
                             is copied

   @return
     Pointer to String instance holding the value
*/
String* Persisted_variables_cache::get_variable_value(THD *thd,
  sys_var *system_var, String *str)
{
  const char* value;
  char val_buf[1024];
  size_t val_length;
  char show_var_buffer[sizeof(SHOW_VAR)];
  SHOW_VAR *show= (SHOW_VAR *)show_var_buffer;
  const CHARSET_INFO *fromcs;
  const CHARSET_INFO *tocs= &my_charset_utf8mb4_bin;
  uint dummy_err;

  show->type= SHOW_SYS;
  show->name= system_var->name.str;
  show->value= (char *) system_var;

  mysql_mutex_lock(&LOCK_global_system_variables);
  value= get_one_variable(thd, show, OPT_GLOBAL, show->type, NULL,
                          &fromcs, val_buf, &val_length);
  mysql_mutex_unlock(&LOCK_global_system_variables);

  /* convert the retrieved value to utf8mb4 */
  str->copy(value, val_length, fromcs, tocs, &dummy_err);
  return str;
}

/**
  Retrieve variables name from sys_var

   @param [in] system_var    Pointer to sys_var which is being SET
   @return
     Pointer to buffer holding the name
*/
const char* Persisted_variables_cache::get_variable_name(sys_var *system_var)
{
  return system_var->name.str;
}

/**
  Convert in-memory copy into a stream of characters and write this
  stream to persisted config file

  @return Error state
    @retval TRUE An error occurred
    @retval FALSE Success
*/
bool Persisted_variables_cache::flush_to_file()
{
  mysql_mutex_lock(&m_LOCK_persist_variables);
  mysql_mutex_lock(&m_LOCK_persist_file);

  /* Construct json formatted string buffer */
  String dest("{ \"mysql_server\": {", &my_charset_utf8mb4_bin);

  for (auto iter= m_persist_variables.begin();
       iter != m_persist_variables.end(); iter++)
  {
    String str;
    string s(iter->key);
    std::unique_ptr<Json_string> var_name(new (std::nothrow)
                    Json_string(s));
    Json_wrapper vn(var_name.release());
    /*
      Convert variable name to a json quoted string. This function will
      take care of all quotes and charsets.
    */
    vn.to_string(&str, true, String().ptr());
    dest.append(str);
    dest.append(": ");
    /* reset str */
    str= String();
    std::unique_ptr<Json_string> var_val(new (std::nothrow)
                    Json_string(iter->value));
    Json_wrapper vv(var_val.release());
    vv.to_string(&str, true, String().ptr());
    dest.append(str);
    dest.append(" , ");
  }

  if (m_persist_ro_variables.size())
  {
    dest.append("\"mysql_server_static_options\": {");
  }

  for (auto iter = m_persist_ro_variables.begin();
       iter != m_persist_ro_variables.end(); iter++)
  {
    String str;
    std::unique_ptr<Json_string> var_name(new (std::nothrow)
                    Json_string(iter->first));
    Json_wrapper vn(var_name.release());
    vn.to_string(&str, true, String().ptr());
    dest.append(str);
    dest.append(": ");
    str= String();
    std::unique_ptr<Json_string> var_val(new (std::nothrow)
                    Json_string(iter->second));
    Json_wrapper vv(var_val.release());
    vv.to_string(&str, true, String().ptr());
    dest.append(str);
    dest.append(" , ");
  }

  if (m_persist_ro_variables.size())
  {
    /* remove last " , " characters */
    dest.chop(); dest.chop(); dest.chop();
    dest.append(" }");
  }
  if (m_persist_variables.size() && !m_persist_ro_variables.size())
  {
    dest.chop(); dest.chop(); dest.chop();
  }
  dest.append(" } }");
  /*
    If file does not exists create one. When persisted_globals_load is 0
    we dont read contents of mysqld-auto.cnf file, thus append any new
    variables which are persisted to this file.
  */
  bool ret= false;

  if(open_persist_file(O_CREAT | O_WRONLY))
  {
    ret= true;
  }
  else
  {
    /* write to file */
    if ((mysql_file_fputs(dest.c_ptr(), m_fd)) < 0)
    {
      ret= true;
    }
  }

  close_persist_file();
  mysql_mutex_unlock(&m_LOCK_persist_file);
  mysql_mutex_unlock(&m_LOCK_persist_variables);
  return ret;
}

/**
  Open persisted config file

  @param [in] flag    File open mode
  @return Error state
    @retval TRUE An error occurred
    @retval FALSE Success
*/
bool Persisted_variables_cache::open_persist_file(int flag)
{
  m_fd= mysql_file_fopen(key_persist_file_cnf,
                         m_persist_filename.c_str(), flag, MYF(0));
  return (m_fd ? 0 : 1);
}

/**
  Close persisted config file
   @return void
*/
void Persisted_variables_cache::close_persist_file()
{
  mysql_file_fclose(m_fd, MYF(0));
  m_fd= NULL;
}

/**
  load_persist_file() read persisted config file

  @return Error state
    @retval TRUE An error occurred
    @retval FALSE Success
*/
bool Persisted_variables_cache::load_persist_file()
{
  if (read_persist_file() > 0)
    return 1;
  return 0;
}

/**
  set_persist_options() will set the options read from persisted config file

  This function does nothing when --no-defaults is set or if
  persisted_globals_load is set to false

   @param [in] plugin_options      Flag which tells what options are being set.
                                   If set to FALSE non plugin variables are set
                                   else plugin variables are set

  @return Error state
    @retval TRUE An error occurred
    @retval FALSE Success
*/
bool Persisted_variables_cache::set_persist_options(bool plugin_options)
{
  THD *thd;
  LEX lex_tmp, *sav_lex= NULL;
  List<set_var_base> tmp_var_list;
  vector<st_persist_var> *persist_variables= NULL;
  List_iterator_fast<set_var_base> it(tmp_var_list);
  set_var_base *var;
  ulong access= 0;
  bool result= 0, new_thd= 0;

  /*
    if persisted_globals_load is set to false or --no-defaults is set
    then do not set persistent options
  */
  if (no_defaults || !persisted_globals_load)
    return 0;

  /*
    This function is called in only 2 places
      1. During server startup.
      2. During install plugin after server has started.
    During server startup before server components are initialized
    current_thd is NULL thus instantiate new temporary THD.
    After server has started we have current_thd so make use of current_thd.
  */
  if (current_thd)
  {
    thd= current_thd;
    sav_lex= thd->lex;
    thd->lex= &lex_tmp;
    lex_start(thd);
  }
  else
  {
    if (!(thd= new THD))
    {
      LogErr(ERROR_LEVEL, ER_FAILED_TO_SET_PERSISTED_OPTIONS);
      return 1;
    }
    thd->thread_stack= (char*) &thd;
    thd->set_new_thread_id();
    thd->store_globals();
    lex_start(thd);
    /* save access privileges */
    access= thd->security_context()->master_access();
    thd->security_context()->set_master_access(~(ulong)0);
    thd->real_id= my_thread_self();
    new_thd= 1;
  }

  /*
    Based on plugin_options, we decide on what options to be set. If
    plugin_options is false we set all non plugin variables and then
    keep all plugin variables in a map. When the plugin is installed
    plugin variables are read from the map and set.
  */
  persist_variables= (plugin_options ? &m_persist_plugin_variables:
                                       &m_persist_variables);

  for (auto iter= persist_variables->begin();
       iter != persist_variables->end(); iter++)
  {
    Item *res= NULL;
    set_var *var= NULL;
    sys_var *sysvar= NULL;
    string var_name= iter->key;

    LEX_STRING base_name= { const_cast<char*>(var_name.c_str()),
                            var_name.length() };

    sysvar= intern_find_sys_var(var_name.c_str(), var_name.length());
    if (sysvar == NULL)
    {
      /*
        for plugin variables we report a warning in error log,
        keep track of this variable so that it is set when plugin
        is loaded and continue with remaining persisted variables
      */
      m_persist_plugin_variables.push_back(*iter);
      my_message_local(WARNING_LEVEL, "Currently unknown variable '%s'"
                       "was read from the persisted config file",
                       var_name.c_str());
      continue;
    }
    switch (sysvar->show_type())
    {
    case SHOW_INT:
    case SHOW_LONG:
    case SHOW_SIGNED_LONG:
    case SHOW_LONGLONG:
    case SHOW_HA_ROWS:
      res= new (thd->mem_root) Item_uint(iter->value.c_str(),
                                         (uint)iter->value.length());
    break;
    case SHOW_CHAR:
    case SHOW_CHAR_PTR:
    case SHOW_LEX_STRING:
    case SHOW_BOOL:
    case SHOW_MY_BOOL:
      res= new (thd->mem_root) Item_string(iter->value.c_str(),
                                            iter->value.length(),
                                            &my_charset_utf8mb4_bin);
    break;
    case SHOW_DOUBLE:
      res= new (thd->mem_root) Item_float(iter->value.c_str(),
                                          (uint)iter->value.length());
    break;
    default:
      my_error(ER_UNKNOWN_SYSTEM_VARIABLE, MYF(0), sysvar->name.str);
      result= 1;
      goto err;
    }

    var= new (thd->mem_root) set_var(OPT_GLOBAL, sysvar,
                                      &base_name, res);
    tmp_var_list.push_back(var);

    if (sql_set_variables(thd, &tmp_var_list, false))
    {
      /*
       If there is a connection and an error occurred during install plugin
       then report error at sql layer, else log the error in server log.
      */
      if (current_thd && plugin_options)
      {
        if (thd->is_error())
          my_error(ER_CANT_SET_PERSISTED, MYF(0),
            thd->get_stmt_da()->message_text());
        else
          my_error(ER_CANT_SET_PERSISTED, MYF(0));
      }
      else
      {
        if (thd->is_error())
          LogErr(ERROR_LEVEL, thd->get_stmt_da()->mysql_errno());
        else
          LogErr(ERROR_LEVEL, ER_FAILED_TO_SET_PERSISTED_OPTIONS);
      }
    result= 1;
    goto err;
    }
  }
  /* Once all persisted options are set update variable source. */
  while ((var= it++))
  {
    set_var* setvar= dynamic_cast<set_var*>(var);
    setvar->var->set_source(enum_variable_source::PERSISTED);
    setvar->var->set_source_name(m_persist_filename.c_str());
    setvar->var->clear_user_host_timestamp();
  }

err:
  if (new_thd)
  {
    /* restore access privileges */
    thd->security_context()->set_master_access(access);
    lex_end(thd->lex);
    thd->release_resources();
    delete thd;
  }
  else
  {
    thd->lex= sav_lex;
  }
  return result;
}

/**
  read_persist_file() reads the persisted config file

  This function does following:
    1. Read the persisted config file into a string buffer
    2. This string buffer is parsed with JSON parser to check
       if the format is correct or not.
    3. Check for correct group name.
    4. Extract key/value pair and populate in a global hash map

  @return Error state
    @retval -1 or 1 Failure
    @retval 0 Success
*/
int Persisted_variables_cache::read_persist_file()
{
  char buff[4096]= { 0 };
  string parsed_value;
  const char *error= NULL;
  size_t offset= 0;

  if ((check_file_permissions(m_persist_filename.c_str(), 0)) < 2)
    return -1;

  if (open_persist_file(O_RDONLY))
    return -1;
  do
  {
    /* Read the persisted config file into a string buffer */
    parsed_value.append(buff);
    buff[0]='\0';
  } while (mysql_file_fgets(buff, sizeof(buff) - 1, m_fd));
  close_persist_file();

  /* parse the file contents to check if it is in json format or not */
  std::unique_ptr<Json_dom> json(Json_dom::parse(parsed_value.c_str(),
                                 parsed_value.length(), &error, &offset));
  if (!json.get())
  {
    LogErr(ERROR_LEVEL, ER_JSON_PARSE_ERROR);
    return 1;
  }
  Json_object *obj= reinterpret_cast<Json_object *>(json.get());
  Json_dom *group= obj->get("mysql_server");
  Json_string *group_name= reinterpret_cast<Json_string *>(group);
  if (!group_name)
  {
    LogErr(ERROR_LEVEL, ER_CONFIG_OPTION_WITHOUT_GROUP);
    return 1;
  }
  /* Extract key/value pair and populate in a global hash map */
  Json_wrapper_object_iterator iter(reinterpret_cast<Json_object *>(group));
  while(!iter.empty())
  {
    const std::string key= iter.elt().first;
    if (key == "mysql_server_static_options")
    {
      if (iter.elt().second.is_dom())
      {
        Json_dom *ro_group= iter.elt().second.to_dom(NULL);
        /* Extract key/value pair for all static variables */
        Json_wrapper_object_iterator
          ro_iter(reinterpret_cast<Json_object *>(ro_group));
        while (!ro_iter.empty())
        {
          const std::string key= ro_iter.elt().first;
          if (ro_iter.elt().second.is_dom())
          {
            Json_dom *key_value_type= reinterpret_cast<Json_string* >
              (ro_iter.elt().second.to_dom(NULL));
            if (key_value_type &&
              key_value_type->json_type() != enum_json_type::J_STRING)
            {
              LogErr(ERROR_LEVEL, ER_JSON_PARSE_ERROR);
              return 1;
            }
          }
          const std::string key_value= ro_iter.elt().second.get_data();
          m_persist_ro_variables[key]= key_value;
          ro_iter.next();
        }
      }
    }
    else
    {
      if (iter.elt().second.is_dom())
      {
        /* ensure that key value is string type */
        Json_dom *key_value_type= reinterpret_cast<Json_string* >
          (iter.elt().second.to_dom(NULL));
        if (key_value_type &&
          key_value_type->json_type() != enum_json_type::J_STRING)
        {
          LogErr(ERROR_LEVEL, ER_JSON_PARSE_ERROR);
          return 1;
        }
      }
      const std::string key_value= iter.elt().second.get_data();
      st_persist_var persist_var(key, key_value);
      m_persist_variables.push_back(persist_var);
    }
    iter.next();
  }
  return 0;
}

/**
  append_read_only_variables() does a lookup into persist_variables for read only
  variables and place them after the command line options with a separator
  "----persist-args-separator----"

  This function does nothing when --no-defaults is set or if
  persisted_globals_load is disabled.

  @param [in] argc                      Pointer to argc of original program
  @param [in] argv                      Pointer to argv of original program
  @param [in] plugin_options            This flag tells wether options are handled
                                        during plugin install. If set to TRUE
                                        options are handled as part of install
                                        plugin.

  @return 0 Success
  @return 1 Failure
*/
bool Persisted_variables_cache::append_read_only_variables(int *argc,
  char ***argv, bool plugin_options)
{
  Prealloced_array<char *, 100> my_args(key_memory_persisted_variables);
  TYPELIB group;
  MEM_ROOT alloc;
  const char *type_name= "mysqld";
  char *ptr, **res;
  map<string, string>::const_iterator iter;

  if (*argc < 2 || no_defaults || !persisted_globals_load)
    return 0;

  init_alloc_root(key_memory_persisted_variables, &alloc, 512, 0);
  group.count= 1;
  group.name= "defaults";
  group.type_names= &type_name;

  for (iter= m_persist_ro_variables.begin();
       iter != m_persist_ro_variables.end(); iter++)
  {
    string persist_option= "--loose_" + iter->first + "=" + iter->second;
    if (find_type((char *) type_name, &group, FIND_TYPE_NO_PREFIX))
    {
      char *tmp;
      if ((!(tmp= (char *)
          alloc_root(&alloc, strlen(persist_option.c_str()) + 1))) ||
          my_args.push_back(tmp))
        return 1;
      my_stpcpy(tmp, (const char *) persist_option.c_str());
    }
  }
  /*
   Update existing command line options if there are any persisted
   reasd only options to be appendded
  */
  if (my_args.size())
  {
    if (!(ptr= (char *) alloc_root(&alloc, sizeof(alloc) +
        (my_args.size() + *argc + 2) * sizeof(char *))))
      goto err;
    if (plugin_options)
    {
      /* free previously allocated memory */
      if (ro_persisted_plugin_argv)
      {
        free_defaults(ro_persisted_plugin_argv);
        ro_persisted_plugin_argv= NULL;
      }
      ro_persisted_plugin_argv= res= (char **) (ptr + sizeof(alloc));
    }
    else
      ro_persisted_argv= res= (char **) (ptr + sizeof(alloc));
    memset(res, 0, (sizeof(char *) * (my_args.size() + *argc + 2)));
    /* copy all arguments to new array */
    memcpy((uchar *) (res), (char *) (*argv), (*argc) * sizeof(char *));

    if (!my_args.empty())
    {
      /*
       Set args separator to know options set as part of command line and
       options set from persisted config file
      */
      set_persist_args_separator(&res[*argc]);
      /* copy arguments from persistent config file */
      memcpy((res + *argc + 1), &my_args[0], my_args.size() * sizeof(char *));
    }
    res[my_args.size() + *argc + 1] = 0;  /* last null */
    (*argc)+= (int)my_args.size() + 1;
    *argv= res;
    *(MEM_ROOT *) ptr= std::move(alloc);
    return 0;
  }
  else
    free_root(&alloc, MYF(0));
  return 0;

err:
  my_message_local(ERROR_LEVEL,
                   "Fatal error in defaults handling. Program aborted!");
  exit(1);
}

/**
  reset_persisted_variables() does a lookup into persist_variables and remove the
  variable from the hash if present and flush the hash to file.

  @param [in] thd                     Pointer to connection handle.
  @param [in] name                    Name of variable to remove, if NULL all
                                      variables are removed from config file.
  @param [in] if_exists               Bool value when set to TRUE reports
                                      warning else error if variable is not
                                      present in the config file.

  @return 0 Success
  @return 1 Failure
*/
bool Persisted_variables_cache::reset_persisted_variables(THD *thd,
  const char* name, bool if_exists)
{
  bool result= 0, flush= 0, not_present= 1;
  string var_name;
  bool reset_all= (name ? 0 : 1);
  var_name= (name ? name : string());
  auto it_ro= m_persist_ro_variables.find(var_name);

  if (reset_all)
  {
    if (!m_persist_variables.empty())
    {
      m_persist_variables.clear();
      flush= 1;
    }
    if (!m_persist_ro_variables.empty())
    {
      m_persist_ro_variables.clear();
      flush= 1;
    }
  }
  else
  {
    if (m_persist_variables.size())
    {
      auto it= std::find_if(m_persist_variables.begin(), m_persist_variables.end(),
        [var_name](st_persist_var const & s) { return s.key == var_name; });
      if (it != m_persist_variables.end())
      {
        /* if variable is present in config file remove it */
        m_persist_variables.erase(it);
        flush= 1;
        not_present= 0;
      }
    }
    if (it_ro != m_persist_ro_variables.end())
    {
      /* if static variable is present in config file remove it */
      m_persist_ro_variables.erase(it_ro);
      flush= 1;
      not_present = 0;
    }
    if (not_present)
    {
      /* if not present and if exists is specified, report warning */
      if (if_exists)
      {
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_VAR_DOES_NOT_EXIST,
                            ER_THD(thd, ER_VAR_DOES_NOT_EXIST),
                            var_name.c_str());
      }
      else /* report error */
      {
        my_error(ER_VAR_DOES_NOT_EXIST, MYF(0), var_name.c_str());
        result= 1;
      }
    }
  }
  if (flush)
    flush_to_file();

  return result;
}

/**
  Return in-memory copy persist_variables_
*/
vector<st_persist_var>*
Persisted_variables_cache::get_persisted_variables()
{
  return &m_persist_variables;
}

/**
  Return in-memory copy for static persisted variables
*/
map<string, string>*
Persisted_variables_cache::get_persist_ro_variables()
{
  return &m_persist_ro_variables;
}

void Persisted_variables_cache::cleanup()
{
  mysql_mutex_destroy(&m_LOCK_persist_variables);
  mysql_mutex_destroy(&m_LOCK_persist_file);
  if (ro_persisted_argv)
  {
    free_defaults(ro_persisted_argv);
    ro_persisted_argv= NULL;
  }
  if (ro_persisted_plugin_argv)
  {
    free_defaults(ro_persisted_plugin_argv);
    ro_persisted_plugin_argv= NULL;
  }
}
