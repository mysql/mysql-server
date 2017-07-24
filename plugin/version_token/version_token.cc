/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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
#include <mysql/plugin_audit.h>
#include <m_string.h>
#include <sql_class.h>
#include <hash.h>
#include <sstream>
#include <errmsg.h>
#include <mysql/service_locking.h>
#include <locking_service.h>

#ifdef WIN32
#define PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define PLUGIN_EXPORT extern "C"
#endif

// This global value is initiated with 1 and the corresponding session
// value with 0. Hence Every session must compare its tokens with the
// global values when it runs its very first query.
static volatile int64 session_number= 1;
static size_t vtoken_string_length;

// This flag is for memory management when the variable
// is updated for the first time.
struct version_token_st {
  LEX_STRING token_name;
  LEX_STRING token_val;
};


#define VTOKEN_LOCKS_NAMESPACE "version_token_locks"

#define LONG_TIMEOUT ((ulong) 3600L*24L*365L)

static HASH version_tokens_hash;

/**
  Utility class implementing an atomic boolean on top of an int32

  The mysys lib does not support atomic booleans.
*/
class atomic_boolean
{
  /** constants for true and false */
  static const int m_true, m_false;
  /** storage for the boolean's current value */
  volatile int32 m_value;
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

    actual_value= my_atomic_load32(&m_value);

    return actual_value == cmp;
  }

  /**
    Sets a new value for the atomic boolean

    @param new value
  */
  void set(bool new_value)
  {
    int32 new_val= new_value ? m_true : m_false;
    my_atomic_store32(&m_value, new_val);
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
                          PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                          "Version number to assist with session tokens check",
                          NULL, NULL, 0L, 0, ((ulong) -1), 0);


static void update_session_version_tokens(MYSQL_THD thd,
                                          struct st_mysql_sys_var *var,
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

PSI_memory_key key_memory_vtoken;

#ifdef HAVE_PSI_INTERFACE
PSI_rwlock_key key_LOCK_vtoken_hash;

static PSI_rwlock_info all_vtoken_rwlocks[]=
{
  {&key_LOCK_vtoken_hash, "LOCK_vtoken_hash", 0},
};

static PSI_memory_info all_vtoken_memory[]=
{
  {&key_memory_vtoken, "vtoken", 0}
};

// Function to register the lock
void vtoken_init_psi_keys(void)
{
  const char* category= "vtoken";
  int count;

  if (PSI_server == NULL)
    return;

  count= array_elements(all_vtoken_rwlocks);
  PSI_server->register_rwlock(category, all_vtoken_rwlocks, count);

  count= array_elements(all_vtoken_memory);
  PSI_server->register_memory(category, all_vtoken_memory, count);
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


static uchar *version_token_get_key(const char *entry MY_ATTRIBUTE((unused)),
                                    size_t *length MY_ATTRIBUTE((unused)),
	    	                    my_bool not_used MY_ATTRIBUTE((unused)));

static void set_vtoken_string_length()
{
  version_token_st *token_obj;
  int i= 0;
  size_t str_size= 0;
  while ((token_obj= (version_token_st *) my_hash_element(&version_tokens_hash, i)))
  {
    if ((token_obj->token_name).str)
      str_size= str_size+ (token_obj->token_name).length;

    if ((token_obj->token_val).str)
      str_size= str_size+ (token_obj->token_val).length;

    str_size+= 2;

    i++;
  }
  vtoken_string_length= str_size;
}


// UDF
PLUGIN_EXPORT my_bool version_tokens_set_init(UDF_INIT *initid, UDF_ARGS *args,
                                              char *message);
PLUGIN_EXPORT char *version_tokens_set(UDF_INIT *initid, UDF_ARGS *args,
                                       char *result, unsigned long *length,
				       char *null_value, char *error);

PLUGIN_EXPORT my_bool version_tokens_show_init(UDF_INIT *initid, UDF_ARGS *args,
                                               char *message);
PLUGIN_EXPORT void version_tokens_show_deinit(UDF_INIT *initid);
PLUGIN_EXPORT char *version_tokens_show(UDF_INIT *initid, UDF_ARGS *args,
                                        char *result, unsigned long *length,
					char *null_value, char *error);

PLUGIN_EXPORT my_bool version_tokens_edit_init(UDF_INIT *initid, UDF_ARGS *args,
                                               char *message);
PLUGIN_EXPORT char *version_tokens_edit(UDF_INIT *initid, UDF_ARGS *args,
                                        char *result, unsigned long *length,
					char *null_value, char *error);

PLUGIN_EXPORT my_bool version_tokens_delete_init(UDF_INIT *initid,
                                                 UDF_ARGS *args, char *message);
PLUGIN_EXPORT char *version_tokens_delete(UDF_INIT *initid, UDF_ARGS *args,
                                          char *result, unsigned long *length,
					  char *null_value, char *error);
PLUGIN_EXPORT my_bool version_tokens_lock_shared_init(
  UDF_INIT *initid, UDF_ARGS *args, char *message);
PLUGIN_EXPORT long long version_tokens_lock_shared(
  UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
PLUGIN_EXPORT my_bool version_tokens_lock_exclusive_init(
  UDF_INIT *initid, UDF_ARGS *args, char *message);
PLUGIN_EXPORT long long version_tokens_lock_exclusive(
  UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
PLUGIN_EXPORT my_bool version_tokens_unlock_init(
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

  @param input          [IN]   List of semicolon separated token name/value pairs
  @param enum command   [IN]   Helps determining the caller function.

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
  ulonglong tmp_token_number= (ulonglong)
                              my_atomic_load64((volatile int64 *) &session_number);

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
	char *name= NULL, *val= NULL;
	size_t name_len, val_len;

	name_len= token_name.length;
	val_len= token_val.length;
	version_token_st *v_token= NULL;

	if (!my_multi_malloc(key_memory_vtoken, MYF(0),
	  		     &v_token, sizeof(version_token_st), &name, name_len,
			     &val, val_len, NULL))
	{
	  push_warning(thd, Sql_condition::SL_WARNING, CR_OUT_OF_MEMORY,
		       "Not enough memory available");
	  return result;
	}

	memcpy(name, token_name.str, name_len);
	memcpy(val, token_val.str, val_len);
	v_token->token_name.str= name;
	v_token->token_val.str= val;
	v_token->token_name.length= name_len;
	v_token->token_val.length= val_len;

	if (my_hash_insert(&version_tokens_hash, (uchar *) v_token))
	{
	  version_token_st *tmp= (version_token_st *)
				 my_hash_search(&version_tokens_hash,
						(uchar *) name, name_len);

	  if (tmp)
	  {
	    my_hash_delete(&version_tokens_hash, (uchar *) tmp);
	  }
	  my_hash_insert(&version_tokens_hash, (uchar *) v_token);
	}
	result++;
      }
	break;

      case CHECK_VTOKEN:
      {
	version_token_st *token_obj;
        char error_str[MYSQL_ERRMSG_SIZE];
        if (!mysql_acquire_locking_service_locks(thd, VTOKEN_LOCKS_NAMESPACE,
	                                         (const char **) &(token_name.str), 1,
					         LOCKING_SERVICE_READ, LONG_TIMEOUT) &&
            !vtokens_unchanged)
	{
	  if ((token_obj= (version_token_st *)my_hash_search(&version_tokens_hash,
							     (const uchar*) token_name.str,
							     token_name.length)))
	  {
	    if ((token_obj->token_val.length != token_val.length) ||
		(memcmp(token_obj->token_val.str, token_val.str, token_val.length) != 0))
	    {

              if (!thd->get_stmt_da()->is_set())
              {
                my_snprintf(error_str, sizeof(error_str),
                            ER(ER_VTOKEN_PLUGIN_TOKEN_MISMATCH),
                            (int) token_name.length, token_name.str,
                            (int) token_obj->token_val.length, 
                            token_obj->token_val.str);

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
                          ER(ER_VTOKEN_PLUGIN_TOKEN_NOT_FOUND),
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
static int version_token_check(MYSQL_THD thd,
                               mysql_event_class_t event_class,
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

  // Initialize hash.
  my_hash_init(&version_tokens_hash,
	       &my_charset_bin,
               4, 0, 0, (my_hash_get_key) version_token_get_key,
               my_free, HASH_UNIQUE,
               key_memory_vtoken);
  version_tokens_hash_inited.set(true);

  if (!cleanup_lock.is_active())
  {
    // Lock for hash.
    mysql_rwlock_init(key_LOCK_vtoken_hash, &LOCK_vtoken_hash);
    // Lock for version number.
    cleanup_lock.activate();
  }
  return 0;
}

/** Plugin deinit. */
static int version_tokens_deinit(void *arg MY_ATTRIBUTE((unused)))
{
  mysql_rwlock_wrlock(&LOCK_vtoken_hash);
  if (version_tokens_hash.records)
    my_hash_reset(&version_tokens_hash);

  my_hash_free(&version_tokens_hash);
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

PLUGIN_EXPORT my_bool version_tokens_set_init(UDF_INIT* initid, UDF_ARGS* args,
                                              char* message)
{
  THD *thd= current_thd;

  if (!(thd->security_context()->check_access(SUPER_ACL)))
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

PLUGIN_EXPORT char *version_tokens_set(UDF_INIT *initid, UDF_ARGS *args,
                                       char *result, unsigned long *length,
				       char *null_value, char *error)
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
    if (version_tokens_hash.records)
      my_hash_reset(&version_tokens_hash);
    vtokens_count= parse_vtokens(hash_str, SET_VTOKEN);
    ss << vtokens_count << " version tokens set.";

    my_free(hash_str);

  }
  else
  {
    if (version_tokens_hash.records)
      my_hash_reset(&version_tokens_hash);
    ss << "Version tokens list cleared.";
  }
  set_vtoken_string_length();

  my_atomic_add64((volatile int64 *) &session_number, 1);

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

PLUGIN_EXPORT my_bool version_tokens_edit_init(UDF_INIT *initid, UDF_ARGS *args,
                                               char *message)
{
  THD *thd= current_thd;

  if (!version_tokens_hash_inited.is_set())
  {
    my_stpcpy(message, "version_token plugin is not installed.");
    return true;
  }

  if (!(thd->security_context()->check_access(SUPER_ACL)))
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

PLUGIN_EXPORT char *version_tokens_edit(UDF_INIT *initid, UDF_ARGS *args,
                                        char *result, unsigned long *length,
					char *null_value, char *error)
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
      my_atomic_add64((volatile int64 *) &session_number, 1);

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

PLUGIN_EXPORT my_bool version_tokens_delete_init(UDF_INIT *initid,
                                                 UDF_ARGS *args, char *message)
{
  THD *thd= current_thd;

  if (!version_tokens_hash_inited.is_set())
  {
    my_stpcpy(message, "version_token plugin is not installed.");
    return true;
  }

  if (!(thd->security_context()->check_access(SUPER_ACL)))
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

PLUGIN_EXPORT char *version_tokens_delete(UDF_INIT *initid, UDF_ARGS *args,
                                          char *result, unsigned long *length,
					  char *null_value, char *error)
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
      version_token_st *tmp;
      LEX_STRING st={ token, strlen(token) };

      trim_whitespace(&my_charset_bin, &st);

      if (st.length)
      {
        tmp= (version_token_st *) my_hash_search(&version_tokens_hash,
                                                 (uchar *) st.str,
                                                 st.length);
        if (tmp)
        {
          my_hash_delete(&version_tokens_hash, (uchar *) tmp);
          vtokens_count++;
        }
      }

      token= my_strtok_r(NULL, separator, &lasts_token);
    }

    set_vtoken_string_length();

    if (vtokens_count)
    {
      my_atomic_add64((volatile int64 *) &session_number, 1);
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

PLUGIN_EXPORT my_bool version_tokens_show_init(UDF_INIT *initid, UDF_ARGS *args,
                                               char *message)
{
  int i= 0;
  size_t str_size= 0;
  char *result_str;
  version_token_st *token_obj;
  THD *thd= current_thd;

  if (!(thd->security_context()->check_access(SUPER_ACL)))
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

    i= 0;

    while ((token_obj= (version_token_st *) my_hash_element(&version_tokens_hash, i)))
    {
      memcpy(result_str, (token_obj->token_name).str, (token_obj->token_name).length);

      result_str+= (token_obj->token_name).length;

      memcpy(result_str, "=", 1);

      result_str++;

      memcpy(result_str, (token_obj->token_val).str, (token_obj->token_val).length);

      result_str+= (token_obj->token_val).length;

      memcpy(result_str, ";", 1);

      result_str++;

      i++;
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

PLUGIN_EXPORT char *version_tokens_show(UDF_INIT *initid, UDF_ARGS *args,
                                        char *result, unsigned long *length,
					char *null_value, char *error)
{
  char *result_str= initid->ptr;
  *length= 0;

  if (!result_str)
    return NULL;

  *length= (unsigned long) strlen(result_str);

  return result_str;
}

static inline my_bool init_acquire(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  initid->maybe_null= FALSE;
  initid->decimals= 0;
  initid->max_length= 1;
  initid->ptr= NULL;
  initid->const_item= 0;
  initid->extension= NULL;

  THD *thd= current_thd;

  if (!(thd->security_context()->check_access(SUPER_ACL)))
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

PLUGIN_EXPORT my_bool version_tokens_lock_shared_init(
  UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  return init_acquire(initid, args, message);
}


PLUGIN_EXPORT long long version_tokens_lock_shared(
  UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error)
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


PLUGIN_EXPORT my_bool version_tokens_lock_exclusive_init(
  UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  return init_acquire(initid, args, message);
}


PLUGIN_EXPORT long long version_tokens_lock_exclusive(
  UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error)
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

PLUGIN_EXPORT my_bool version_tokens_unlock_init(
  UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  THD *thd= current_thd;

  if (!(thd->security_context()->check_access(SUPER_ACL)))
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


long long version_tokens_unlock(UDF_INIT *initid, UDF_ARGS *args,
                                             char *is_null, char *error)
{
  // For the UDF 1 == success, 0 == failure.
  return !release_locking_service_locks(NULL, VTOKEN_LOCKS_NAMESPACE);
}



static uchar *version_token_get_key(const char *entry, size_t *length,
		                    my_bool not_used MY_ATTRIBUTE((unused)))
{
  char *key;
  key= (((version_token_st *) entry)->token_name).str;
  *length= (((version_token_st *) entry)->token_name).length;
  return (uchar *) key;
}
