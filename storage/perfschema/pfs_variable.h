/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef PFS_VARIABLE_H
#define PFS_VARIABLE_H

/**
  @file storage/perfschema/pfs_variable.h
  Performance schema system and status variables (declarations).
*/

/**
  OVERVIEW
  --------
  Status and system variables are implemented differently in the server, but the
  steps to process them in the Performance Schema are essentially the same:

  1. INITIALIZE - Build or acquire a sorted list of variables to use for input.
     Use the SHOW_VAR struct as an intermediate format common to system, status
     and user vars:

     SHOW_VAR
       Name  - Text string
       Value - Pointer to memory location, function, subarray structure
       Type  - Scalar, function, or subarray
       Scope - SESSION, GLOBAL, BOTH

     Steps:
     - Register the server's internal buffer with the class. Acquire locks
       if necessary, then scan the contents of the input buffer.
     - For system variables, convert each element to SHOW_VAR format, store in
       a temporary array.
     - For status variables, copy existing global status array into a local
       array that can be used without locks. Expand nested subarrays, indicated
       by a type of SHOW_ARRAY.

  2. MATERIALIZE - Convert the list of SHOW_VAR variables to string format,
     store in a local cache:
     - Resolve each variable according to the type.
     - Recursively process unexpanded nested arrays and callback functions.
     - Aggregate values across threads for global status.
     - Convert numeric values to a string.
     - Prefix variable name with the plugin name.

  3. OUTPUT - Iterate the cache for the SHOW command or table query.

  CLASS OVERVIEW
  --------------
  1. System_variable - A materialized system variable
  2. Status_variable - A materialized status variable
  3. PFS_variable_cache - Base class that defines the interface for the operations above.
     public
       init_show_var_array() - Build SHOW_VAR list of variables for processing
       materialize_global()  - Materialize global variables, aggregate across sessions
       materialize_session() - Materialize variables for a given PFS_thread or THD
       materialize_user()    - Materialize variables for a user, aggregate across related threads.
       materialize_host()    - Materialize variables for a host, aggregate across related threads.
       materialize_account() - Materialize variables for a account, aggregate across related threads.
     private
       m_show_var_array         - Prealloc_array of SHOW_VARs for input to Materialize
       m_cache                  - Prealloc_array of materialized variables for output
       do_materialize_global()  - Implementation of materialize_global()
       do_materialize_session() - Implementation of materialize_session()
       do_materialize_client()  - Implementation of materialize_user/host/account()

  4. PFS_system_variable_cache - System variable implementation of PFS_variable_cache
  5. PFS_status_variable_cache - Status variable implementation of PFS_variable_cache
  6. Find_THD_variable         - Used by the thread manager to find and lock a THD.

  GLOSSARY
  --------
  Status variable - Server or plugin status counter. Not dynamic.
  System variable - Server or plugin configuration variable. Usually dynamic.
  GLOBAL scope    - Associated with the server, no context at thread level.
  SESSION scope   - Associated with a connection or thread, but no global context.
  BOTH scope      - Globally defined but applies at the session level.
  Initialize      - Build list of variables in SHOW_VAR format.
  Materialize     - Convert variables in SHOW_VAR list to string, cache for output.
  Manifest        - Substep of Materialize. Resolve variable values according to
                    type. This includes SHOW_FUNC types which are resolved by
                    executing a callback function (possibly recursively), and
                    SHOW_ARRAY types that expand into nested subarrays.
  LOCK PRIORITIES
  ---------------
  System Variables
    LOCK_plugin_delete              (block plugin delete)
     LOCK_system_variables_hash
     LOCK_thd_data                  (block THD delete)
     LOCK_thd_sysvar                (block system variable updates, alloc_and_copy_thd_dynamic_variables)
       LOCK_global_system_variables (very briefly held)

  Status Variables
    LOCK_status
      LOCK_thd_data                 (block THD delete)
*/

/* Iteration on THD from the sql layer. */
#include "mysqld_thd_manager.h"
#define PFS_VAR
/* Class sys_var */
#include "set_var.h"
/* convert_value_to_string */
#include "sql_show.h"
/* PFS_thread */
#include "pfs_instr.h"
#include "pfs_user.h"
#include "pfs_host.h"
#include "pfs_account.h"

/* Global array of all server and plugin-defined status variables. */
extern Status_var_array all_status_vars;
extern bool status_vars_inited;
static const uint SYSVAR_MEMROOT_BLOCK_SIZE = 4096;

extern mysql_mutex_t LOCK_plugin_delete;

class Find_THD_Impl;
class Find_THD_variable;
typedef PFS_connection_slice PFS_client;

/**
  CLASS System_variable - System variable derived from sys_var object.
*/
class System_variable
{
public:
  System_variable();
  System_variable(THD *target_thd, const SHOW_VAR *show_var, enum_var_type query_scope);
  ~System_variable() {}

  bool is_null() const {return !m_initialized;};

public:
  const char *m_name;
  size_t m_name_length;
  char m_value_str[SHOW_VAR_FUNC_BUFF_SIZE+1];
  size_t m_value_length;
  enum_mysql_show_type m_type;
  int m_scope;
  const CHARSET_INFO *m_charset;

private:
  bool m_initialized;
  void init(THD *thd, const SHOW_VAR *show_var, enum_var_type query_scope);
};


/**
  CLASS Status_variable - Status variable derived from SHOW_VAR.
*/
class Status_variable
{
public:
  Status_variable() : m_name(NULL), m_name_length(0), m_value_length(0),
                      m_type(SHOW_UNDEF), m_scope(SHOW_SCOPE_UNDEF),
                      m_charset(NULL), m_initialized(false) {}

  Status_variable(const SHOW_VAR *show_var, STATUS_VAR *status_array, enum_var_type query_scope);

  ~Status_variable() {}

  bool is_null() const {return !m_initialized;};

public:
  const char *m_name;
  size_t m_name_length;
  char m_value_str[SHOW_VAR_FUNC_BUFF_SIZE+1];
  size_t m_value_length;
  SHOW_TYPE  m_type;
  SHOW_SCOPE m_scope;
  const CHARSET_INFO *m_charset;
private:
  bool m_initialized;
  void init(const SHOW_VAR *show_var, STATUS_VAR *status_array, enum_var_type query_scope);
};


/**
  CLASS Find_THD_variable - Get and lock a validated THD from the thread manager.
*/
class Find_THD_variable : public Find_THD_Impl
{
public:
  Find_THD_variable() : m_unsafe_thd(NULL) {}
  Find_THD_variable(THD *unsafe_thd) : m_unsafe_thd(unsafe_thd) {}

  virtual bool operator()(THD *thd)
  {
    //TODO: filter bg threads?
    if (thd != m_unsafe_thd)
      return false;

    /* Hold this lock to keep THD during materialization. */
    mysql_mutex_lock(&thd->LOCK_thd_data);
    return true;
  }
  void set_unsafe_thd(THD *unsafe_thd) { m_unsafe_thd= unsafe_thd; }
private:
  THD *m_unsafe_thd;
};

/**
  CLASS PFS_variable_cache - Base class for a system or status variable cache.
*/
template <class Var_type>
class PFS_variable_cache
{
public:
  typedef Prealloced_array<Var_type, SHOW_VAR_PREALLOC, false> Variable_array;

  PFS_variable_cache(bool external_init)
    : m_safe_thd(NULL),
      m_unsafe_thd(NULL),
      m_current_thd(current_thd),
      m_pfs_thread(NULL),
      m_pfs_client(NULL),
      m_thd_finder(),
      m_cache(PSI_INSTRUMENT_ME),
      m_initialized(false),
      m_external_init(external_init),
      m_materialized(false),
      m_show_var_array(PSI_INSTRUMENT_ME),
      m_version(0),
      m_query_scope(OPT_DEFAULT),
      m_use_mem_root(false),
      m_aggregate(false)
  { }

  virtual ~PFS_variable_cache()= 0;

  /**
    Build array of SHOW_VARs from the external variable source.
    Filter using session scope.
  */
  bool initialize_session(void);

  /**
    Build array of SHOW_VARs suitable for aggregation by user, host or account.
    Filter using session scope.
  */
  bool initialize_client_session(void);

  /**
    Build cache of GLOBAL system or status variables.
    Aggregate across threads if applicable.
  */
  int materialize_global();

  /**
    Build cache of GLOBAL and SESSION variables for a non-instrumented thread.
  */
  int materialize_all(THD *thd);

  /**
    Build cache of SESSION variables for a non-instrumented thread.
  */
  int materialize_session(THD *thd);

  /**
    Build cache of SESSION variables for an instrumented thread.
  */
  int materialize_session(PFS_thread *pfs_thread, bool use_mem_root= false);

  /**
    Cache a single SESSION variable for an instrumented thread.
  */
  int materialize_session(PFS_thread *pfs_thread, uint index);

  /**
    Build cache of SESSION status variables for a user.
  */
  int materialize_user(PFS_user *pfs_user);

  /**
    Build cache of SESSION status variables for a host.
  */
  int materialize_host(PFS_host *pfs_host);

  /**
    Build cache of SESSION status variables for an account.
  */
  int materialize_account(PFS_account *pfs_account);

  /**
    True if variables have been materialized.
  */
  bool is_materialized(void)
  {
    return m_materialized;
  }

  /**
    True if variables have been materialized for given THD.
  */
  bool is_materialized(THD *unsafe_thd)
  {
    return (unsafe_thd == m_unsafe_thd && m_materialized);
  }

  /**
    True if variables have been materialized for given PFS_thread.
  */
  bool is_materialized(PFS_thread *pfs_thread)
  {
    return (pfs_thread == m_pfs_thread && m_materialized);
  }

  /**
    True if variables have been materialized for given PFS_user.
  */
  bool is_materialized(PFS_user *pfs_user)
  {
    return (static_cast<PFS_client *>(pfs_user) == m_pfs_client && m_materialized);
  }

  /**
    True if variables have been materialized for given PFS_host.
  */
  bool is_materialized(PFS_host *pfs_host)
  {
    return (static_cast<PFS_client *>(pfs_host) == m_pfs_client && m_materialized);
  }

  /**
    True if variables have been materialized for given PFS_account.
  */
  bool is_materialized(PFS_account *pfs_account)
  {
    return (static_cast<PFS_client *>(pfs_account) == m_pfs_client && m_materialized);
  }

  /**
    True if variables have been materialized for given PFS_user/host/account.
  */
  bool is_materialized(PFS_client *pfs_client)
  {
    return (static_cast<PFS_client *>(pfs_client) == m_pfs_client && m_materialized);
  }

  /**
    Get a validated THD from the thread manager. Execute callback function while
    inside of the thread manager locks.
  */
  THD *get_THD(THD *thd);
  THD *get_THD(PFS_thread *pfs_thread);

  /**
    Get a single variable from the cache.
    Get the first element in the cache by default.
  */
  const Var_type *get(uint index= 0) const
  {
    if (index >= m_cache.size())
      return NULL;

    const Var_type *p= &m_cache.at(index);
    return p;
  }

  /**
    Number of elements in the cache.
  */
  uint size()
  {
    return (uint)m_cache.size();
  }

private:
  virtual bool do_initialize_global(void) { return true; }
  virtual bool do_initialize_session(void) { return true; }
  virtual int do_materialize_global(void) { return 1; }
  virtual int do_materialize_all(THD *thd) { return 1; }
  virtual int do_materialize_session(THD *thd) { return 1; }
  virtual int do_materialize_session(PFS_thread *) { return 1; }
  virtual int do_materialize_session(PFS_thread *, uint index) { return 1; }

protected:
  /* Validated THD */
  THD *m_safe_thd;

  /* Unvalidated THD */
  THD *m_unsafe_thd;

  /* Current THD */
  THD *m_current_thd;

  /* Current PFS_thread. */
  PFS_thread *m_pfs_thread;

  /* Current PFS_user, host or account. */
  PFS_client *m_pfs_client;

  /* Callback for thread iterator. */
  Find_THD_variable m_thd_finder;

  /* Cache of materialized variables. */
  Variable_array m_cache;

  /* True when list of SHOW_VAR is complete. */
  bool m_initialized;

  /*
    True if the SHOW_VAR array must be initialized externally from the
    materialization step, such as with aggregations and queries by thread.
  */
  bool m_external_init;

  /* True when cache is complete. */
  bool m_materialized;

  /* Array of variables to be materialized. Last element must be null. */
  Show_var_array m_show_var_array;

  /* Version of global hash/array. Changes when vars added/removed. */
  ulonglong m_version;

  /* Query scope: GLOBAL or SESSION. */
  enum_var_type m_query_scope;

  /* True if temporary mem_root should be used for materialization. */
  bool m_use_mem_root;

  /* True if summarizing across users, hosts or accounts. */
  bool m_aggregate;

};

/**
  Required implementation for pure virtual destructor of a template class.
*/
template <class Var_type>
PFS_variable_cache<Var_type>::~PFS_variable_cache()
{
}

/**
  Get a validated THD from the thread manager. Execute callback function while
  while inside the thread manager lock.
*/
template <class Var_type>
THD *PFS_variable_cache<Var_type>::get_THD(THD *unsafe_thd)
{
  if (unsafe_thd == NULL)
  {
    /*
      May happen, precisely because the pointer is unsafe
      (THD just disconnected for example).
      No need to walk Global_THD_manager for that.
    */
    return NULL;
  }

  m_thd_finder.set_unsafe_thd(unsafe_thd);
  THD* safe_thd= Global_THD_manager::get_instance()->find_thd(&m_thd_finder);
  return safe_thd;
}

template <class Var_type>
THD *PFS_variable_cache<Var_type>::get_THD(PFS_thread *pfs_thread)
{
  DBUG_ASSERT(pfs_thread != NULL);
  return get_THD(pfs_thread->m_thd);
}

/**
  Build array of SHOW_VARs from external source of system or status variables.
  Filter using session scope.
*/
template <class Var_type>
bool PFS_variable_cache<Var_type>::initialize_session(void)
{
  if (m_initialized)
    return 0;

  return do_initialize_session();
}

/**
  Build array of SHOW_VARs suitable for aggregation by user, host or account.
  Filter using session scope.
*/
template <class Var_type>
bool PFS_variable_cache<Var_type>::initialize_client_session(void)
{
  if (m_initialized)
    return 0;

  /* Requires aggregation by user, host or account. */
  m_aggregate= true;

  return do_initialize_session();
}

/**
  Build cache of all GLOBAL variables.
*/
template <class Var_type>
int PFS_variable_cache<Var_type>::materialize_global()
{
  if (is_materialized())
    return 0;

  return do_materialize_global();
}

/**
  Build cache of GLOBAL and SESSION variables for a non-instrumented thread.
*/
template <class Var_type>
int PFS_variable_cache<Var_type>::materialize_all(THD *unsafe_thd)
{
  if (!unsafe_thd)
    return 1;

  if (is_materialized(unsafe_thd))
    return 0;

  return do_materialize_all(unsafe_thd);
}

/**
  Build cache of SESSION variables for a non-instrumented thread.
*/
template <class Var_type>
int PFS_variable_cache<Var_type>::materialize_session(THD *unsafe_thd)
{
  if (!unsafe_thd)
    return 1;

  if (is_materialized(unsafe_thd))
    return 0;

  return do_materialize_session(unsafe_thd);
}

/**
  Build cache of SESSION variables for a thread.
*/
template <class Var_type>
int PFS_variable_cache<Var_type>::materialize_session(PFS_thread *pfs_thread, bool use_mem_root)
{
  if (!pfs_thread)
    return 1;

  if (is_materialized(pfs_thread))
    return 0;

  if (!pfs_thread->m_lock.is_populated() || pfs_thread->m_thd == NULL)
    return 1;

  m_use_mem_root= use_mem_root;

  return do_materialize_session(pfs_thread);
}

/**
  Materialize a single variable for a thread.
*/
template <class Var_type>
int PFS_variable_cache<Var_type>::materialize_session(PFS_thread *pfs_thread, uint index)
{
  /* No check for is_materialized(). */

  if (!pfs_thread)
    return 1;

  if (!pfs_thread->m_lock.is_populated() || pfs_thread->m_thd == NULL)
    return 1;

  return do_materialize_session(pfs_thread, index);
}

/**
  CLASS PFS_system_variable_cache - System variable cache.
*/
class PFS_system_variable_cache : public PFS_variable_cache<System_variable>
{
public:
  PFS_system_variable_cache(bool external_init) :
                            PFS_variable_cache<System_variable>(external_init),
                            m_mem_thd(NULL), m_mem_thd_save(NULL),
                            m_mem_sysvar_ptr(NULL) { }
  bool match_scope(int scope);
  ulonglong get_sysvar_hash_version(void) { return m_version; }
  ~PFS_system_variable_cache() { free_mem_root(); }

private:
  /* Build SHOW_var array. */
  bool init_show_var_array(enum_var_type scope, bool strict);
  bool do_initialize_session(void);

  /* Global */
  int do_materialize_global(void);
  /* Global and Session - THD */
  int do_materialize_all(THD* thd);
  /* Session - THD */
  int do_materialize_session(THD* thd);
  /* Session -  PFS_thread */
  int do_materialize_session(PFS_thread *thread);
  /* Single variable -  PFS_thread */
  int do_materialize_session(PFS_thread *pfs_thread, uint index);

  /* Temporary mem_root to use for materialization. */
  MEM_ROOT m_mem_sysvar;
  /* Pointer to THD::mem_root. */
  MEM_ROOT **m_mem_thd;
  /* Save THD::mem_root. */
  MEM_ROOT *m_mem_thd_save;
  /* Pointer to temporary mem_root. */
  MEM_ROOT *m_mem_sysvar_ptr;
  /* Allocate and/or assign temporary mem_root. */
  void set_mem_root(void);
  /* Mark all memory blocks as free in temporary mem_root. */
  void clear_mem_root(void);
  /* Free mem_root memory. */
  void free_mem_root(void);
};


/**
  CLASS PFS_status_variable_cache - Status variable cache
*/
class PFS_status_variable_cache : public PFS_variable_cache<Status_variable>
{
public:
  PFS_status_variable_cache(bool external_init);

  int materialize_user(PFS_user *pfs_user);
  int materialize_host(PFS_host *pfs_host);
  int materialize_account(PFS_account *pfs_account);

  ulonglong get_status_array_version(void) { return m_version; }

protected:
  /* Get PFS_user, account or host associated with a PFS_thread. Implemented by table class. */
  virtual PFS_client *get_pfs(PFS_thread *pfs_thread) { return NULL; }

  /* True if query is a SHOW command. */
  bool m_show_command;

private:
  bool do_initialize_session(void);

  int do_materialize_global(void);
  /* Global and Session - THD */
  int do_materialize_all(THD* thd);
  int do_materialize_session(THD *thd);
  int do_materialize_session(PFS_thread *thread);
  int do_materialize_session(PFS_thread *thread, uint index) { return 0; }
  int do_materialize_client(PFS_client *pfs_client);

  /* Callback to sum user, host or account status variables. */
  void (*m_sum_client_status)(PFS_client *pfs_client, STATUS_VAR *status_totals);

  /* Build SHOW_VAR array from external source. */
  bool init_show_var_array(enum_var_type scope, bool strict);

  /* Recursively expand nested SHOW_VAR arrays. */
  void expand_show_var_array(const SHOW_VAR *show_var_array, const char *prefix, bool strict);

  /* Exclude unwanted variables from the query. */
  bool filter_show_var(const SHOW_VAR *show_var, bool strict);

  /* Check the variable scope against the query scope. */
  bool match_scope(SHOW_SCOPE variable_scope, bool strict);

  /* Exclude specific status variables by name or prefix. */
  bool filter_by_name(const SHOW_VAR *show_var);

  /* Check if a variable has an aggregatable type. */
  bool can_aggregate(enum_mysql_show_type variable_type);

  /* Build status variable name with prefix. Return in the buffer provided. */
  char *make_show_var_name(const char* prefix, const char* name, char *name_buf, size_t buf_len);

  /* Build status variable name with prefix. Return copy of the string. */
  char *make_show_var_name(const char* prefix, const char* name);

  /* For the current THD, use initial_status_vars taken from before the query start. */
  STATUS_VAR *set_status_vars(void);

  /* Build the list of status variables from SHOW_VAR array. */
  void manifest(THD *thd, const SHOW_VAR *show_var_array,
                STATUS_VAR *status_var_array, const char *prefix, bool nested_array, bool strict);
};

/* Callback functions to sum status variables for a given user, host or account. */
void sum_user_status(PFS_client *pfs_user, STATUS_VAR *status_totals);
void sum_host_status(PFS_client *pfs_host, STATUS_VAR *status_totals);
void sum_account_status(PFS_client *pfs_account, STATUS_VAR *status_totals);


/** @} */
#endif

