/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#define MYSQL_SERVER
#include <my_global.h>
#include <boost/optional/optional.hpp>
#include <sql_class.h> // THD

#define MAX_KEYRING_UDF_KEY_LENGTH_IN_BITS 16384
#define MAX_KEYRING_UDF_KEY_TEXT_LENGTH MAX_KEYRING_UDF_KEY_LENGTH_IN_BITS/8
#define KEYRING_UDF_KEY_TYPE_LENGTH 3

#ifdef WIN32
#define PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define PLUGIN_EXPORT extern "C"
#endif

static my_bool is_keyring_udf_initialized= FALSE;

static int keyring_udf_init(void *p)
{
  DBUG_ENTER("keyring_udf_init");
  is_keyring_udf_initialized= TRUE;
  DBUG_RETURN(0);
}

static int keyring_udf_deinit(void *p)
{
  DBUG_ENTER("keyring_udf_deinit");
  is_keyring_udf_initialized= FALSE;
  DBUG_RETURN(0);
}

struct st_mysql_daemon keyring_udf_decriptor=
{ MYSQL_DAEMON_INTERFACE_VERSION  };

/*
  Plugin library descriptor
*/

mysql_declare_plugin(keyring_udf)
{
  MYSQL_DAEMON_PLUGIN,
  &keyring_udf_decriptor,
  "keyring_udf",
  "Oracle Corporation",
  "Keyring UDF plugin",
  PLUGIN_LICENSE_GPL,
  keyring_udf_init,           /* Plugin Init */
  keyring_udf_deinit,         /* Plugin Deinit */
  0x0100 /* 1.0 */,
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL,                       /* config options                  */
  0,                          /* flags                           */
}
mysql_declare_plugin_end;

static my_bool get_current_user(std::string *current_user)
{
  THD *thd= current_thd;
  MYSQL_SECURITY_CONTEXT sec_ctx;
  LEX_CSTRING user, host;

  if (thd_get_security_context(thd, &sec_ctx) ||
      security_context_get_option(sec_ctx, "priv_user", &user) ||
      security_context_get_option(sec_ctx, "priv_host", &host))
    return TRUE;

  if(user.length)
    current_user->append(user.str, user.length);
  DBUG_ASSERT(host.length);
  current_user->append("@").append(host.str, host.length);

  return FALSE;
}

enum what_to_validate
{
  VALIDATE_KEY= 1,
  VALIDATE_KEY_ID= 2,
  VALIDATE_KEY_TYPE= 4,
  VALIDATE_KEY_LENGTH= 8
};

uint get_args_count_from_validation_request(int to_validate)
{
  uint args_count= 0;

  //Since to_validate is a bit mask - count the number of bits set
  for(; to_validate; to_validate >>= 1)
    if(to_validate & 1)
      ++args_count;

  return args_count;
}

static my_bool validate(UDF_ARGS *args, uint expected_arg_count,
                        int to_validate, char *message)
{
  THD *thd= current_thd;
  MYSQL_SECURITY_CONTEXT sec_ctx;
  my_svc_bool has_current_user_execute_privilege= 0;

  if(is_keyring_udf_initialized == FALSE)
  {
    strcpy(message, "This function requires keyring_udf plugin which is not installed."
                    " Please install keyring_udf plugin and try again.");
    return TRUE;
  }

  if (thd_get_security_context(thd, &sec_ctx) ||
      security_context_get_option(sec_ctx, "privilege_execute",
                                  &has_current_user_execute_privilege))
    return TRUE;

  if (has_current_user_execute_privilege == FALSE)
  {
    strcpy(message, "The user is not privileged to execute this function. "
                    "User needs to have EXECUTE permission.");
    return TRUE;
  }

  if (args->arg_count != expected_arg_count)
  {
    strcpy(message, "Mismatch in number of arguments to the function.");
    return TRUE;
  }

  if (to_validate & VALIDATE_KEY_ID && (args->args[0] == NULL ||
      args->arg_type[0] != STRING_RESULT))
  {
    strcpy(message, "Mismatch encountered. A string argument is expected "
      "for key id.");
    return TRUE;
  }

  if (to_validate & VALIDATE_KEY_TYPE && (args->args[1] == NULL ||
      args->arg_type[1] != STRING_RESULT))
  {
    strcpy(message, "Mismatch encountered. A string argument is expected "
      "for key type.");
    return TRUE;
  }

  if (to_validate & VALIDATE_KEY_LENGTH)
  {
    if(args->args[2] == NULL || args->arg_type[2] != INT_RESULT)
    {
      strcpy(message, "Mismatch encountered. An integer argument is expected "
        "for key length.");
      return TRUE;
    }
    long long key_length= *reinterpret_cast<long long*>(args->args[2]);

    if (key_length > MAX_KEYRING_UDF_KEY_TEXT_LENGTH)
    {
      sprintf(message, "%s%d", "The key is to long. The max length of the key is ",
        MAX_KEYRING_UDF_KEY_TEXT_LENGTH);
      return TRUE;
    }
  }

  if (to_validate & VALIDATE_KEY &&
      (args->args[2] == NULL || args->arg_type[2] != STRING_RESULT))
  {
    strcpy(message, "Mismatch encountered. A string argument is expected "
      "for key.");
    return TRUE;
  }
  return FALSE;
}

my_bool keyring_udf_func_init(UDF_INIT *initid, UDF_ARGS *args, char *message,
                              int to_validate,
                              const boost::optional<size_t> max_lenth_to_return,
                              const size_t size_of_memory_to_allocate)
{
  initid->ptr= NULL;
  uint expected_arg_count= get_args_count_from_validation_request(to_validate);

  if (validate(args, expected_arg_count , to_validate, message))
    return TRUE;

  if (max_lenth_to_return)
    initid->max_length= *max_lenth_to_return; //if no max_length_to_return passed to the function
                                              //it means that max_length stays default
  initid->maybe_null= 1;

  if (size_of_memory_to_allocate != 0)
  {
    initid->ptr= new(std::nothrow) char[size_of_memory_to_allocate];
    if (initid->ptr == NULL)
      return TRUE;
    else
      memset(initid->ptr, 0, size_of_memory_to_allocate);
  }

  return FALSE;
}

PLUGIN_EXPORT
my_bool keyring_key_store_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  return keyring_udf_func_init(initid, args, message, (VALIDATE_KEY_ID | VALIDATE_KEY_TYPE | VALIDATE_KEY), 1, 0);
}

PLUGIN_EXPORT
void keyring_key_store_deinit(UDF_INIT *initid)
{}

/**
  Implementation of the UDF:
  INT keyring_key_store(STRING key_id, STRING key_type, STRING key)
  @return 1 on success, NULL and error on failure
*/
PLUGIN_EXPORT
long long keyring_key_store(UDF_INIT *initid, UDF_ARGS *args, char *is_null,
                            char *error)
{
  std::string current_user;

  if(get_current_user(&current_user))
  {
    *error= 1;
    return 0;
  }

  if(my_key_store(args->args[0], args->args[1], current_user.c_str(),
                  args->args[2], strlen(args->args[2])))
  {
    my_error(ER_KEYRING_UDF_KEYRING_SERVICE_ERROR, MYF(0), "keyring_key_store");
    *error= 1;
    return 0;
  }

  // For the UDF 1 == success, 0 == failure.
  return 1;
}

static my_bool fetch(const char* function_name, char *key_id, char **a_key,
                     char **a_key_type, size_t *a_key_len)
{
  std::string current_user;
  if (get_current_user(&current_user))
    return TRUE;

  char *key_type= NULL, *key= NULL;
  size_t key_len= 0;

  if (my_key_fetch(key_id, &key_type, current_user.c_str(),
                   reinterpret_cast<void**>(&key), &key_len))
  {
    my_error(ER_KEYRING_UDF_KEYRING_SERVICE_ERROR, MYF(0), function_name);

    if (key != NULL)
      my_free(key);
    if (key_type != NULL)
      my_free(key_type);
    return TRUE;
  }

  DBUG_ASSERT((key == NULL && key_len == 0) || (key != NULL &&
            key_len <= MAX_KEYRING_UDF_KEY_TEXT_LENGTH && key_type != NULL &&
            strlen(key_type) <= KEYRING_UDF_KEY_TYPE_LENGTH));

  if (a_key != NULL)
    *a_key= key;
  else
    my_free(key);
  if (a_key_type != NULL)
    *a_key_type= key_type;
  else
    my_free(key_type);
  if (a_key_len != NULL)
    *a_key_len= key_len;

  return FALSE;
}

PLUGIN_EXPORT
my_bool keyring_key_fetch_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
 return keyring_udf_func_init(initid, args, message, VALIDATE_KEY_ID,
                               MAX_KEYRING_UDF_KEY_TEXT_LENGTH, MAX_KEYRING_UDF_KEY_TEXT_LENGTH);
}


PLUGIN_EXPORT
void keyring_key_fetch_deinit(UDF_INIT *initid)
{
  if (initid->ptr)
  {
    delete[] initid->ptr;
    initid->ptr= NULL;
  }
}

/**
  Implementation of the UDF:
  STRING keyring_key_fetch(STRING key_id)
  @return key on success, NULL if key does not exist, NULL and error on failure
*/
PLUGIN_EXPORT
char *keyring_key_fetch(UDF_INIT *initid, UDF_ARGS *args, char *result,
                        unsigned long *length, char *is_null, char *error)
{
  char *key= NULL;
  size_t key_len= 0;

  if (fetch("keyring_key_fetch", args->args[0], &key, NULL, &key_len))
  {
    if (key != NULL)
      my_free(key);
    *error= 1;
    return NULL;
  }

  if (key != NULL)
  {
    memcpy(initid->ptr, key, key_len);
    my_free(key);
  }
  else
    *is_null= 1;

  *length= key_len;
  *error= 0;
  return initid->ptr;
}

PLUGIN_EXPORT
my_bool keyring_key_type_fetch_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  return keyring_udf_func_init(initid, args, message, VALIDATE_KEY_ID,
                               KEYRING_UDF_KEY_TYPE_LENGTH, KEYRING_UDF_KEY_TYPE_LENGTH);
}


PLUGIN_EXPORT
void keyring_key_type_fetch_deinit(UDF_INIT *initid)
{
  if (initid->ptr)
  {
    delete[] initid->ptr;
    initid->ptr= NULL;
  }
}

/**
  Implementation of the UDF:
  STRING keyring_key_type_fetch(STRING key_id)
  @return key's type on success, NULL if key does not exist, NULL and error on failure
*/
PLUGIN_EXPORT
char *keyring_key_type_fetch(UDF_INIT *initid, UDF_ARGS *args, char *result,
                             unsigned long *length, char *is_null, char *error)
{
  char *key_type= NULL;
  if(fetch("keyring_key_type_fetch", args->args[0], NULL, &key_type, NULL))
  {
    if (key_type != NULL)
      my_free(key_type);
    *error= 1;
    return NULL;
  }

  if (key_type != NULL)
  {
    memcpy(initid->ptr, key_type, KEYRING_UDF_KEY_TYPE_LENGTH);
    *length= KEYRING_UDF_KEY_TYPE_LENGTH;
    my_free(key_type);
  }
  else
  {
    *is_null= 1;
    *length= 0;
  }

  *error= 0;
  return initid->ptr;
}

PLUGIN_EXPORT
my_bool keyring_key_length_fetch_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  return keyring_udf_func_init(initid, args, message, VALIDATE_KEY_ID,
                               boost::none, 0);
}


PLUGIN_EXPORT
void keyring_key_length_fetch_deinit(UDF_INIT *initid)
{
  if (initid->ptr)
  {
    delete[] initid->ptr;
    initid->ptr= NULL;
  }
}

/**
  Implementation of the UDF:
  INT keyring_key_length_fetch(STRING key_id)
  @return key's length on success, NULL if key does not exist, NULL and error on failure
*/
PLUGIN_EXPORT
long long keyring_key_length_fetch(UDF_INIT *initid, UDF_ARGS *args, char *is_null,
                                   char *error)
{
  size_t key_len= 0;
  char* key= NULL;

  *error= fetch("keyring_key_length_fetch", args->args[0], &key, NULL, &key_len);

  if (*error == 0 && key == NULL)
    *is_null= 1;

  if(key != NULL)
    my_free(key);

  // For the UDF 0 == failure.
  return (*error) ? 0 : key_len;
}

PLUGIN_EXPORT
my_bool keyring_key_remove_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  return keyring_udf_func_init(initid, args, message, VALIDATE_KEY_ID,
                               1, 0);
}

PLUGIN_EXPORT
void keyring_key_remove_deinit(UDF_INIT *initid)
{}

/**
  Implementation of the UDF:
  INT keyring_key_remove(STRING key_id)
  @return 1 on success, NULL on failure
*/
PLUGIN_EXPORT
long long keyring_key_remove(UDF_INIT *initid, UDF_ARGS *args, char *is_null,
                             char *error)
{
  std::string current_user;
  if (get_current_user(&current_user))
  {
    *error=1;
    return 0;
  }
  if (my_key_remove(args->args[0], current_user.c_str()))
  {
    my_error(ER_KEYRING_UDF_KEYRING_SERVICE_ERROR, MYF(0), "keyring_key_remove");
    *error= 1;
    return 0;
  }
  *error= 0;
  return 1;
}

PLUGIN_EXPORT
my_bool keyring_key_generate_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  return keyring_udf_func_init(initid, args, message,
                               (VALIDATE_KEY_ID | VALIDATE_KEY_TYPE | VALIDATE_KEY_LENGTH),
                               1, 0);
}

PLUGIN_EXPORT
void keyring_key_generate_deinit(UDF_INIT *initid)
{}

/**
  Implementation of the UDF:
  STRING keyring_key_generate(STRING key_id, STRING key_type, INTEGER key_length)
  @return 1 on success, NULL and error on failure
*/
PLUGIN_EXPORT
long long keyring_key_generate(UDF_INIT *initid, UDF_ARGS *args, char *is_null,
                               char *error)
{
  std::string current_user;
  if (get_current_user(&current_user))
    return 0;

  long long key_length= *reinterpret_cast<long long*>(args->args[2]);

  if (my_key_generate(args->args[0], args->args[1], current_user.c_str(),
                      key_length))
  {
    my_error(ER_KEYRING_UDF_KEYRING_SERVICE_ERROR, MYF(0), "keyring_key_generate");
    *error= 1;
    // For the UDF 1 == success, 0 == failure.
    return 0;
  }
  return 1;
}
