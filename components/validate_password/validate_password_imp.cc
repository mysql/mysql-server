/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "validate_password_imp.h"

#include <assert.h>
#include <string.h>
#include <algorithm>  // std::swap
#include <fstream>    // std::ifsteam
#include <iomanip>
#include <set>  // std::set
#include <sstream>

#include "mysql/components/library_mysys/my_memory.h"
#include "mysql/components/services/mysql_rwlock.h"
#include "mysqld_error.h"

#define PSI_NOT_INSTRUMENTED 0
const int MAX_DICTIONARY_FILE_LENGTH = (1024 * 1024);
const int PASSWORD_SCORE = 25;
const int MIN_DICTIONARY_WORD_LENGTH = 4;
const int MAX_PASSWORD_LENGTH = 100;

#define array_elements(A) ((size_t)(sizeof(A) / sizeof(A[0])))

/* Read-write lock for dictionary_words cache */
mysql_rwlock_t LOCK_dict_file;

PSI_rwlock_key key_validate_password_LOCK_dict_file;

static PSI_rwlock_info all_validate_password_rwlocks[] = {
    {&key_validate_password_LOCK_dict_file, "LOCK_dict_file", 0, 0, ""}};

static void init_validate_password_psi_keys() {
  const char *category = "validate_pwd";
  int count;

  count = static_cast<int>(array_elements(all_validate_password_rwlocks));
  mysql_rwlock_register(category, all_validate_password_rwlocks, count);
}

/*
  These are the 3 password policies that this plugin allow to set
  and configure as per the requirements.
*/

enum password_policy_enum {
  PASSWORD_POLICY_LOW,
  PASSWORD_POLICY_MEDIUM,
  PASSWORD_POLICY_STRONG
};

static const char *policy_names[] = {"LOW", "MEDIUM", "STRONG", NULL};

static TYPE_LIB password_policy_typelib_t = {array_elements(policy_names) - 1,
                                             "password_policy_typelib_t",
                                             policy_names, NULL};

typedef std::string string_type;
typedef std::set<string_type> set_type;
static set_type dictionary_words;

static int validate_password_length;
static int validate_password_number_count;
static int validate_password_mixed_case_count;
static int validate_password_special_char_count;
static ulong validate_password_policy;
static char *validate_password_dictionary_file;
static char *validate_password_dictionary_file_last_parsed = NULL;
static long long validate_password_dictionary_file_words_count = 0;
static bool check_user_name;

static SHOW_VAR validate_password_status_variables[] = {
    {"validate_password.dictionary_file_last_parsed",
     (char *)&validate_password_dictionary_file_last_parsed, SHOW_CHAR_PTR,
     SHOW_SCOPE_GLOBAL},
    {"validate_password.dictionary_file_words_count",
     (char *)&validate_password_dictionary_file_words_count, SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {NULL, NULL, SHOW_LONG, SHOW_SCOPE_GLOBAL}};

/**
  Activate the new dictionary

  Assigns a local list to the global variable,
  taking the correct locks in the process.
  Also updates the status variables.
  @param dict_words new dictionary words set

*/
static void dictionary_activate(set_type *dict_words) {
  time_t start_time;
  struct tm tm;
  std::stringstream ss; /* To store date & time in "YYYY-MM-DD HH:MM:SS" */

  /* fetch the start time */
  start_time = time(0);
  localtime_r(&start_time, &tm);

  ss << std::setfill('0') << std::setw(4) << tm.tm_year + 1900 << "-"
     << std::setfill('0') << std::setw(2) << tm.tm_mon + 1 << "-"
     << std::setfill('0') << std::setw(2) << tm.tm_mday << " "
     << std::setfill('0') << std::setw(2) << tm.tm_hour << ":"
     << std::setfill('0') << std::setw(2) << tm.tm_min << ":"
     << std::setfill('0') << std::setw(2) << tm.tm_sec;

  mysql_rwlock_wrlock(&LOCK_dict_file);
  std::swap(dictionary_words, *dict_words);
  validate_password_dictionary_file_words_count = dictionary_words.size();
  /*
    We are re-using 'validate_password_dictionary_file_last_parsed'
    so, we need to make sure to free the previously allocated memory.
  */
  if (validate_password_dictionary_file_last_parsed) {
    my_free(validate_password_dictionary_file_last_parsed);
    validate_password_dictionary_file_last_parsed = NULL;
  }
  validate_password_dictionary_file_last_parsed =
      (char *)my_malloc(PSI_NOT_INSTRUMENTED, ss.str().length() + 1, MYF(0));
  strncpy(validate_password_dictionary_file_last_parsed, ss.str().c_str(),
          ss.str().length() + 1);
  mysql_rwlock_unlock(&LOCK_dict_file);

  /* frees up the data just replaced */
  if (!dict_words->empty()) dict_words->clear();
}

/* To read dictionary file into std::set */
static void read_dictionary_file() {
  string_type words;
  set_type dict_words;
  std::streamoff file_length;

  if (validate_password_dictionary_file == NULL) {
    if (validate_password_policy == PASSWORD_POLICY_STRONG)
      LogEvent()
          .type(LOG_TYPE_ERROR)
          .prio(WARNING_LEVEL)
          .lookup(ER_VALIDATE_PWD_STRONG_POLICY_DICT_FILE_UNSPECIFIED);
    /* NULL is a valid value, despite the warning */
    dictionary_activate(&dict_words);
    return;
  }
  try {
    std::ifstream dictionary_stream(validate_password_dictionary_file);
    if (!dictionary_stream || !dictionary_stream.is_open()) {
      LogEvent()
          .type(LOG_TYPE_ERROR)
          .prio(WARNING_LEVEL)
          .lookup(ER_VALIDATE_PWD_DICT_FILE_OPEN_FAILED);
      return;
    }
    dictionary_stream.seekg(0, std::ios::end);
    file_length = dictionary_stream.tellg();
    dictionary_stream.seekg(0, std::ios::beg);
    if (file_length > MAX_DICTIONARY_FILE_LENGTH) {
      dictionary_stream.close();
      LogEvent()
          .type(LOG_TYPE_ERROR)
          .prio(WARNING_LEVEL)
          .lookup(ER_VALIDATE_PWD_DICT_FILE_TOO_BIG);
      return;
    }
    for (std::getline(dictionary_stream, words); dictionary_stream.good();
         std::getline(dictionary_stream, words))
      dict_words.insert(words);
    dictionary_stream.close();
    dictionary_activate(&dict_words);
  } catch (...)  // no exceptions !
  {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(WARNING_LEVEL)
        .lookup(ER_VALIDATE_PWD_DICT_FILE_NOT_SPECIFIED);
  }
}

/* Clear words from std::set */
static void free_dictionary_file() {
  mysql_rwlock_wrlock(&LOCK_dict_file);
  if (!dictionary_words.empty()) dictionary_words.clear();
  if (validate_password_dictionary_file_last_parsed) {
    my_free(validate_password_dictionary_file_last_parsed);
    validate_password_dictionary_file_last_parsed = NULL;
  }
  mysql_rwlock_unlock(&LOCK_dict_file);
}

/*
  Checks whether password or substring of password
  is present in dictionary file stored as std::set
*/
static int validate_dictionary_check(my_h_string password) {
  int length;
  char *buffer;
  my_h_string lower_string_handle;

  if (dictionary_words.empty()) return (1);

  /* New String is allocated */
  if (mysql_service_mysql_string_factory->create(&lower_string_handle)) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_STRING_HANDLER_MEM_ALLOCATION_FAILED);
    return (0);
  }
  if (mysql_service_mysql_string_case->tolower(&lower_string_handle,
                                               password)) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_STRING_CONV_TO_LOWERCASE_FAILED);
    return (0);
  }
  if (!(buffer = (char *)my_malloc(PSI_NOT_INSTRUMENTED, MAX_PASSWORD_LENGTH,
                                   MYF(0))))
    return (0);

  if (mysql_service_mysql_string_converter->convert_to_buffer(
          lower_string_handle, buffer, MAX_PASSWORD_LENGTH, "utf8")) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_STRING_CONV_TO_BUFFER_FAILED);
    return (0);
  }
  length = strlen(buffer);
  /* Free the allocated string */
  mysql_service_mysql_string_factory->destroy(lower_string_handle);
  int substr_pos = 0;
  int substr_length = length;
  string_type password_str = string_type((const char *)buffer, length);
  string_type password_substr;
  set_type::iterator itr;
  /*
    std::set as container stores the dictionary words,
    binary comparison between dictionary words and password
  */
  mysql_rwlock_rdlock(&LOCK_dict_file);
  while (substr_length >= MIN_DICTIONARY_WORD_LENGTH) {
    substr_pos = 0;
    while (substr_pos + substr_length <= length) {
      password_substr = password_str.substr(substr_pos, substr_length);
      itr = dictionary_words.find(password_substr);
      if (itr != dictionary_words.end()) {
        mysql_rwlock_unlock(&LOCK_dict_file);
        my_free(buffer);
        return (0);
      }
      substr_pos++;
    }
    substr_length--;
  }
  mysql_rwlock_unlock(&LOCK_dict_file);
  my_free(buffer);
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
static bool my_memcmp_reverse(const char *a, size_t a_len, const char *b,
                              size_t b_len) {
  const char *a_ptr;
  const char *b_ptr;

  if (a_len != b_len) return false;

  for (a_ptr = a, b_ptr = b + b_len - 1; b_ptr >= b; a_ptr++, b_ptr--)
    if (*a_ptr != *b_ptr) return false;
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

  @retval true name can be used
  @retval false name is invalid
*/
static bool is_valid_user(Security_context_handle ctx, const char *buffer,
                          int length, const char *field_name) {
  MYSQL_LEX_CSTRING user = {NULL, 0};

  if (mysql_service_mysql_security_context_options->get(ctx, field_name,
                                                        &user)) {
    assert(0); /* can't retrieve the logical_name from the ctx */
    return false;
  }

  /* lengths must match for the strings to match */
  if (user.length != (size_t)length) return true;
  /* empty strings turn the check off */
  if (user.length == 0) return true;
  /* empty strings turn the check off */
  if (!user.str) return true;

  return (0 != memcmp(buffer, user.str, user.length) &&
          !my_memcmp_reverse(user.str, user.length, buffer, length));
}

/**
  Check if the password is not the user name

  Helper function.
  Checks if the password supplied is valid to use by comparing it
  the effected and the login user names to it and to the reverse of it.
  logs an error to the error log if it can't pick up the names.

  @param thd MySQL THD object
  @param password the password handle
  @retval true The password can be used
  @retval false the password is invalid
*/
static bool is_valid_password_by_user_name(void *thd, my_h_string password) {
  char buffer[MAX_PASSWORD_LENGTH];
  int length;
  Security_context_handle ctx = NULL;

  if (!check_user_name) return true;

  if (mysql_service_mysql_thd_security_context->get(thd, &ctx) || !ctx) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(WARNING_LEVEL)
        .lookup(ER_VALIDATE_PWD_FAILED_TO_GET_SECURITY_CTX);
    return false;
  }

  if (mysql_service_mysql_string_converter->convert_to_buffer(
          password, buffer, MAX_PASSWORD_LENGTH, "utf8")) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(WARNING_LEVEL)
        .lookup(ER_VALIDATE_PWD_CONVERT_TO_BUFFER_FAILED);
    return false;
  }
  length = strlen(buffer);

  return is_valid_user(ctx, buffer, length, "user") &&
         is_valid_user(ctx, buffer, length, "priv_user");
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
static void readjust_validate_password_length() {
  int policy_password_length;

  /*
    Effective value of validate_password_length variable is:

    MAX(validate_password_length,
        (validate_password_number_count +
         2*validate_password_mixed_case_count +
         validate_password_special_char_count))
  */
  policy_password_length = (validate_password_number_count +
                            (2 * validate_password_mixed_case_count) +
                            validate_password_special_char_count);

  if (validate_password_length < policy_password_length) {
    /*
       Raise a warning that effective restriction on password
       length is changed.
    */
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(WARNING_LEVEL)
        .lookup(ER_VALIDATE_PWD_LENGTH_CHANGED, policy_password_length);
    validate_password_length = policy_password_length;
  }
}

/*
  Update function for validate_password_dictionary_file.
  If dictionary file is changed, this function will flush
  the cache and re-load the new dictionary file.
*/
static void dictionary_update(MYSQL_THD, SYS_VAR *, void *var_ptr,
                              const void *save) {
  *(const char **)var_ptr = *(const char **)save;
  read_dictionary_file();
}

/*
  update function for:
  1. validate_password_length
  2. validate_password_number_count
  3. validate_password_mixed_case_count
  4. validate_password_special_char_count
*/
static void length_update(MYSQL_THD, SYS_VAR *, void *var_ptr,
                          const void *save) {
  /* check if there is an actual change */
  if (*((int *)var_ptr) == *((int *)save)) return;

  /*
    set new value for system variable.
    Note that we need not know for which of the above mentioned
    variables, length_update() is called because var_ptr points
    to the location at which corresponding static variable is
    declared in this file.
  */
  *((int *)var_ptr) = *((int *)save);

  readjust_validate_password_length();
}

static int validate_password_policy_strength(void *thd, my_h_string password,
                                             int policy) {
  int has_digit = 0;
  int has_lower = 0;
  int has_upper = 0;
  int has_special_chars = 0;
  int n_chars = 0;
  my_h_string_iterator iter = NULL;
  int out_iter_char;
  bool out = false;

  if (mysql_service_mysql_string_iterator->iterator_create(password, &iter)) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(WARNING_LEVEL)
        .lookup(ER_VALIDATE_PWD_COULD_BE_NULL);
    return (0);
  }
  while (mysql_service_mysql_string_iterator->iterator_get_next(
             iter, &out_iter_char) != true) {
    n_chars++;
    if (policy > PASSWORD_POLICY_LOW) {
      if (!mysql_service_mysql_string_ctype->is_lower(iter, &out) && out)
        has_lower++;
      else if (!mysql_service_mysql_string_ctype->is_upper(iter, &out) && out)
        has_upper++;
      else if (!mysql_service_mysql_string_ctype->is_digit(iter, &out) && out)
        has_digit++;
      else
        has_special_chars++;
    }
  }

  mysql_service_mysql_string_iterator->iterator_destroy(iter);
  if (n_chars >= validate_password_length) {
    if (!is_valid_password_by_user_name(thd, password)) return (0);

    if (policy == PASSWORD_POLICY_LOW) return (1);
    if (has_upper >= validate_password_mixed_case_count &&
        has_lower >= validate_password_mixed_case_count &&
        has_special_chars >= validate_password_special_char_count &&
        has_digit >= validate_password_number_count) {
      if (policy == PASSWORD_POLICY_MEDIUM ||
          validate_dictionary_check(password))
        return (1);
    }
  }
  return (0);
}

/**
  Gets the password strength between (0-100)

  @param thd MYSQL THD object
  @param password Given Password
  @param [out] strength pointer to handle the strength of the given password.
               in the range of [0-100], where 0 is week password and
               100 is strong password
  @return Status of performed operation
  @return false success
  @return true failure
*/
DEFINE_BOOL_METHOD(validate_password_imp::get_strength,
                   (void *thd, my_h_string password, unsigned int *strength)) {
  int policy = 0;
  int n_chars = 0;
  int out_iter_char;
  my_h_string_iterator iter = NULL;

  *strength = 0;

  if (!is_valid_password_by_user_name(thd, password)) return true;

  if (mysql_service_mysql_string_iterator->iterator_create(password, &iter)) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(WARNING_LEVEL)
        .lookup(ER_VALIDATE_PWD_COULD_BE_NULL);
    return true;
  }
  while (mysql_service_mysql_string_iterator->iterator_get_next(
             iter, &out_iter_char) != true)
    n_chars++;

  mysql_service_mysql_string_iterator->iterator_destroy(iter);
  if (n_chars < MIN_DICTIONARY_WORD_LENGTH) return true;
  if (n_chars < validate_password_length) {
    *strength = PASSWORD_SCORE;
    return false;
  } else {
    policy = PASSWORD_POLICY_LOW;
    if (validate_password_policy_strength(thd, password,
                                          PASSWORD_POLICY_MEDIUM)) {
      policy = PASSWORD_POLICY_MEDIUM;
      if (validate_dictionary_check(password)) policy = PASSWORD_POLICY_STRONG;
    }
  }
  *strength = ((policy + 1) * PASSWORD_SCORE + PASSWORD_SCORE);
  return false;
}

/**
  Validates the strength of given password.

  @param thd MYSQL THD object
  @param password Given Password
  @return Status of performed operation
  @return false success valid password
  @return true failure invalid password
*/
DEFINE_BOOL_METHOD(validate_password_imp::validate,
                   (void *thd, my_h_string password)) {
  return (validate_password_policy_strength(thd, password,
                                            validate_password_policy) == 0);
}

int register_status_variables() {
  if (mysql_service_status_variable_registration->register_variable(
          (SHOW_VAR *)&validate_password_status_variables)) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_STATUS_VAR_REGISTRATION_FAILED);
    return 1;
  }
  return 0;
}

int register_system_variables() {
  INTEGRAL_CHECK_ARG(int) length, num_count, mixed_case_count, spl_char_count;
  length.def_val = 8;
  length.min_val = 0;
  length.max_val = 0;
  length.blk_sz = 0;
  if (mysql_service_component_sys_variable_register->register_variable(
          "validate_password", "length", PLUGIN_VAR_INT | PLUGIN_VAR_RQCMDARG,
          "Password validate length to check for minimum password_length", NULL,
          length_update, (void *)&length, (void *)&validate_password_length)) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_VARIABLE_REGISTRATION_FAILED,
                "validate_password.length");
    return 1;
  }

  num_count.def_val = 1;
  num_count.min_val = 0;
  num_count.max_val = 0;
  num_count.blk_sz = 0;
  if (mysql_service_component_sys_variable_register->register_variable(
          "validate_password", "number_count",
          PLUGIN_VAR_INT | PLUGIN_VAR_RQCMDARG,
          "password validate digit to ensure minimum numeric character in "
          "password",
          NULL, length_update, (void *)&num_count,
          (void *)&validate_password_number_count)) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_VARIABLE_REGISTRATION_FAILED,
                "validate_password.number_count");
    goto number_count;
  }

  mixed_case_count.def_val = 1;
  mixed_case_count.min_val = 0;
  mixed_case_count.max_val = 0;
  mixed_case_count.blk_sz = 0;
  if (mysql_service_component_sys_variable_register->register_variable(
          "validate_password", "mixed_case_count",
          PLUGIN_VAR_INT | PLUGIN_VAR_RQCMDARG,
          "Password validate mixed case to ensure minimum upper/lower case "
          "in password",
          NULL, length_update, (void *)&mixed_case_count,
          (void *)&validate_password_mixed_case_count)) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_VARIABLE_REGISTRATION_FAILED,
                "validate_password.mixed_case_count");
    goto mixed_case_count;
  }

  spl_char_count.def_val = 1;
  spl_char_count.min_val = 0;
  spl_char_count.max_val = 0;
  spl_char_count.blk_sz = 0;
  if (mysql_service_component_sys_variable_register->register_variable(
          "validate_password", "special_char_count",
          PLUGIN_VAR_INT | PLUGIN_VAR_RQCMDARG,
          "password validate special to ensure minimum special character "
          "in password",
          NULL, length_update, (void *)&spl_char_count,
          (void *)&validate_password_special_char_count)) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_VARIABLE_REGISTRATION_FAILED,
                "validate_password.special_char_count");
    goto special_char_count;
  }

  ENUM_CHECK_ARG(enum) enum_arg;
  enum_arg.def_val = PASSWORD_POLICY_MEDIUM;
  enum_arg.typelib = &password_policy_typelib_t;
  if (mysql_service_component_sys_variable_register->register_variable(
          "validate_password", "policy", PLUGIN_VAR_ENUM | PLUGIN_VAR_RQCMDARG,
          "password_validate_policy choosen policy to validate password "
          "possible values are LOW MEDIUM (default), STRONG",
          NULL, NULL, (void *)&enum_arg, (void *)&validate_password_policy)) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_VARIABLE_REGISTRATION_FAILED,
                "validate_password.policy");
    goto policy;
  }

  STR_CHECK_ARG(str) str_arg;
  str_arg.def_val = NULL;
  if (mysql_service_component_sys_variable_register->register_variable(
          "validate_password", "dictionary_file",
          PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_RQCMDARG,
          "password_validate_dictionary file to be loaded and check for "
          "password",
          NULL, dictionary_update, (void *)&str_arg,
          (void *)&validate_password_dictionary_file)) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_VARIABLE_REGISTRATION_FAILED,
                "validate_password.dictionary_file");
    goto dictionary_file;
  }

  BOOL_CHECK_ARG(bool) bool_arg;
  bool_arg.def_val = true;
  if (mysql_service_component_sys_variable_register->register_variable(
          "validate_password", "check_user_name",
          PLUGIN_VAR_BOOL | PLUGIN_VAR_RQCMDARG,
          "Check if the password matches the login or the effective "
          "user names or the reverse of them",
          NULL, NULL, (void *)&bool_arg, (void *)&check_user_name)) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_VARIABLE_REGISTRATION_FAILED,
                "validate_password.check_user_name");
    goto check_user_name;
  }
  return 0; /* All system variables registered successfully */
check_user_name:
  mysql_service_component_sys_variable_unregister->unregister_variable(
      "validate_password", "dictionary_file");
dictionary_file:
  mysql_service_component_sys_variable_unregister->unregister_variable(
      "validate_password", "policy");
policy:
  mysql_service_component_sys_variable_unregister->unregister_variable(
      "validate_password", "special_char_count");
special_char_count:
  mysql_service_component_sys_variable_unregister->unregister_variable(
      "validate_password", "mixed_case_count");
mixed_case_count:
  mysql_service_component_sys_variable_unregister->unregister_variable(
      "validate_password", "number_count");
number_count:
  mysql_service_component_sys_variable_unregister->unregister_variable(
      "validate_password", "length");
  return 1; /* register_variable() api failed for one of the system variable */
}

int unregister_status_variables() {
  if (mysql_service_status_variable_registration->unregister_variable(
          (SHOW_VAR *)&validate_password_status_variables)) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_STATUS_VAR_UNREGISTRATION_FAILED);
    return 1;
  }
  return 0;
}

int unregister_system_variables() {
  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "validate_password", "length")) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_VARIABLE_UNREGISTRATION_FAILED,
                "validate_password.length");
  }

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "validate_password", "number_count")) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_VARIABLE_UNREGISTRATION_FAILED,
                "validate_password.number_count");
  }

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "validate_password", "mixed_case_count")) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_VARIABLE_UNREGISTRATION_FAILED,
                "validate_password.mixed_case_count");
  }

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "validate_password", "special_char_count")) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_VARIABLE_UNREGISTRATION_FAILED,
                "validate_password.special_char_count");
  }

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "validate_password", "policy")) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_VARIABLE_UNREGISTRATION_FAILED,
                "validate_password.policy");
  }

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "validate_password", "dictionary_file")) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_VARIABLE_UNREGISTRATION_FAILED,
                "validate_password.dictionary_file");
  }

  if (mysql_service_component_sys_variable_unregister->unregister_variable(
          "validate_password", "check_user_name")) {
    LogEvent()
        .type(LOG_TYPE_ERROR)
        .prio(ERROR_LEVEL)
        .lookup(ER_VALIDATE_PWD_VARIABLE_UNREGISTRATION_FAILED,
                "validate_password.check_user_name");
  }
  return 0;
}

SERVICE_TYPE(log_builtins) * log_bi;
SERVICE_TYPE(log_builtins_string) * log_bs;

/**
  logger services initialization method for Component used when
  loading the Component.

  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool log_service_init() {
  log_bi = mysql_service_log_builtins;
  log_bs = mysql_service_log_builtins_string;

  return false;
}

/**
  logger services de-initialization method for Component used when
  unloading the Component.

  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool log_service_deinit() { return false; }

/**
  Initialization entry method for Component used when loading the Component.

  @return Status of performed operation
  @retval false success
  @retval true failure
*/
static mysql_service_status_t validate_password_init() {
  init_validate_password_psi_keys();
  mysql_rwlock_init(key_validate_password_LOCK_dict_file, &LOCK_dict_file);
  if (log_service_init() || register_system_variables() ||
      register_status_variables())
    return true;
  read_dictionary_file();
  /* Check if validate_password_length needs readjustment */
  readjust_validate_password_length();
  return false;
}

/**
  De-initialization method for Component used when unloading the Component.

  @return Status of performed operation
  @retval false success
  @retval true failure
*/
static mysql_service_status_t validate_password_deinit() {
  free_dictionary_file();
  mysql_rwlock_destroy(&LOCK_dict_file);
  if (unregister_system_variables() || unregister_status_variables() ||
      log_service_deinit())
    return true;
  return false;
}
/* This component provides an implementation for validate_password component
   only. */
BEGIN_SERVICE_IMPLEMENTATION(validate_password, validate_password)
validate_password_imp::validate,
    validate_password_imp::get_strength END_SERVICE_IMPLEMENTATION();

/* component provides: the validate_password service */
BEGIN_COMPONENT_PROVIDES(validate_password)
PROVIDES_SERVICE(validate_password, validate_password),
    END_COMPONENT_PROVIDES();

/* A block for specifying dependencies of this Component. Note that for each
  dependency we need to have a placeholder, a extern to placeholder in
  header file of the Component, and an entry on requires list below. */
REQUIRES_SERVICE_PLACEHOLDER(log_builtins);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_string);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_factory);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_case);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_converter);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_iterator);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_ctype);
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_register);
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_unregister);
REQUIRES_SERVICE_PLACEHOLDER(status_variable_registration);
REQUIRES_SERVICE_PLACEHOLDER(mysql_thd_security_context);
REQUIRES_SERVICE_PLACEHOLDER(mysql_security_context_options);

/* A list of dependencies.
   The dynamic_loader fetches the references for the below services at the
   component load time and disposes off them at unload.
*/
BEGIN_COMPONENT_REQUIRES(validate_password)
REQUIRES_SERVICE(registry), REQUIRES_SERVICE(log_builtins),
    REQUIRES_SERVICE(log_builtins_string),
    REQUIRES_SERVICE(mysql_string_factory), REQUIRES_SERVICE(mysql_string_case),
    REQUIRES_SERVICE(mysql_string_converter),
    REQUIRES_SERVICE(mysql_string_iterator),
    REQUIRES_SERVICE(mysql_string_ctype),
    REQUIRES_SERVICE(component_sys_variable_register),
    REQUIRES_SERVICE(component_sys_variable_unregister),
    REQUIRES_SERVICE(status_variable_registration),
    REQUIRES_SERVICE(mysql_thd_security_context),
    REQUIRES_SERVICE(mysql_security_context_options),
    REQUIRES_PSI_MEMORY_SERVICE, REQUIRES_MYSQL_RWLOCK_SERVICE,
    END_COMPONENT_REQUIRES();

/* component description */
BEGIN_COMPONENT_METADATA(validate_password)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"),
    METADATA("validate_password_service", "1"), END_COMPONENT_METADATA();

/* component declaration */
DECLARE_COMPONENT(validate_password, "mysql:validate_password")
validate_password_init, validate_password_deinit END_DECLARE_COMPONENT();

/* components contained in this library.
   for now assume that each library will have exactly one component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(validate_password)
    END_DECLARE_LIBRARY_COMPONENTS
