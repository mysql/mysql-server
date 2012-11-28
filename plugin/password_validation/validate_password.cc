/* Copyright © 2012, Oracle and/or its affiliates. All rights reserved.

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
#include <set>
#include <iostream>
#include <fstream>


/*  
  __attribute__(A) needs to be defined for Windows else complier
  do not recognise it. Argument in plugin_init and plugin_deinit
  Used in other plugins as well.
*/
#if !defined(__attribute__) && (defined(__cplusplus) || !defined(__GNUC__)  || __GNUC__ == 2 && __GNUC_MINOR__ < 8)
#define __attribute__(A)
#endif

#define MAX_DICTIONARY_FILE_LENGTH    1024 * 1024
#define PASSWORD_SCORE                25
#define MIN_DICTIONARY_WORD_LENGTH    4
#define MAX_PASSWORD_LENGTH           100

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

/* To read dictionary file into std::set */
static void read_dictionary_file()
{
  string_type words;
  long file_length;

  if (validate_password_dictionary_file == NULL)
  {
    my_plugin_log_message(&plugin_info_ptr, MY_WARNING_LEVEL,
                          "Dictionary file not specified");
    return;
  }
  std::ifstream dictionary_stream(validate_password_dictionary_file);
  if (!dictionary_stream)
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
                          "Dictionary file size exceed",
                          "MAX_DICTIONARY_FILE_LENGTH, not loaded");
    return;
  }
  while (dictionary_stream.good())
  {
    std::getline(dictionary_stream, words);
    dictionary_words.insert(words);
  }
  dictionary_stream.close();
}

/* Clear words from std::set */
static void free_dictionary_file()
{
  if (!dictionary_words.empty())
    dictionary_words.clear();
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
  mysql_string_handle lower_string_handle= mysql_string_to_lowercase(password);
  if (!(buffer= (char*) malloc(MAX_PASSWORD_LENGTH)))
    return (0);

  length= mysql_string_convert_to_char_ptr(lower_string_handle, "utf8",
                                           buffer, MAX_PASSWORD_LENGTH,
                                           &error);
  int substr_pos= 0;
  int substr_length= length;
  string_type password_str= (const char *)buffer;
  string_type password_substr;
  set_type::iterator itr;
  /*  
    std::set as container stores the dictionary words,
    binary comparison between dictionary words and password
  */
  if (!dictionary_words.empty())
  {
    while (substr_length >= MIN_DICTIONARY_WORD_LENGTH)
    {
      substr_pos= 0;
      while (substr_pos + substr_length <= length)
      {
        password_substr= password_str.substr(substr_pos, substr_length);
        itr= dictionary_words.find(password_substr);
        if (itr != dictionary_words.end())
        {
          free(buffer);
          return (0);
        }
        substr_pos++;
      }
      substr_length--;
    }
  }
  free(buffer);
  return (1);
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
  read_dictionary_file();
  return (0);
}

/*  
  Terminate the password plugin at server shutdown or plugin deinstallation.
  It empty the std::set and returns 0
*/

static int validate_password_deinit(void *arg __attribute__((unused)))
{
  free_dictionary_file();
  return (0);
}

/* Plugin system variables */

static MYSQL_SYSVAR_INT(length, validate_password_length,
  PLUGIN_VAR_RQCMDARG,
  "Password validate length to check for minimum password_length",
  NULL, NULL, 8, 0, 0, 0);

static MYSQL_SYSVAR_INT(number_count, validate_password_number_count,
  PLUGIN_VAR_RQCMDARG,
  "password validate digit to ensure minimum numeric character in password",
  NULL, NULL, 1, 0, 0, 0);

static MYSQL_SYSVAR_INT(mixed_case_count, validate_password_mixed_case_count,
  PLUGIN_VAR_RQCMDARG,
  "Password validate mixed case to ensure minimum upper/lower case in password",
  NULL, NULL, 1, 0, 0, 0);

static MYSQL_SYSVAR_INT(special_char_count,
  validate_password_special_char_count, PLUGIN_VAR_RQCMDARG,
  "password validate special to ensure minimum special character in password",
  NULL, NULL, 1, 0, 0, 0);

static MYSQL_SYSVAR_ENUM(policy, validate_password_policy,
  PLUGIN_VAR_RQCMDARG,
  "password_validate_policy choosen policy to validate password"
  "possible values are LOW MEDIUM (default), STRONG",
  NULL, NULL, PASSWORD_POLICY_MEDIUM, &password_policy_typelib_t);

static MYSQL_SYSVAR_STR(dictionary_file, validate_password_dictionary_file,
  PLUGIN_VAR_READONLY,
  "password_validate_dictionary file to be loaded and check for password",
  NULL, NULL, NULL);

static struct st_mysql_sys_var* validate_password_system_variables[]= {
  MYSQL_SYSVAR(length),
  MYSQL_SYSVAR(number_count),
  MYSQL_SYSVAR(mixed_case_count),
  MYSQL_SYSVAR(special_char_count),
  MYSQL_SYSVAR(policy),
  MYSQL_SYSVAR(dictionary_file),
  NULL
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
  0x0100,                             /*   version                         */
  NULL,
  validate_password_system_variables, /*   system variables                */
  NULL,
  0,
}
mysql_declare_plugin_end;
