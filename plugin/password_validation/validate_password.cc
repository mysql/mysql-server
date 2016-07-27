/* Copyright (c) 2012, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include <my_sys.h>
#include <string>
#include <mysql/plugin_validate_password.h>
#include <mysql/service_my_plugin_log.h>
#include <mysql/service_mysql_string.h>
#include <set>
#include <iostream>
#include <fstream>
#include <algorithm> // std::swap
THD *thd_get_current_thd(); // from sql_class.cc



#define MAX_DICTIONARY_FILE_LENGTH    1024 * 1024
#define PASSWORD_SCORE                25
#define MIN_DICTIONARY_WORD_LENGTH    4
#define MAX_PASSWORD_LENGTH           100

/* Read-write lock for dictionary_words cache */
mysql_rwlock_t LOCK_dict_file;

#ifdef HAVE_PSI_INTERFACE
PSI_rwlock_key key_validate_password_LOCK_dict_file;

static PSI_rwlock_info all_validate_password_rwlocks[]=
{
  { &key_validate_password_LOCK_dict_file, "LOCK_dict_file", 0}
};

void init_validate_password_psi_keys()
{
  const char* category= "validate";
  int count;

  count= array_elements(all_validate_password_rwlocks);
  mysql_rwlock_register(category, all_validate_password_rwlocks, count);
}
#endif /* HAVE_PSI_INTERFACE */


/*
  Handle assigned when loading the plugin.
  Used with the error reporting functions.
*/

static MYSQL_PLUGIN plugin_info_ptr;
/*  
  These are the 3 password policies that this plugin allow to set
  and configure as per the requirements.
*/

enum password_policy_enum { PASSWORD_POLICY_LOW,
                            PASSWORD_POLICY_MEDIUM,
                            PASSWORD_POLICY_STRONG
};

static const char* policy_names[] = { "LOW", "MEDIUM", "STRONG", NullS };

static TYPELIB password_policy_typelib_t = {
        array_elements(policy_names) - 1,
        "password_policy_typelib_t",
        policy_names,
        NULL
};

typedef std::string string_type;
typedef std::set<string_type> set_type;
static set_type dictionary_words;

static int validate_password_length;
static int validate_password_number_count;
static int validate_password_mixed_case_count;
static int validate_password_special_char_count;
static ulong validate_password_policy;
static char *validate_password_dictionary_file;
static char *validate_password_dictionary_file_last_parsed= NULL;
static long long validate_password_dictionary_file_words_count= 0;
static my_bool check_user_name;

/**
  Activate the new dictionary

  Assigns a local list to the global variable,
  taking the correct locks in the process.
  Also updates the status variables.
  @param dict_words new dictionary words set

*/
static void dictionary_activate(set_type *dict_words)
{
  time_t start_time;
  struct tm tm;
  char timebuf[20]; /* "YYYY-MM-DD HH:MM:SS" */
  char *new_ts;

  /* fetch the start time */
  start_time= my_time(MYF(0));
  localtime_r(&start_time, &tm);
  my_snprintf(timebuf, sizeof(timebuf), "%04d-%02d-%02d %02d:%02d:%02d",
              tm.tm_year + 1900,
              tm.tm_mon + 1,
              tm.tm_mday,
              tm.tm_hour,
              tm.tm_min,
              tm.tm_sec);
  new_ts= my_strdup(PSI_NOT_INSTRUMENTED, timebuf, MYF(0));

  mysql_rwlock_wrlock(&LOCK_dict_file);
  std::swap(dictionary_words, *dict_words);
  validate_password_dictionary_file_words_count= dictionary_words.size();
  std::swap(new_ts, validate_password_dictionary_file_last_parsed);
  mysql_rwlock_unlock(&LOCK_dict_file);

  /* frees up the data just replaced */
  if (!dict_words->empty())
    dict_words->clear();
  if (new_ts)
    my_free(new_ts);
}


/* To read dictionary file into std::set */
static void read_dictionary_file()
{
  string_type words;
  set_type dict_words;
  std::streamoff file_length;

  if (validate_password_dictionary_file == NULL)
  {
    if (validate_password_policy == PASSWORD_POLICY_STRONG)
      my_plugin_log_message(&plugin_info_ptr, MY_WARNING_LEVEL,
                            "Dictionary file not specified");
    /* NULL is a valid value, despite the warning */
    dictionary_activate(&dict_words);
    return;
  }
  try
  {
    std::ifstream dictionary_stream(validate_password_dictionary_file);
    if (!dictionary_stream || !dictionary_stream.is_open())
    {
      my_plugin_log_message(&plugin_info_ptr, MY_WARNING_LEVEL,
                            "Dictionary file not loaded");
      return;
    }
    dictionary_stream.seekg(0, std::ios::end);
    file_length= dictionary_stream.tellg();
    dictionary_stream.seekg(0, std::ios::beg);
    if (file_length > MAX_DICTIONARY_FILE_LENGTH)
    {
      dictionary_stream.close();
      my_plugin_log_message(&plugin_info_ptr, MY_WARNING_LEVEL,
                            "Dictionary file size exceeded",
                            "MAX_DICTIONARY_FILE_LENGTH, not loaded");
      return;
    }
    for (std::getline(dictionary_stream, words); dictionary_stream.good();
         std::getline(dictionary_stream, words))
         dict_words.insert(words);
    dictionary_stream.close();
    dictionary_activate(&dict_words);
  }
  catch (...) // no exceptions !
  {
    my_plugin_log_message(&plugin_info_ptr, MY_WARNING_LEVEL,
                          "Exception while reading the dictionary file");
  }
}


/* Clear words from std::set */
static void free_dictionary_file()
{
  mysql_rwlock_wrlock(&LOCK_dict_file);
  if (!dictionary_words.empty())
    dictionary_words.clear();
  if (validate_password_dictionary_file_last_parsed)
  {
    my_free(validate_password_dictionary_file_last_parsed);
    validate_password_dictionary_file_last_parsed= NULL;
  }
  mysql_rwlock_unlock(&LOCK_dict_file);
}

/*
  Checks whether password or substring of password
  is present in dictionary file stored as std::set
*/
static int validate_dictionary_check(mysql_string_handle password)
{
  int length;
  int error= 0;
  char *buffer;

  if (dictionary_words.empty())
   return (1);

  /* New String is allocated */
  mysql_string_handle lower_string_handle= mysql_string_to_lowercase(password);
  if (!(buffer= (char*) malloc(MAX_PASSWORD_LENGTH)))
    return (0);

  length= mysql_string_convert_to_char_ptr(lower_string_handle, "utf8",
                                           buffer, MAX_PASSWORD_LENGTH,
                                           &error);
  /* Free the allocated string */
  mysql_string_free(lower_string_handle);
  int substr_pos= 0;
  int substr_length= length;
  string_type password_str= string_type((const char *)buffer, length);
  string_type password_substr;
  set_type::iterator itr;
  /*  
    std::set as container stores the dictionary words,
    binary comparison between dictionary words and password
  */
  mysql_rwlock_rdlock(&LOCK_dict_file);
  while (substr_length >= MIN_DICTIONARY_WORD_LENGTH)
  {
    substr_pos= 0;
    while (substr_pos + substr_length <= length)
    {
      password_substr= password_str.substr(substr_pos, substr_length);
      itr= dictionary_words.find(password_substr);
      if (itr != dictionary_words.end())
      {
        mysql_rwlock_unlock(&LOCK_dict_file);
        free(buffer);
        return (0);
      }
      substr_pos++;
    }
    substr_length--;
  }
  mysql_rwlock_unlock(&LOCK_dict_file);
  free(buffer);
  return (1);
}


/**
  Compare a sequence of bytes in "a" with the reverse sequence of bytes of "b"

  @param a the first sequence
  @param a_len the length of a
  @param b the second sequence
  @param b_len the length of b

  @retval true sequences match
  @retval false sequences don't match
*/
static bool my_memcmp_reverse(const char *a, size_t a_len,
                              const char *b, size_t b_len)
{
  const char *a_ptr;
  const char *b_ptr;

  if (a_len != b_len)
    return false;

  for (a_ptr= a, b_ptr= b + b_len - 1; b_ptr >= b; a_ptr++, b_ptr--)
    if (*a_ptr != *b_ptr)
      return false;
  return true;
}

/**
  Validate a user name from the security context

  A helper function.
  Validates one user name (as specified by field_name)
  against the data in buffer/length by comparing the byte
  sequences in forward and reverse.

  Logs an error to the error log if it can't pick up the user names.

  @param ctx the current security context
  @param buffer the password data
  @param length the length of buffer
  @param field_name the id of the security context field to use
  @param logical_name the name of the field to use in the error message

  @retval true name can be used
  @retval false name is invalid
*/
static bool is_valid_user(MYSQL_SECURITY_CONTEXT ctx,
                          const char *buffer, int length,
                          const char *field_name,
                          const char *logical_name)
{
  MYSQL_LEX_CSTRING user={ NULL, 0 };

  if (security_context_get_option(ctx, field_name, &user))
  {
    my_plugin_log_message(&plugin_info_ptr, MY_ERROR_LEVEL,
                          "Can't retrieve the %s from the"
                          "security context", logical_name);
    return false;
  }

  /* lengths must match for the strings to match */
  if (user.length != (size_t) length)
    return true;
  /* empty strings turn the check off */
  if (user.length == 0)
    return true;
  /* empty strings turn the check off */
  if (!user.str)
    return true;

  return (0 != memcmp(buffer, user.str, user.length) &&
          !my_memcmp_reverse(user.str, user.length, buffer, length));
}


/**
  Check if the password is not the user name

  Helper function.
  Checks if the password supplied is valid to use by comparing it
  the effected and the login user names to it and to the reverse of it.
  logs an error to the error log if it can't pick up the names.

  @param password the password handle
  @retval true The password can be used
  @retval false the password is invalid
*/
static bool is_valid_password_by_user_name(mysql_string_handle password)
{
  char buffer[MAX_PASSWORD_LENGTH];
  int length, error;
  MYSQL_SECURITY_CONTEXT ctx= NULL;

  if (!check_user_name)
    return true;

  if (thd_get_security_context(thd_get_current_thd(), &ctx) || !ctx)
  {
    my_plugin_log_message(&plugin_info_ptr, MY_ERROR_LEVEL,
                          "Can't retrieve the security context");
    return false;
  }

  length= mysql_string_convert_to_char_ptr(password, "utf8",
                                           buffer, MAX_PASSWORD_LENGTH,
                                           &error);

  return
    is_valid_user(ctx, buffer, length, "user", "login user name")
    && is_valid_user(ctx, buffer, length, "priv_user", "effective user name");
}

static int validate_password_policy_strength(mysql_string_handle password,
                                             int policy)
{
  int has_digit= 0;
  int has_lower= 0;
  int has_upper= 0;
  int has_special_chars= 0;
  int n_chars= 0;
  mysql_string_iterator_handle iter;

  iter = mysql_string_get_iterator(password);
  while(mysql_string_iterator_next(iter))
  {
    n_chars++;
    if (policy > PASSWORD_POLICY_LOW)
    {
      if (mysql_string_iterator_islower(iter))
        has_lower++;
      else if (mysql_string_iterator_isupper(iter))
        has_upper++;
      else if (mysql_string_iterator_isdigit(iter))
        has_digit++;
      else
        has_special_chars++;
    }
  }

  mysql_string_iterator_free(iter);
  if (n_chars >= validate_password_length)
  {
    if (!is_valid_password_by_user_name(password))
      return(0);

    if (policy == PASSWORD_POLICY_LOW)
      return (1);
    if (has_upper >= validate_password_mixed_case_count &&
        has_lower >= validate_password_mixed_case_count &&
        has_special_chars >= validate_password_special_char_count &&
        has_digit >= validate_password_number_count)
    {
      if (policy == PASSWORD_POLICY_MEDIUM || validate_dictionary_check
                                              (password))
        return (1);
    }
  }
  return (0);
}

/* Actual plugin function which acts as a wrapper */
static int validate_password(mysql_string_handle password)
{
  return validate_password_policy_strength(password, validate_password_policy);
}

/* Password strength between (0-100) */
static int get_password_strength(mysql_string_handle password)
{
  int policy= 0;
  int n_chars= 0;
  mysql_string_iterator_handle iter;

  if (!is_valid_password_by_user_name(password))
    return 0;

  iter = mysql_string_get_iterator(password);
  while(mysql_string_iterator_next(iter))
    n_chars++;

  mysql_string_iterator_free(iter);
  if (n_chars < MIN_DICTIONARY_WORD_LENGTH)
    return (policy);
  if (n_chars < validate_password_length)
    return (PASSWORD_SCORE);
  else
  {
    policy= PASSWORD_POLICY_LOW;
    if (validate_password_policy_strength(password, PASSWORD_POLICY_MEDIUM))
    {
      policy= PASSWORD_POLICY_MEDIUM;
      if (validate_dictionary_check(password))
        policy= PASSWORD_POLICY_STRONG;
    }
  }
  return ((policy+1) * PASSWORD_SCORE + PASSWORD_SCORE);
}

/**
  @brief Check and readjust effective value of validate_password_length

  @details
  Readjust validate_password_length according to the values of
  validate_password_number_count,validate_password_mixed_case_count
  and validate_password_special_char_count. This is required at the
  time plugin installation and as a part of setting new values for
  any of above mentioned variables.

*/
static void
readjust_validate_password_length()
{
  int policy_password_length;

  /*
    Effective value of validate_password_length variable is:

    MAX(validate_password_length,
        (validate_password_number_count +
         2*validate_password_mixed_case_count +
         validate_password_special_char_count))
  */
  policy_password_length= (validate_password_number_count +
                           (2 * validate_password_mixed_case_count) +
                           validate_password_special_char_count);

  if (validate_password_length < policy_password_length)
  {
    /*
       Raise a warning that effective restriction on password
       length is changed.
    */
    my_plugin_log_message(&plugin_info_ptr, MY_WARNING_LEVEL,
                          "Effective value of validate_password_length is changed."
                          " New value is %d",
                          policy_password_length);

    validate_password_length= policy_password_length;
  }
}

/* Plugin type-specific descriptor */
static struct st_mysql_validate_password validate_password_descriptor=
{
  MYSQL_VALIDATE_PASSWORD_INTERFACE_VERSION,
  validate_password,                         /* validate function          */
  get_password_strength                      /* validate strength function */
};

/*  
  Initialize the password plugin at server start or plugin installation,
  read dictionary file into std::set.
*/

static int validate_password_init(MYSQL_PLUGIN plugin_info)
{
  plugin_info_ptr= plugin_info;
#ifdef HAVE_PSI_INTERFACE
  init_validate_password_psi_keys();
#endif
  mysql_rwlock_init(key_validate_password_LOCK_dict_file, &LOCK_dict_file);
  read_dictionary_file();
  /* Check if validate_password_length needs readjustment */
  readjust_validate_password_length();
  return (0);
}

/*  
  Terminate the password plugin at server shutdown or plugin deinstallation.
  It empty the std::set and returns 0
*/

static int validate_password_deinit(void *arg MY_ATTRIBUTE((unused)))
{
  free_dictionary_file();
  mysql_rwlock_destroy(&LOCK_dict_file);
  return (0);
}

/*
  Update function for validate_password_dictionary_file.
  If dictionary file is changed, this function will flush
  the cache and re-load the new dictionary file.
*/
static void
dictionary_update(MYSQL_THD thd MY_ATTRIBUTE((unused)),
                  struct st_mysql_sys_var *var MY_ATTRIBUTE((unused)),
                  void *var_ptr, const void *save)
{
  *(const char**)var_ptr= *(const char**)save;
  read_dictionary_file();
}

/*
  update function for:
  1. validate_password_length
  2. validate_password_number_count
  3. validate_password_mixed_case_count
  4. validate_password_special_char_count
*/
static void
length_update(MYSQL_THD thd MY_ATTRIBUTE((unused)),
              struct st_mysql_sys_var *var MY_ATTRIBUTE((unused)),
              void *var_ptr, const void *save)
{
  /* check if there is an actual change */
  if (*((int *)var_ptr) == *((int *)save))
    return;

  /*
    set new value for system variable.
    Note that we need not know for which of the above mentioned
    variables, length_update() is called because var_ptr points
    to the location at which corresponding static variable is
    declared in this file.
  */
  *((int *)var_ptr)= *((int *)save);

  readjust_validate_password_length();
}



/* Plugin system variables */

static MYSQL_SYSVAR_INT(length, validate_password_length,
  PLUGIN_VAR_RQCMDARG,
  "Password validate length to check for minimum password_length",
  NULL, length_update, 8, 0, 0, 0);

static MYSQL_SYSVAR_INT(number_count, validate_password_number_count,
  PLUGIN_VAR_RQCMDARG,
  "password validate digit to ensure minimum numeric character in password",
  NULL, length_update, 1, 0, 0, 0);

static MYSQL_SYSVAR_INT(mixed_case_count, validate_password_mixed_case_count,
  PLUGIN_VAR_RQCMDARG,
  "Password validate mixed case to ensure minimum upper/lower case in password",
  NULL, length_update, 1, 0, 0, 0);

static MYSQL_SYSVAR_INT(special_char_count,
  validate_password_special_char_count, PLUGIN_VAR_RQCMDARG,
  "password validate special to ensure minimum special character in password",
  NULL, length_update, 1, 0, 0, 0);

static MYSQL_SYSVAR_ENUM(policy, validate_password_policy,
  PLUGIN_VAR_RQCMDARG,
  "password_validate_policy choosen policy to validate password"
  "possible values are LOW MEDIUM (default), STRONG",
  NULL, NULL, PASSWORD_POLICY_MEDIUM, &password_policy_typelib_t);

static MYSQL_SYSVAR_STR(dictionary_file, validate_password_dictionary_file,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
  "password_validate_dictionary file to be loaded and check for password",
  NULL, dictionary_update, NULL);

static MYSQL_SYSVAR_BOOL(check_user_name, check_user_name,
  PLUGIN_VAR_NOCMDARG,
  "Check if the password matches the login or the effective user names "
  "or the reverse of them",
  NULL, NULL, FALSE);

static struct st_mysql_sys_var* validate_password_system_variables[]= {
  MYSQL_SYSVAR(length),
  MYSQL_SYSVAR(number_count),
  MYSQL_SYSVAR(mixed_case_count),
  MYSQL_SYSVAR(special_char_count),
  MYSQL_SYSVAR(policy),
  MYSQL_SYSVAR(dictionary_file),
  MYSQL_SYSVAR(check_user_name),
  NULL
};

static struct st_mysql_show_var validate_password_status_variables[]= {
    { "validate_password_dictionary_file_last_parsed",
      (char *) &validate_password_dictionary_file_last_parsed,
      SHOW_CHAR_PTR, SHOW_SCOPE_GLOBAL },
    { "validate_password_dictionary_file_words_count",
      (char *) &validate_password_dictionary_file_words_count,
      SHOW_LONGLONG, SHOW_SCOPE_GLOBAL },
    { NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL }
};

mysql_declare_plugin(validate_password)
{
  MYSQL_VALIDATE_PASSWORD_PLUGIN,     /*   type                            */
  &validate_password_descriptor,      /*   descriptor                      */
  "validate_password",                /*   name                            */
  "Oracle Corporation",               /*   author                          */
  "check password strength",          /*   description                     */
  PLUGIN_LICENSE_GPL,
  validate_password_init,             /*   init function (when loaded)     */
  validate_password_deinit,           /*   deinit function (when unloaded) */
  0x0101,                             /*   version                         */
  validate_password_status_variables, /*   status variables                */
  validate_password_system_variables, /*   system variables                */
  NULL,
  0,
}
mysql_declare_plugin_end;
