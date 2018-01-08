/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <mysql/components/my_service.h>
#include <mysql/components/services/dynamic_privilege.h>
#include <mysql/plugin_audit.h>
#include <mysql/psi/mysql_memory.h>
#include <mysql/psi/mysql_rwlock.h>
#include <mysql/service_locking.h>
#include <sys/types.h>
#include <algorithm>
#include <atomic>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "errmsg.h"
#include "lex_string.h"
#include "m_string.h"
#include "map_helpers.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_psi_config.h"
#include "sql/auth/auth_acls.h"
#include "sql/current_thd.h"
#include "sql/derror.h"
#include "sql/locking_service.h"
#include "sql/sql_class.h"

#ifdef WIN32
#define PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define PLUGIN_EXPORT extern "C"
#endif

using std::pair;
using std::sort;
using std::string;
using std::vector;

// This global value is initiated with 1 and the corresponding session
// value with 0. Hence Every session must compare its tokens with the
// global values when it runs its very first query.
static std::atomic<int64> session_number{1};
static size_t vtoken_string_length;

#define VTOKEN_LOCKS_NAMESPACE "version_token_locks"

#define LONG_TIMEOUT ((ulong) 3600L*24L*365L)

PSI_memory_key key_memory_vtoken;

static malloc_unordered_map<string, string> *version_tokens_hash;

/**
  Utility class implementing an atomic boolean on top of an int32

  The mysys lib does not support atomic booleans.
*/
class atomic_boolean
{
  /** constants for true and false */
  static const int m_true, m_false;
  /** storage for the boolean's current value */
  std::atomic<int32> m_value;
public:

  /**
    Constructs a new atomic_boolean.

    @param value  The value to initialize the boolean with.
  */
  atomic_boolean(bool value= false) : m_value(value ? m_true : m_false)
  {}

  /**
    Checks if the atomic boolean has a certain value

    if used without an argument checks if the atomic boolean is on.

    @param value  the value to check for
    @retval true  the atomic boolean value matches the argument value
    @retval false the atomic boolean value is different from the argument value
  */
  bool is_set(bool value= true)
  {
    int32 cmp= value ? m_true : m_false, actual_value;

    actual_value= m_value.load();

    return actual_value == cmp;
  }

  /**
    Sets a new value for the atomic boolean

    @param new_value value to set
  */
  void set(bool new_value)
  {
    int32 new_val= new_value ? m_true : m_false;
    m_value.store(new_val);
  }
};

const int atomic_boolean::m_true= 0;
const int atomic_boolean::m_false= 1;


/**
  State of the version tokens hash global structure

  Needed since both the UDFs and the plugin are using the global
  and thus it can't be freed until the last UDF or plugin has been unloaded.
*/
static atomic_boolean version_tokens_hash_inited;

static MYSQL_THDVAR_ULONG(session_number,
                          PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY |
                          PLUGIN_VAR_NOPERSIST,
                          "Version number to assist with session tokens check",
                          NULL, NULL, 0L, 0, ((ulong) -1), 0);


static void update_session_version_tokens(MYSQL_THD thd,
                                          struct st_mysql_sys_var*,
					  void *var_ptr, const void *save)
{
  THDVAR(thd, session_number)= 0;
  *(char **) var_ptr= *(char **) save;
}



static MYSQL_THDVAR_STR(session,
                        PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
                        "Holds the session value for version tokens",
                        NULL, update_session_version_tokens, NULL);


// Lock to be used for global variable hash.
mysql_rwlock_t LOCK_vtoken_hash;

#ifdef HAVE_PSI_INTERFACE
PSI_rwlock_key key_LOCK_vtoken_hash;

static PSI_rwlock_info all_vtoken_rwlocks[]=
{
  {&key_LOCK_vtoken_hash, "LOCK_vtoken_hash", 0, 0, PSI_DOCUMENT_ME},
};

static PSI_memory_info all_vtoken_memory[]=
{
  {&key_memory_vtoken, "vtoken", 0, 0, PSI_DOCUMENT_ME}
};

// Function to register the lock
static void vtoken_init_psi_keys(void)
{
  const char* category= "vtoken";
  int count;

  count= static_cast<int>(array_elements(all_vtoken_rwlocks));
  mysql_rwlock_register(category, all_vtoken_rwlocks, count);

  count= static_cast<int>(array_elements(all_vtoken_memory));
  mysql_memory_register(category, all_vtoken_memory, count);
}

#endif /* HAVE_PSI_INTERFACE */

static bool is_blank_string(char *input)
{
  LEX_STRING input_lex;
  input_lex.str= input;
  input_lex.length= strlen(input);

  trim_whitespace(&my_charset_bin, &input_lex);

  if (input_lex.length == 0)
    return true;
  else
    return false;
}

/**
  Check if user either has SUPER or VERSION_TOKEN_ADMIN privileges
  @param thd Thread handle
  
  @return succcess state
    @retval true User has the required privileges
    @retval false User has not the required privileges
*/

bool has_required_privileges(THD *thd)
{
  if (!(thd->security_context()->check_access(SUPER_ACL)))
  {
    SERVICE_TYPE(registry) *r= mysql_plugin_registry_acquire();
    bool has_admin_privilege= false;
    {
      my_service<SERVICE_TYPE(global_grants_check)>
        service("global_grants_check.mysql_server", r);
      if (service.is_valid())
      {
        has_admin_privilege=
          service->
            has_global_grant(reinterpret_cast<Security_context_handle>(thd->security_context()),
                             STRING_WITH_LEN("VERSION_TOKEN_ADMIN"));
      }
    } // end scope
    mysql_plugin_registry_release(r);
    return (has_admin_privilege);
  } // end if
  return true;
}

static void set_vtoken_string_length()
{
  size_t str_size= 0;
  for (const auto &key_and_value : *version_tokens_hash)
  {
    str_size+= key_and_value.first.size() + 1;
    str_size+= key_and_value.second.size() + 1;
  }
  vtoken_string_length= str_size;
}


// UDF
PLUGIN_EXPORT bool version_tokens_set_init(UDF_INIT *initid, UDF_ARGS *args,
                                              char *message);
PLUGIN_EXPORT char *version_tokens_set(UDF_INIT *initid, UDF_ARGS *args,
                                       char *result, unsigned long *length,
				       char *null_value, char *error);

PLUGIN_EXPORT bool version_tokens_show_init(UDF_INIT *initid, UDF_ARGS *args,
                                               char *message);
PLUGIN_EXPORT void version_tokens_show_deinit(UDF_INIT *initid);
PLUGIN_EXPORT char *version_tokens_show(UDF_INIT *initid, UDF_ARGS *args,
                                        char *result, unsigned long *length,
					char *null_value, char *error);

PLUGIN_EXPORT bool version_tokens_edit_init(UDF_INIT *initid, UDF_ARGS *args,
                                               char *message);
PLUGIN_EXPORT char *version_tokens_edit(UDF_INIT *initid, UDF_ARGS *args,
                                        char *result, unsigned long *length,
					char *null_value, char *error);

PLUGIN_EXPORT bool version_tokens_delete_init(UDF_INIT *initid,
                                                 UDF_ARGS *args, char *message);
PLUGIN_EXPORT char *version_tokens_delete(UDF_INIT *initid, UDF_ARGS *args,
                                          char *result, unsigned long *length,
					  char *null_value, char *error);
PLUGIN_EXPORT bool version_tokens_lock_shared_init(
  UDF_INIT *initid, UDF_ARGS *args, char *message);
PLUGIN_EXPORT long long version_tokens_lock_shared(
  UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
PLUGIN_EXPORT bool version_tokens_lock_exclusive_init(
  UDF_INIT *initid, UDF_ARGS *args, char *message);
PLUGIN_EXPORT long long version_tokens_lock_exclusive(
  UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
PLUGIN_EXPORT bool version_tokens_unlock_init(
  UDF_INIT *initid, UDF_ARGS *args, char *message);
PLUGIN_EXPORT long long version_tokens_unlock(
  UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);

enum command {
  SET_VTOKEN= 0,
  EDIT_VTOKEN,
  CHECK_VTOKEN
};

/**
  @brief Parses the list of version tokens and either updates the global
         list with the input or checks the input against the global according
	 to which function the caller is.

  @param [in] input  List of semicolon separated token name/value pairs
  @param [in] type   Helps determining the caller function.

  @return (error)    -1 in case of error.
  @return (success)  Number of tokens updated/set on EDIT_VTOKEN and SET_VTOKEN.
	             0 in case of CHECK_VTOKEN.

  TODO: Add calls to get_lock services in CHECK_VTOKEN.
*/
static int parse_vtokens(char *input, enum command type)
{
  char *token, *lasts_token= NULL;
  const char *separator= ";";
  int result= 0;
  THD *thd= current_thd;
  ulonglong thd_session_number= THDVAR(thd, session_number);
  ulonglong tmp_token_number= (ulonglong) session_number.load();

  bool vtokens_unchanged= (thd_session_number == tmp_token_number);

  token= my_strtok_r(input, separator, &lasts_token);

  while (token)
  {
    const char *equal= "=";
    char *lasts_val= NULL;
    LEX_STRING token_name, token_val;

    if (is_blank_string(token))
    {
      token= my_strtok_r(NULL, separator, &lasts_token);
      continue;
    }

    token_name.str= my_strtok_r(token, equal, &lasts_val);
    token_val.str= lasts_val;

    token_name.length= token_name.str ? strlen(token_name.str) : 0;
    token_val.length= lasts_val ? strlen(lasts_val):0;
    trim_whitespace(&my_charset_bin, &token_name);
    trim_whitespace(&my_charset_bin, &token_val);

    if ((token_name.length == 0) || (token_val.length == 0))
    {
      switch (type)
      {
	case CHECK_VTOKEN:
	{
	  if (!thd->get_stmt_da()->is_set())
	    thd->get_stmt_da()->set_error_status(ER_ACCESS_DENIED_ERROR,
						 "Empty version token "
						 "name/value encountered",
						 "42000");
	  return -1;
	}
	case SET_VTOKEN:
	case EDIT_VTOKEN:
	{
	  push_warning(thd, Sql_condition::SL_WARNING, 42000,
	               "Invalid version token pair encountered. The list "
		       "provided is only partially updated.");
	}
      }
      return result;
    }

    if (token_name.length > 64)
    {
      switch (type)
      {
	case CHECK_VTOKEN:
	{
	  if (!thd->get_stmt_da()->is_set())
	    thd->get_stmt_da()->set_error_status(ER_ACCESS_DENIED_ERROR,
						 "Lengthy version token name "
						 "encountered.  Maximum length "
						 "allowed for a token name is "
						 "64 characters.",
						 "42000");
	  return -1;
	}
	case SET_VTOKEN:
	case EDIT_VTOKEN:
	{
	  push_warning(thd, Sql_condition::SL_WARNING, 42000,
	               "Lengthy version token name encountered. Maximum length "
		       "allowed for a token name is 64 characters. The list "
		       "provided is only partially updated.");
	}
      }
      return result;
    }

    switch (type) {

      case SET_VTOKEN:
      case EDIT_VTOKEN:
      {
        (*version_tokens_hash)[to_string(token_name)]= to_string(token_val);
	result++;
      }
	break;

      case CHECK_VTOKEN:
      {
        char error_str[MYSQL_ERRMSG_SIZE];
        if (!mysql_acquire_locking_service_locks(thd, VTOKEN_LOCKS_NAMESPACE,
	                                         (const char **) &(token_name.str), 1,
					         LOCKING_SERVICE_READ, LONG_TIMEOUT) &&
            !vtokens_unchanged)
	{
          auto it= version_tokens_hash->find(to_string(token_name));
	  if (it != version_tokens_hash->end())
	  {
            if (it->second != to_string(token_val))
	    {

              if (!thd->get_stmt_da()->is_set())
              {
                my_snprintf(error_str, sizeof(error_str),
                            ER_THD(thd, ER_VTOKEN_PLUGIN_TOKEN_MISMATCH),
                            (int) token_name.length, token_name.str,
                            (int) it->second.size(),
                            it->second.data());

                thd->get_stmt_da()->set_error_status(ER_VTOKEN_PLUGIN_TOKEN_MISMATCH,
                                                     (const char *) error_str,
                                                     "42000");
              }
	      return -1;
	    }
	  }
	  else
	  {
            if (!thd->get_stmt_da()->is_set())
            {
              my_snprintf(error_str, sizeof(error_str),
                          ER_THD(thd, ER_VTOKEN_PLUGIN_TOKEN_NOT_FOUND),
                          (int) token_name.length, token_name.str);

              thd->get_stmt_da()->set_error_status(ER_VTOKEN_PLUGIN_TOKEN_NOT_FOUND,
                                                   (const char *) error_str,
                                                   "42000");
            }
	    return -1;
	  }
	}
      }
    }
    token= my_strtok_r(NULL, separator, &lasts_token);
  }

  if (type == CHECK_VTOKEN)
  {
    THDVAR(thd, session_number)= (long) tmp_token_number;
  }

  return result;
}


/**
  Audit API entry point for the version token pluign

  Plugin audit function to compare session version tokens with
  the global ones.
  At the start of each query (MYSQL_AUDIT_GENERAL_LOG
  currently) if there's a session version token vector it will
  acquire the GET_LOCK shared locks for the session version tokens
  and then will try to find them in the global version lock and
  compare their values with the ones found. Throws errors if not
  found or the version values do not match. See parse_vtokens().
  At query end (MYSQL_AUDIT_GENERAL_STATUS currently) it releases
  the GET_LOCK shared locks it has aquired.

  @param thd          The current thread
  @param event_class  audit API event class
  @param event        pointer to the audit API event data
*/
static
int version_token_check(MYSQL_THD thd,
                        mysql_event_class_t event_class MY_ATTRIBUTE((unused)),
                        const void *event)
{
  char *sess_var;

  const struct mysql_event_general *event_general=
    (const struct mysql_event_general *) event;
  const uchar *command= (const uchar *) event_general->general_command.str;
  size_t length= event_general->general_command.length;

  DBUG_ASSERT(event_class == MYSQL_AUDIT_GENERAL_CLASS);

  switch (event_general->event_subclass)
  {
    case MYSQL_AUDIT_GENERAL_LOG:
    {
      /* Ignore all commands but COM_QUERY and COM_STMT_PREPARE */
      if (0 != my_charset_latin1.coll->strnncoll(&my_charset_latin1,
                                                 command, length,
                                                 (const uchar *) STRING_WITH_LEN("Query"),
                                                 0) &&
          0 != my_charset_latin1.coll->strnncoll(&my_charset_latin1,
                                                 command, length,
                                                 (const uchar *) STRING_WITH_LEN("Prepare"),
                                                 0))
        return 0;


      if (THDVAR(thd, session))
	sess_var= my_strndup(key_memory_vtoken,
			     THDVAR(thd, session),
                             strlen(THDVAR(thd, session)),
			     MYF(MY_FAE));
      else
	return 0;

      // Lock the hash before checking for values.
      mysql_rwlock_rdlock(&LOCK_vtoken_hash);

      parse_vtokens(sess_var, CHECK_VTOKEN);

      // Unlock hash
      mysql_rwlock_unlock(&LOCK_vtoken_hash);
      my_free(sess_var);
      break;
    }
    case MYSQL_AUDIT_GENERAL_STATUS:
    {
      /*
        Release locks only if the session variable is set.

        This relies on the fact that MYSQL_AUDIT_GENERAL_STATUS
        is always generated at the end of query execution.
      */
      if (THDVAR(thd, session))
        mysql_release_locking_service_locks(NULL, VTOKEN_LOCKS_NAMESPACE);
      break;
    }
    default:
      break;
  }

  return 0;
}


/**
  Helper class to dispose of the rwlocks at DLL/so unload.

  We can't release the rwlock at plugin or UDF unload since we're using it
  to synchronize both.

  So we need to rely on the shared object deinitialization function to
  dispose of lock up.

  For that we declare a helper class with a destructor that disposes of the
  global object and declare one global variable @ref cleanup_lock of that
  class and expect the C library to call the destructor when unloading
  the DLL/so.
*/
class vtoken_lock_cleanup
{
  atomic_boolean activated;
public:
  vtoken_lock_cleanup()
  {};
  ~vtoken_lock_cleanup()
  {
    if (activated.is_set())
      mysql_rwlock_destroy(&LOCK_vtoken_hash);
  }
  void activate()
  {
    activated.set(true);
  }

  bool is_active()
  {
    return activated.is_set();
  }
};

/**
  A single global variable to invoke the destructor.
  See @ref vtoken_lock_cleanup.
*/
static vtoken_lock_cleanup cleanup_lock;



static struct st_mysql_audit version_token_descriptor=
{
  MYSQL_AUDIT_INTERFACE_VERSION,                       /* interface version */
  NULL,                                                /* release_thd()     */
  version_token_check,                                 /* event_notify()    */
  { (unsigned long) MYSQL_AUDIT_GENERAL_ALL, }         /* class mask        */
};

/** Plugin init. */
static int version_tokens_init(void *arg MY_ATTRIBUTE((unused)))
{
#ifdef HAVE_PSI_INTERFACE
  // Initialize psi keys.
  vtoken_init_psi_keys();
#endif /* HAVE_PSI_INTERFACE */

  version_tokens_hash=
    new malloc_unordered_map<string, string>{key_memory_vtoken};
  version_tokens_hash_inited.set(true);

  if (!cleanup_lock.is_active())
  {
    // Lock for hash.
    mysql_rwlock_init(key_LOCK_vtoken_hash, &LOCK_vtoken_hash);
    // Lock for version number.
    cleanup_lock.activate();
  }
  bool ret= false;
  SERVICE_TYPE(registry) *r= mysql_plugin_registry_acquire();
  {
    my_service<SERVICE_TYPE(dynamic_privilege_register)>
      service("dynamic_privilege_register.mysql_server", r);
    if (service.is_valid())
    {
      ret |=
        service->register_privilege(STRING_WITH_LEN("VERSION_TOKEN_ADMIN"));
    }
  } // end scope
  mysql_plugin_registry_release(r);
  return (ret?1:0);
}

/** Plugin deinit. */
static int version_tokens_deinit(void *arg MY_ATTRIBUTE((unused)))
{
  SERVICE_TYPE(registry) *r= mysql_plugin_registry_acquire();
  {
    my_service<SERVICE_TYPE(dynamic_privilege_register)>
      service("dynamic_privilege_register.mysql_server", r);
    if (service.is_valid())
    {
      service->unregister_privilege(STRING_WITH_LEN("VERSION_TOKEN_ADMIN"));
    }
  }
  mysql_plugin_registry_release(r);
  mysql_rwlock_wrlock(&LOCK_vtoken_hash);

  delete version_tokens_hash;
  version_tokens_hash= nullptr;
  version_tokens_hash_inited.set(false);
  mysql_rwlock_unlock(&LOCK_vtoken_hash);

  return 0;
}

static struct st_mysql_sys_var* system_variables[]={
  MYSQL_SYSVAR(session_number),
  MYSQL_SYSVAR(session),
  NULL
};


// Declare plugin
mysql_declare_plugin(version_tokens)
{
  MYSQL_AUDIT_PLUGIN,                /* type                            */
  &version_token_descriptor,         /* descriptor                      */
  "version_tokens",                  /* name                            */
  "Oracle Corp",                     /* author                          */
  "version token check",             /* description                     */
  PLUGIN_LICENSE_GPL,
  version_tokens_init,               /* init function (when loaded)     */
  NULL,                              /* cwcheck uninstall function      */
  version_tokens_deinit,             /* deinit function (when unloaded) */
  0x0101,                            /* version          */
  NULL,                              /* status variables */
  system_variables,                  /* system variables */
  NULL,
  0
}
mysql_declare_plugin_end;


/**
  A function to check if the hash is inited and generate an error.

  To be called while holding LOCK_vtoken_hash

  @param function  the UDF function name for the error message
  @param error     the UDF error pointer to set
  @retval false    hash not initialized. Error set. Bail out.
  @retval true     All good. Go on.
*/

static bool is_hash_inited(const char *function, char *error)
{

  if (!version_tokens_hash_inited.is_set())
  {
    my_error(ER_CANT_INITIALIZE_UDF, MYF(0), function,
             "version_token plugin is not installed.");
    *error= 1;
    return false;
  }
  return true;
}



/*
  Below is the UDF for setting global list of version tokens.
  Input must be provided as semicolon separated tokens.

  Function signature:

  VERSION_TOKENS_SET(tokens_list varchar)
*/

PLUGIN_EXPORT bool version_tokens_set_init(UDF_INIT*, UDF_ARGS* args,
                                           char* message)
{
  THD *thd= current_thd;

  if (!has_required_privileges(thd))
  {
    my_stpcpy(message, "The user is not privileged to use this function.");
    return true;
  }

  if (!version_tokens_hash_inited.is_set())
  {
    my_stpcpy(message, "version_token plugin is not installed.");
    return true;
  }

  if (args->arg_count != 1 || args->arg_type[0] != STRING_RESULT)
  {
    my_stpcpy(message, "Wrong arguments provided for the function.");
    return true;
  }

  return false;
}

PLUGIN_EXPORT char *version_tokens_set(UDF_INIT*, UDF_ARGS *args,
                                       char *result, unsigned long *length,
				       char*, char *error)
{
  char *hash_str;
  int len= args->lengths[0];
  int vtokens_count= 0;
  std::stringstream ss;

  mysql_rwlock_wrlock(&LOCK_vtoken_hash);
  if (!is_hash_inited("version_tokens_set", error))
  {
    mysql_rwlock_unlock(&LOCK_vtoken_hash);
    return NULL;
  }
  if (len > 0)
  {
    // Separate copy for values to be inserted in hash.
    hash_str= (char *)my_malloc(key_memory_vtoken,
	                        len+1, MYF(MY_WME));

    if (!hash_str)
    {
      *error= 1;
      mysql_rwlock_unlock(&LOCK_vtoken_hash);
      return NULL;
    }
    memcpy(hash_str, args->args[0], len);
    hash_str[len]= 0;

    // Hash built with its own copy of string.
    version_tokens_hash->clear();
    vtokens_count= parse_vtokens(hash_str, SET_VTOKEN);
    ss << vtokens_count << " version tokens set.";

    my_free(hash_str);

  }
  else
  {
    version_tokens_hash->clear();
    ss << "Version tokens list cleared.";
  }
  set_vtoken_string_length();

  ++session_number;

  mysql_rwlock_unlock(&LOCK_vtoken_hash);

  ss.getline(result, MAX_FIELD_WIDTH, '\0');
  *length= (unsigned long) ss.gcount();

  return result;
}


/*
  Below is the UDF for updating global version tokens.
  Tokens to be updated must be provided as semicolon
  separated tokens.

  Function signature:

  VERSION_TOKENS_EDIT(tokens_list varchar)
*/

PLUGIN_EXPORT bool version_tokens_edit_init(UDF_INIT*, UDF_ARGS *args,
                                            char *message)
{
  THD *thd= current_thd;

  if (!version_tokens_hash_inited.is_set())
  {
    my_stpcpy(message, "version_token plugin is not installed.");
    return true;
  }

  if (!has_required_privileges(thd))
  {
    my_stpcpy(message, "The user is not privileged to use this function.");
    return true;
  }

  if (args->arg_count != 1 || args->arg_type[0] != STRING_RESULT)
  {
    my_stpcpy(message, "Wrong arguments provided for the function.");
    return true;
  }

  return false;
}

PLUGIN_EXPORT char *version_tokens_edit(UDF_INIT*, UDF_ARGS *args,
                                        char *result, unsigned long *length,
					char*, char *error)
{
  char *hash_str;
  int len= args->lengths[0];
  std::stringstream ss;
  int vtokens_count= 0;

  if (len > 0)
  {
    // Separate copy for values to be inserted in hash.
    hash_str= (char *)my_malloc(key_memory_vtoken,
	                        len+1, MYF(MY_WME));

    if (!hash_str)
    {
      *error= 1;
      return NULL;
    }
    memcpy(hash_str, args->args[0], len);
    hash_str[len]= 0;

    // Hash built with its own copy of string.
    mysql_rwlock_wrlock(&LOCK_vtoken_hash);
    if (!is_hash_inited("version_tokens_edit", error))
    {
      mysql_rwlock_unlock(&LOCK_vtoken_hash);
      return NULL;
    }

    vtokens_count= parse_vtokens(hash_str, EDIT_VTOKEN);

    set_vtoken_string_length();

    if (vtokens_count)
      ++session_number;

    mysql_rwlock_unlock(&LOCK_vtoken_hash);
    my_free(hash_str);
  }
  ss << vtokens_count << " version tokens updated.";
  ss.getline(result, MAX_FIELD_WIDTH, '\0');
  *length= (unsigned long) ss.gcount();

  return result;
}


/*
  Below is the UDF for deleting selected global version tokens.
  Names of tokens to be deleted must be provided in a semicolon separated list.

  Function signature:

  VERSION_TOKENS_DELETE(tokens_list varchar)
*/

PLUGIN_EXPORT bool version_tokens_delete_init(UDF_INIT*,
                                              UDF_ARGS *args, char *message)
{
  THD *thd= current_thd;

  if (!version_tokens_hash_inited.is_set())
  {
    my_stpcpy(message, "version_token plugin is not installed.");
    return true;
  }

  if (!has_required_privileges(thd))
  {
    my_stpcpy(message, "The user is not privileged to use this function.");
    return true;
  }

  if (args->arg_count != 1 || args->arg_type[0] != STRING_RESULT)
  {
    my_stpcpy(message, "Wrong arguments provided for the function.");
    return true;
  }

  return false;
}

PLUGIN_EXPORT char *version_tokens_delete(UDF_INIT*, UDF_ARGS *args,
                                          char *result, unsigned long *length,
					  char*, char *error)
{
  const char *arg= args->args[0];
  std::stringstream ss;
  int vtokens_count= 0;

  if (args->lengths[0] > 0)
  {
    char *input;
    const char *separator= ";";
    char *token, *lasts_token= NULL;

    if (NULL == (input= my_strdup(key_memory_vtoken, arg, MYF(MY_WME))))
    {
      *error= 1;
      return NULL;
    }

    mysql_rwlock_wrlock(&LOCK_vtoken_hash);
    if (!is_hash_inited("version_tokens_delete", error))
    {
      mysql_rwlock_unlock(&LOCK_vtoken_hash);
      return NULL;
    }

    token= my_strtok_r(input, separator, &lasts_token);

    while (token)
    {
      LEX_STRING st={ token, strlen(token) };

      trim_whitespace(&my_charset_bin, &st);

      if (st.length)
      {
        vtokens_count+= version_tokens_hash->erase(to_string(st));
      }

      token= my_strtok_r(NULL, separator, &lasts_token);
    }

    set_vtoken_string_length();

    if (vtokens_count)
    {
      ++session_number;
    }

    mysql_rwlock_unlock(&LOCK_vtoken_hash);
    my_free(input);
  }

  ss << vtokens_count << " version tokens deleted.";

  ss.getline(result, MAX_FIELD_WIDTH, '\0');
  *length= (unsigned long) ss.gcount();
  return result;
}


/*
  Below is the UDF for showing the existing list of global version tokens.
  Names of tokens will be returned in a semicolon separated list.

  Function signature:

  VERSION_TOKENS_SHOW()
*/

PLUGIN_EXPORT bool version_tokens_show_init(UDF_INIT *initid, UDF_ARGS *args,
                                               char *message)
{
  size_t str_size= 0;
  char *result_str;

  THD *thd= current_thd;
  if (!has_required_privileges(thd))
  {
        my_stpcpy(message, "The user is not privileged to use this function.");
        return true;
  }

  if (args->arg_count != 0)
  {
    my_stpcpy(message, "This function does not take any arguments.");
    return true;
  }

  mysql_rwlock_rdlock(&LOCK_vtoken_hash);
  if (!version_tokens_hash_inited.is_set())
  {
    my_stpcpy(message, "version_token plugin is not installed.");
    mysql_rwlock_unlock(&LOCK_vtoken_hash);
    return true;
  }

  str_size= vtoken_string_length;

  if (str_size)
  {
    str_size++;
    initid->ptr= (char *)my_malloc(key_memory_vtoken,
	                           str_size, MYF(MY_WME));

    if (initid->ptr == NULL)
    {
      my_stpcpy(message, "Not enough memory available.");
      mysql_rwlock_unlock(&LOCK_vtoken_hash);
      return true;
    }

    result_str= initid->ptr;

    // This sort isn't required, but makes for easier testing.
    vector<pair<string, string>> sorted_version_tokens
      (version_tokens_hash->begin(), version_tokens_hash->end());
    sort(sorted_version_tokens.begin(), sorted_version_tokens.end());

    for (const auto &key_and_value : sorted_version_tokens)
    {
      const string &token_name= key_and_value.first;
      const string &token_val= key_and_value.second;

      memcpy(result_str, token_name.data(), token_name.size());

      result_str+= token_name.size();

      memcpy(result_str, "=", 1);

      result_str++;

      memcpy(result_str, token_val.data(), token_val.size());

      result_str+= token_val.size();

      memcpy(result_str, ";", 1);

      result_str++;
    }

    initid->ptr[str_size-1]= '\0';
  }
  else
    initid->ptr= NULL;
  mysql_rwlock_unlock(&LOCK_vtoken_hash);

  return false;
}

PLUGIN_EXPORT void version_tokens_show_deinit(UDF_INIT *initid)
{
  if (initid->ptr)
    my_free(initid->ptr);
}

PLUGIN_EXPORT char *version_tokens_show(UDF_INIT *initid, UDF_ARGS*,
                                        char*, unsigned long *length,
					char*, char*)
{
  char *result_str= initid->ptr;
  *length= 0;

  if (!result_str)
    return NULL;

  *length= (unsigned long) strlen(result_str);

  return result_str;
}

static inline bool init_acquire(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  initid->maybe_null= FALSE;
  initid->decimals= 0;
  initid->max_length= 1;
  initid->ptr= NULL;
  initid->const_item= 0;
  initid->extension= NULL;

  THD *thd= current_thd;
  if (!has_required_privileges(thd))
  {
    my_stpcpy(message, "The user is not privileged to use this function.");
    return true;
  }

  // At least two arguments - lock, timeout
  if (args->arg_count < 2)
  {
    strcpy(message,
           "Requires at least two arguments: (lock(...),timeout).");
    return TRUE;
  }

  // Timeout is the last argument, should be INT
  if (args->arg_type[args->arg_count - 1] != INT_RESULT)
  {
    strcpy(message, "Wrong argument type - expected integer.");
    return TRUE;
  }

  // All other arguments should be strings
  for (size_t i= 0; i < (args->arg_count - 1); i++)
  {
    if (args->arg_type[i] != STRING_RESULT)
    {
      strcpy(message, "Wrong argument type - expected string.");
      return TRUE;
    }
  }

  return FALSE;
}

PLUGIN_EXPORT bool version_tokens_lock_shared_init(
  UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  return init_acquire(initid, args, message);
}


PLUGIN_EXPORT long long version_tokens_lock_shared(UDF_INIT*, UDF_ARGS *args,
                                                   char*, char *error)
{
  long long timeout=
    args->args[args->arg_count - 1] ?                      // Null ?
    *((long long *) args->args[args->arg_count - 1]) : -1;

  if (timeout < 0)
  {
    my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "timeout",
             "version_tokens_lock_shared");
    *error= 1;
  }

  // For the UDF 1 == success, 0 == failure.
  return !acquire_locking_service_locks(NULL, VTOKEN_LOCKS_NAMESPACE,
                                        const_cast<const char**>(&args->args[0]),
					args->arg_count - 1,
					LOCKING_SERVICE_READ, (unsigned long) timeout);
}


PLUGIN_EXPORT bool version_tokens_lock_exclusive_init(
  UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  return init_acquire(initid, args, message);
}


PLUGIN_EXPORT long long version_tokens_lock_exclusive(UDF_INIT*, UDF_ARGS *args,
                                                      char*, char *error)
{
  long long timeout=
    args->args[args->arg_count - 1] ?                      // Null ?
    *((long long *) args->args[args->arg_count - 1]) : -1;

  if (timeout < 0)
  {
    my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "timeout",
             "version_tokens_lock_exclusive");
    *error= 1;
  }

  // For the UDF 1 == success, 0 == failure.
  return !acquire_locking_service_locks(NULL, VTOKEN_LOCKS_NAMESPACE,
                                        const_cast<const char**>(&args->args[0]),
					args->arg_count - 1,
					LOCKING_SERVICE_WRITE, (unsigned long) timeout);
}

PLUGIN_EXPORT bool version_tokens_unlock_init(UDF_INIT*, UDF_ARGS *args,
                                              char *message)
{
  THD *thd= current_thd;

  if (!has_required_privileges(thd))
  {
    my_stpcpy(message, "The user is not privileged to use this function.");
    return true;
  }

  if (args->arg_count != 0)
  {
    strcpy(message, "Requires no arguments.");
    return TRUE;
  }

  return FALSE;
}

long long version_tokens_unlock(UDF_INIT*, UDF_ARGS*,
                                char*, char*)
{
  // For the UDF 1 == success, 0 == failure.
  return !release_locking_service_locks(NULL, VTOKEN_LOCKS_NAMESPACE);
}
