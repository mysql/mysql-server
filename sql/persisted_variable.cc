/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "persisted_variable.h"
#include "current_thd.h"
#include "derror.h"           // ER_THD
#include "json_dom.h"
#include "lex_string.h"
#include "log.h"                        // sql_print_error
#include "my_default.h"                 // check_file_permissions
#include "mysqld.h"
#include "set_var.h"
#include "sql_class.h"
#include "sql_lex.h"
#include "sys_vars_shared.h"

#ifdef HAVE_PSI_FILE_INTERFACE
PSI_file_key key_persist_file_cnf;
static PSI_file_info all_persist_files[]=
{
  { &key_persist_file_cnf, "cnf", 0}
};
#endif /* HAVE_PSI_FILE_INTERFACE */

#ifdef HAVE_PSI_MUTEX_INTERFACE
PSI_mutex_key key_persist_file, key_persist_hash;
static PSI_mutex_info all_persist_mutexes[]=
{
  { &key_persist_file, "m_LOCK_persist_file", 0, 0},
  { &key_persist_hash, "m_LOCK_persist_hash", 0, 0}
};
#endif /* HAVE_PSI_MUTEX_INTERFACE */

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
  PSI_MUTEX_CALL(register_mutex)(category, all_persist_mutexes, count);
#endif
}
#endif

Persisted_variables_cache* Persisted_variables_cache::m_instance= NULL;

/**
  Initilize class members.
*/
void Persisted_variables_cache::init()
{
  char name[FN_REFLEN];

#ifdef HAVE_PSI_INTERFACE
   my_init_persist_psi_keys();
#endif

  if (mysql_real_data_home_ptr)
    convert_dirname(name, mysql_real_data_home_ptr, NullS);
  else
    convert_dirname(name, MYSQL_DATADIR, NullS);

  m_persist_filename= string(name) + MYSQL_PERSIST_CONFIG_NAME + ".cnf";

  mysql_mutex_init(key_persist_hash,
    &m_LOCK_persist_hash, MY_MUTEX_INIT_FAST);

  mysql_mutex_init(key_persist_file,
    &m_LOCK_persist_file, MY_MUTEX_INIT_FAST);

  m_instance= this;
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
  char val_buf[1024];
  size_t val_length;

  struct st_persist_var_data
  {
    string name;
    string value;
  } tmp_var;
  sys_var *system_var= setvar->var;

  const char* var_name=
    Persisted_variables_cache::get_variable_name(system_var);
  const char* var_value=
    Persisted_variables_cache::get_variable_value(thd,
                             system_var, val_buf, &val_length);

  /* structured variables may have basename if specified */
  string struct_var_name= (setvar->base.str ? setvar->base.str : string());
  tmp_var.name= var_name;
  tmp_var.value= var_value;

  if (struct_var_name.length())
    tmp_var.name= struct_var_name.append(".").append(tmp_var.name);
  /* modification to in-memory must be thread safe */
  mysql_mutex_lock(&m_LOCK_persist_hash);
  /* if present update variable with new value else insert into hash */
  m_persist_hash[tmp_var.name]= tmp_var.value;
  mysql_mutex_unlock(&m_LOCK_persist_hash);
}

/**
  Retrieve variables value from sys_var

   @param [in] thd           Pointer to connection handler
   @param [in] system_var    Pointer to sys_var which is being SET
   @param [out] val_buf      Buffer pointing to variable value
   @param [out] val_length   Length of the value buffer

   @return
     Pointer to buffer holding the value
*/
const char* Persisted_variables_cache::get_variable_value(THD *thd,
   sys_var *system_var, char* val_buf, size_t* val_length)
{
  const char* value= val_buf;
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
                   &fromcs, val_buf, val_length);
  mysql_mutex_unlock(&LOCK_global_system_variables);
  val_buf[*val_length]= '\0';

  /* convert the retrieved value to utf8mb4 */
  size_t new_len= (tocs->mbmaxlen * (*val_length)) / fromcs->mbminlen + 1;
  char *result= new char[new_len];
  memset(result, 0, new_len);
  *val_length= copy_and_convert(result, new_len, tocs, value, *val_length,
                               fromcs, &dummy_err);
  memcpy(val_buf, result, strlen(result)+1);
  delete []result;
  return val_buf;
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
  mysql_mutex_lock(&m_LOCK_persist_file);
  /* construct json formatted string buffer */
  String dest("{ \"mysql_server\": {", &my_charset_utf8mb4_bin);
  map<string, string>::const_iterator iter, last_iter;

  if (m_persist_hash.size() > 1)
    last_iter= --(m_persist_hash.end());

  for (iter = m_persist_hash.begin();
       iter != m_persist_hash.end(); iter++)
  {
    String str;
    std::unique_ptr<Json_string> var_name(new (std::nothrow)
                    Json_string(iter->first));
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
                    Json_string(iter->second));
    Json_wrapper vv(var_val.release());
    vv.to_string(&str, true, String().ptr());
    dest.append(str);
    if (m_persist_hash.size() > 1 && iter != last_iter)
      dest.append(" , ");
  }
  dest.append(" } }");
  /*
    If file does not exists create one. When persisted_globals_load is 0
    we dont read contents of mysqld-auto.cnf file, thus append any new
    variables which are persisted to this file.
  */
  bool ret= 0;
  ret= open_persist_file(O_CREAT | O_WRONLY);

  if(ret)
  {
    mysql_mutex_unlock(&m_LOCK_persist_file);
    close_persist_file();
    return 1;
  }
  /* write to file */
  if ((mysql_file_fputs(dest.c_ptr(), fd)) < 0)
  {
    mysql_mutex_unlock(&m_LOCK_persist_file);
    close_persist_file();
    return 1;
  }

  close_persist_file();
  mysql_mutex_unlock(&m_LOCK_persist_file);
  return 0;
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
  fd= mysql_file_fopen(key_persist_file_cnf,
                       m_persist_filename.c_str(), flag, MYF(0));
  return (fd ? 0 : 1);
}

/**
  Close persisted config file
   @return void
*/
void Persisted_variables_cache::close_persist_file()
{
  mysql_file_fclose(fd, MYF(0));
  fd= NULL;
}

/**
  load_persist_file() searches for persisted config file in
  data dir, reads options from this config file

  Before loading this config file do following:
    1. Read the datadir if present in config file or set as command line,
       in order to know from where to load this config file. If not set
       then read from MYSQL_DATADIR.

  @return Error state
    @retval TRUE An error occurred
    @retval FALSE Success
*/
bool Persisted_variables_cache::load_persist_file()
{
  const char *conf_file= MYSQL_PERSIST_CONFIG_NAME;
  char *end;
  const char *dirs, *ext= ".cnf";
  char persist_config_file[FN_REFLEN + 10];

  /*
    if datadir is set then search in this data dir else search in
    MYSQL_DATADIR
  */
  dirs= ((mysql_real_data_home_ptr) ?
    mysql_real_data_home_ptr : MYSQL_DATADIR);
  if (dirs && (strlen(dirs) >= FN_REFLEN-3))
    return 1;
  end=convert_dirname(persist_config_file, dirs, NullS);
  if (dirs[0] == FN_HOMELIB)
    *end++='.';
  strxmov(end, conf_file, ext, NullS);
  fn_format(persist_config_file, persist_config_file, "", "",
            MY_UNPACK_FILENAME);

  // set file name
  m_persist_filename= string(persist_config_file);

  if (read_persist_file() > 0)
    return 1;

  return 0;
}

/**
  set_persist_options() will set the options read from persisted config file

  This function does nothing when --no-defaults is set or if
  persisted_globals_load is set to false

   @param [in] what_options        Flag which tell what options are being set.
                                   If set to FALSE non plugin variables are set
                                   else plugin variables are set

  @return Error state
    @retval TRUE An error occurred
    @retval FALSE Success
*/
bool Persisted_variables_cache::set_persist_options(bool what_options)
{
  THD *thd;
  LEX lex_tmp, *sav_lex= NULL;
  List<set_var_base> tmp_var_list;
  map<string, string> *persist_hash= NULL;
  map<string, string>::const_iterator iter;
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
      sql_print_error("Failed to set persisted options.");
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
    Based on what_options, we decide on what options to be set. If
    what_options is false we set all non plugin variables and then
    keep all plugin variables in a map. When the plugin is installed
    plugin variables are read from the map and set.
  */
  persist_hash= (what_options ? &m_persist_plugin_hash : &m_persist_hash);

  for (iter = persist_hash->begin();
       iter != persist_hash->end(); iter++)
  {
    Item *res= NULL;
    set_var *var= NULL;
    sys_var *sysvar= NULL;
    string var_name= iter->first;
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
      m_persist_plugin_hash[iter->first]= iter->second;
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
      res= new (thd->mem_root) Item_uint(iter->second.c_str(),
                                          iter->second.length());
    break;
    case SHOW_CHAR:
    case SHOW_CHAR_PTR:
    case SHOW_LEX_STRING:
    case SHOW_BOOL:
    case SHOW_MY_BOOL:
      res= new (thd->mem_root) Item_string(iter->second.c_str(),
                                            iter->second.length(),
                                            &my_charset_utf8mb4_bin);
    break;
    case SHOW_DOUBLE:
      res= new (thd->mem_root) Item_float(iter->second.c_str(),
                                          iter->second.length());
    break;
    default:
      my_error(ER_UNKNOWN_SYSTEM_VARIABLE, MYF(0), sysvar->name.str);
      result= 1;
      goto err;
    }

    var= new (thd->mem_root) set_var(OPT_GLOBAL, sysvar,
                                      &base_name, res);
    tmp_var_list.push_back(var);
  }
  if (sql_set_variables(thd, &tmp_var_list, false))
  {
    sql_print_error("Failed to set persisted options.");
    result= 1;
    goto err;
  }
  /* Once all persisted options are set update variable source. */
  while ((var= it++))
  {
    set_var* setvar= dynamic_cast<set_var*>(var);
    setvar->var->set_source(enum_variable_source::PERSISTED);
    setvar->var->set_source_name(m_persist_filename.c_str());
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
  } while (mysql_file_fgets(buff, sizeof(buff) - 1, fd));
  close_persist_file();

  /* parse the file contents to check if it is in json format or not */
  std::unique_ptr<Json_dom> json(Json_dom::parse(parsed_value.c_str(),
                                 parsed_value.length(), &error, &offset));
  if (!json.get())
  {
    sql_print_error("JSON parsing error");
    return 1;
  }
  Json_object *obj= reinterpret_cast<Json_object *>(json.get());
  Json_dom *group= obj->get("mysql_server");
  Json_string *group_name= reinterpret_cast<Json_string *>(group);
  if (!group_name)
  {
    sql_print_error("Found option without preceding group in config file");
    return 1;
  }
  /* Extract key/value pair and populate in a global hash map */
  Json_wrapper_object_iterator iter(reinterpret_cast<Json_object *>(group));
  while(!iter.empty())
  {
    const std::string key= iter.elt().first;
    const std::string key_value= iter.elt().second.get_data();
    m_persist_hash[key] = key_value;
    iter.next();
  }
  return 0;
}

/**
  reset_persisted_variables() does a lookup into persist_hash and remove the
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
  bool result= 0, flush= 0;
  string var_name;
  bool reset_all= (name ? 0 : 1);
  var_name= (name ? name : string());
  std::map<string, string>::iterator it= m_persist_hash.find(var_name);

  if (reset_all)
  {
    if (!m_persist_hash.empty())
    {
      m_persist_hash.clear();
      flush= 1;
    }
  }
  else
  {
    if (it != m_persist_hash.end())
    {
      /* if variable is present in config file remove it */
      m_persist_hash.erase(it);
      flush= 1;
    }
    else
    {
      /* if not present and if exists is specified report warning */
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
  Return in-memory copy persist_hash
*/
map<string, string>* Persisted_variables_cache::get_persist_hash()
{
  return &m_persist_hash;
}
void Persisted_variables_cache::cleanup()
{
  mysql_mutex_destroy(&m_LOCK_persist_hash);
  mysql_mutex_destroy(&m_LOCK_persist_file);
}
