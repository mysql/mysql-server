/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
  */

#include "storage/perfschema/pfs_variable.h"

#include <map>
#include <vector>

#include "my_compiler.h"
/**
  @file storage/perfschema/pfs_variable.cc
  Performance schema system variable and status variable (implementation).
*/
#include "my_dbug.h"
#include "my_macros.h"
#include "my_sys.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"
#include "sql/derror.h"
#include "sql/mysqld.h"
#include "sql/persisted_variable.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "sql/sql_show.h"
#include "storage/perfschema/pfs.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_visitor.h"

using std::map;
using std::vector;

bool Find_THD_variable::operator()(THD *thd) {
  // TODO: filter bg threads?
  if (thd != m_unsafe_thd) {
    return false;
  }

  /* Hold this lock to keep THD during materialization. */
  mysql_mutex_lock(&thd->LOCK_thd_data);
  return true;
}

template <class Var_type>
PFS_variable_cache<Var_type>::PFS_variable_cache(bool external_init)
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
      m_aggregate(false) {}

PFS_system_variable_cache::PFS_system_variable_cache(bool external_init)
    : PFS_variable_cache<System_variable>(external_init),
      m_mem_thd(NULL),
      m_mem_thd_save(NULL),
      m_mem_sysvar_ptr(NULL) {}

/**
  Build a sorted list of all system variables from the system variable hash.
  Filter by scope. Must be called inside of LOCK_plugin_delete.
*/
bool PFS_system_variable_cache::init_show_var_array(enum_var_type scope,
                                                    bool strict) {
  DBUG_ASSERT(!m_initialized);
  m_query_scope = scope;

  mysql_rwlock_rdlock(&LOCK_system_variables_hash);
  DEBUG_SYNC(m_current_thd, "acquired_LOCK_system_variables_hash");

  /* Record the system variable hash version to detect subsequent changes. */
  m_version = get_system_variable_hash_version();

  /* Build the SHOW_VAR array from the system variable hash. */
  enumerate_sys_vars(&m_show_var_array, true, m_query_scope, strict);

  mysql_rwlock_unlock(&LOCK_system_variables_hash);

  /* Increase cache size if necessary. */
  m_cache.reserve(m_show_var_array.size());

  m_initialized = true;
  return true;
}

/**
  Build an array of SHOW_VARs from the system variable hash.
  Filter for SESSION scope.
*/
bool PFS_system_variable_cache::do_initialize_session(void) {
  /* Block plugins from unloading. */
  mysql_mutex_lock(&LOCK_plugin_delete);

  /* Build the array. */
  bool ret = init_show_var_array(OPT_SESSION, true);

  mysql_mutex_unlock(&LOCK_plugin_delete);
  return ret;
}

/**
  Match system variable scope to desired scope.
*/
bool PFS_system_variable_cache::match_scope(int scope) {
  switch (scope) {
    case sys_var::GLOBAL:
      return m_query_scope == OPT_GLOBAL;
      break;

    case sys_var::SESSION:
      return (m_query_scope == OPT_GLOBAL || m_query_scope == OPT_SESSION);
      break;

    case sys_var::ONLY_SESSION:
      return m_query_scope == OPT_SESSION;
      break;

    default:
      return false;
      break;
  }
  return false;
}

/**
  Build a GLOBAL system variable cache.
*/
int PFS_system_variable_cache::do_materialize_global(void) {
  /* Block plugins from unloading. */
  mysql_mutex_lock(&LOCK_plugin_delete);

  m_materialized = false;

  /*
     Build array of SHOW_VARs from system variable hash. Do this within
     LOCK_plugin_delete to ensure that the hash table remains unchanged
     during materialization.
   */
  if (!m_external_init) {
    init_show_var_array(OPT_GLOBAL, true);
  }

  /* Resolve the value for each SHOW_VAR in the array, add to cache. */
  for (Show_var_array::iterator show_var = m_show_var_array.begin();
       show_var->value && (show_var != m_show_var_array.end()); show_var++) {
    sys_var *value = (sys_var *)show_var->value;
    DBUG_ASSERT(value);

    /* Match the system variable scope to the target scope. */
    if (match_scope(value->scope())) {
      /* Resolve value, convert to text, add to cache. */
      System_variable system_var(m_current_thd, show_var, m_query_scope);
      m_cache.push_back(system_var);
    }
  }

  m_materialized = true;
  mysql_mutex_unlock(&LOCK_plugin_delete);
  return 0;
}

/**
  Build a GLOBAL and SESSION system variable cache.
*/
int PFS_system_variable_cache::do_materialize_all(THD *unsafe_thd) {
  int ret = 1;

  m_unsafe_thd = unsafe_thd;
  m_safe_thd = NULL;
  m_materialized = false;
  m_cache.clear();

  /* Block plugins from unloading. */
  mysql_mutex_lock(&LOCK_plugin_delete);

  /*
     Build array of SHOW_VARs from system variable hash. Do this within
     LOCK_plugin_delete to ensure that the hash table remains unchanged
     while this thread is materialized.
   */
  if (!m_external_init) {
    init_show_var_array(OPT_SESSION, false);
  }

  /* Get and lock a validated THD from the thread manager. */
  if ((m_safe_thd = get_THD(unsafe_thd)) != NULL) {
    for (Show_var_array::iterator show_var = m_show_var_array.begin();
         show_var->value && (show_var != m_show_var_array.end()); show_var++) {
      /* Resolve value, convert to text, add to cache. */
      System_variable system_var(m_safe_thd, show_var, m_query_scope);
      m_cache.push_back(system_var);
    }

    /* Release lock taken in get_THD(). */
    mysql_mutex_unlock(&m_safe_thd->LOCK_thd_data);

    m_materialized = true;
    ret = 0;
  }

  mysql_mutex_unlock(&LOCK_plugin_delete);
  return ret;
}

/**
  Allocate and assign mem_root for system variable materialization.
*/
void PFS_system_variable_cache::set_mem_root(void) {
  if (m_mem_sysvar_ptr == NULL) {
    init_sql_alloc(PSI_INSTRUMENT_ME, &m_mem_sysvar, SYSVAR_MEMROOT_BLOCK_SIZE,
                   0);
    m_mem_sysvar_ptr = &m_mem_sysvar;
  }
  m_mem_thd = THR_MALLOC;      /* pointer to current THD mem_root */
  m_mem_thd_save = *m_mem_thd; /* restore later */
  *m_mem_thd = &m_mem_sysvar;  /* use temporary mem_root */
}

/**
  Mark memory blocks in the temporary mem_root as free.
  Restore THD::mem_root.
*/
void PFS_system_variable_cache::clear_mem_root(void) {
  if (m_mem_sysvar_ptr) {
    free_root(&m_mem_sysvar, MYF(MY_MARK_BLOCKS_FREE));
    *m_mem_thd = m_mem_thd_save; /* restore original mem_root */
    m_mem_thd = NULL;
    m_mem_thd_save = NULL;
  }
}

/**
  Free the temporary mem_root.
  Restore THD::mem_root if necessary.
*/
void PFS_system_variable_cache::free_mem_root(void) {
  if (m_mem_sysvar_ptr) {
    free_root(&m_mem_sysvar, MYF(0));
    m_mem_sysvar_ptr = NULL;
    if (m_mem_thd && m_mem_thd_save) {
      *m_mem_thd = m_mem_thd_save; /* restore original mem_root */
      m_mem_thd = NULL;
      m_mem_thd_save = NULL;
    }
  }
}

/**
  Build a SESSION system variable cache for a pfs_thread.
  Requires that init_show_var_array() has already been called.
  Return 0 for success.
*/
int PFS_system_variable_cache::do_materialize_session(PFS_thread *pfs_thread) {
  int ret = 1;

  m_pfs_thread = pfs_thread;
  m_materialized = false;
  m_cache.clear();

  /* Block plugins from unloading. */
  mysql_mutex_lock(&LOCK_plugin_delete);

  /* The SHOW_VAR array must be initialized externally. */
  DBUG_ASSERT(m_initialized);

  /* Use a temporary mem_root to avoid depleting THD mem_root. */
  if (m_use_mem_root) {
    set_mem_root();
  }

  /* Get and lock a validated THD from the thread manager. */
  if ((m_safe_thd = get_THD(pfs_thread)) != NULL) {
    for (Show_var_array::iterator show_var = m_show_var_array.begin();
         show_var->value && (show_var != m_show_var_array.end()); show_var++) {
      sys_var *value = (sys_var *)show_var->value;

      /* Match the system variable scope to the target scope. */
      if (match_scope(value->scope())) {
        /* Resolve value, convert to text, add to cache. */
        System_variable system_var(m_safe_thd, show_var, m_query_scope);
        m_cache.push_back(system_var);
      }
    }

    /* Release lock taken in get_THD(). */
    mysql_mutex_unlock(&m_safe_thd->LOCK_thd_data);

    m_materialized = true;
    ret = 0;
  }

  /* Mark mem_root blocks as free. */
  if (m_use_mem_root) {
    clear_mem_root();
  }

  mysql_mutex_unlock(&LOCK_plugin_delete);
  return ret;
}

/**
  Materialize a single system variable for a pfs_thread.
  Requires that init_show_var_array() has already been called.
  Return 0 for success.
*/
int PFS_system_variable_cache::do_materialize_session(PFS_thread *pfs_thread,
                                                      uint index) {
  int ret = 1;

  m_pfs_thread = pfs_thread;
  m_materialized = false;
  m_cache.clear();

  /* Block plugins from unloading. */
  mysql_mutex_lock(&LOCK_plugin_delete);

  /* The SHOW_VAR array must be initialized externally. */
  DBUG_ASSERT(m_initialized);

  /* Get and lock a validated THD from the thread manager. */
  if ((m_safe_thd = get_THD(pfs_thread)) != NULL) {
    SHOW_VAR *show_var = &m_show_var_array.at(index);

    if (show_var && show_var->value && (show_var != m_show_var_array.end())) {
      sys_var *value = (sys_var *)show_var->value;

      /* Match the system variable scope to the target scope. */
      if (match_scope(value->scope())) {
        /* Resolve value, convert to text, add to cache. */
        System_variable system_var(m_safe_thd, show_var, m_query_scope);
        m_cache.push_back(system_var);
      }
    }

    /* Release lock taken in get_THD(). */
    mysql_mutex_unlock(&m_safe_thd->LOCK_thd_data);

    m_materialized = true;
    ret = 0;
  }

  mysql_mutex_unlock(&LOCK_plugin_delete);
  return ret;
}

/**
  Build a SESSION system variable cache for a THD.
*/
int PFS_system_variable_cache::do_materialize_session(THD *unsafe_thd) {
  int ret = 1;

  m_unsafe_thd = unsafe_thd;
  m_safe_thd = NULL;
  m_materialized = false;
  m_cache.clear();

  /* Block plugins from unloading. */
  mysql_mutex_lock(&LOCK_plugin_delete);

  /*
     Build array of SHOW_VARs from system variable hash. Do this within
     LOCK_plugin_delete to ensure that the hash table remains unchanged
     while this thread is materialized.
   */
  if (!m_external_init) {
    init_show_var_array(OPT_SESSION, true);
  }

  /* Get and lock a validated THD from the thread manager. */
  if ((m_safe_thd = get_THD(unsafe_thd)) != NULL) {
    for (Show_var_array::iterator show_var = m_show_var_array.begin();
         show_var->value && (show_var != m_show_var_array.end()); show_var++) {
      sys_var *value = (sys_var *)show_var->value;

      /* Match the system variable scope to the target scope. */
      if (match_scope(value->scope())) {
        /* Resolve value, convert to text, add to cache. */
        System_variable system_var(m_safe_thd, show_var, m_query_scope);
        m_cache.push_back(system_var);
      }
    }

    /* Release lock taken in get_THD(). */
    mysql_mutex_unlock(&m_safe_thd->LOCK_thd_data);

    m_materialized = true;
    ret = 0;
  }

  mysql_mutex_unlock(&LOCK_plugin_delete);
  return ret;
}

/**
  CLASS PFS_system_variable_info_cache
*/

/**
  Build GLOBAL and SESSION system variable cache.
*/
int PFS_system_variable_info_cache::do_materialize_all(THD *unsafe_thd) {
  int ret = 1;

  m_unsafe_thd = unsafe_thd;
  m_safe_thd = NULL;
  m_materialized = false;
  m_cache.clear();

  /* Block plugins from unloading. */
  mysql_mutex_lock(&LOCK_plugin_delete);

  /*
     Build array of SHOW_VARs from system variable hash. Do this within
     LOCK_plugin_delete to ensure that the hash table remains unchanged
     while this thread is materialized.
   */
  if (!m_external_init) {
    init_show_var_array(OPT_SESSION, false);
  }

  /* Get and lock a validated THD from the thread manager. */
  if ((m_safe_thd = get_THD(unsafe_thd)) != NULL) {
    for (Show_var_array::iterator show_var = m_show_var_array.begin();
         show_var->value && (show_var != m_show_var_array.end()); show_var++) {
      /* Resolve value, convert to text, add to cache. */
      System_variable system_var(m_safe_thd, show_var);
      m_cache.push_back(system_var);
    }

    /* Release lock taken in get_THD(). */
    mysql_mutex_unlock(&m_safe_thd->LOCK_thd_data);

    m_materialized = true;
    ret = 0;
  }

  mysql_mutex_unlock(&LOCK_plugin_delete);
  return ret;
}

/**
  CLASS PFS_system_persisted_variables_cache
*/

/**
  Build PERSISTED system variable cache.
*/
int PFS_system_persisted_variables_cache::do_materialize_all(THD *unsafe_thd) {
  int ret = 1;
  m_unsafe_thd = unsafe_thd;
  m_safe_thd = NULL;
  m_materialized = false;
  m_cache.clear();

  /* Block plugins from unloading. */
  mysql_mutex_lock(&LOCK_plugin_delete);

  /* Get and lock a validated THD from the thread manager. */
  if ((m_safe_thd = get_THD(unsafe_thd)) != NULL) {
    Persisted_variables_cache *pv = Persisted_variables_cache::get_instance();
    if (pv) {
      vector<st_persist_var> *persist_variables = pv->get_persisted_variables();
      pv->lock();
      for (auto iter = persist_variables->begin();
           iter != persist_variables->end(); iter++) {
        System_variable system_var;
        system_var.m_charset = system_charset_info;

        system_var.m_name = iter->key.c_str();
        system_var.m_name_length = iter->key.length();
        system_var.m_value_length =
            std::min(SHOW_VAR_FUNC_BUFF_SIZE, (int)iter->value.length());
        memcpy(system_var.m_value_str, iter->value.c_str(),
               system_var.m_value_length);
        system_var.m_value_str[system_var.m_value_length] = 0;

        m_cache.push_back(system_var);
      }
      map<string, st_persist_var> *persist_ro_variables =
          pv->get_persist_ro_variables();
      for (auto ro_iter = persist_ro_variables->begin();
           ro_iter != persist_ro_variables->end(); ro_iter++) {
        System_variable system_var;
        system_var.m_charset = system_charset_info;

        system_var.m_name = ro_iter->first.c_str();
        system_var.m_name_length = ro_iter->first.length();
        system_var.m_value_length = std::min(
            SHOW_VAR_FUNC_BUFF_SIZE, (int)ro_iter->second.value.length());
        memcpy(system_var.m_value_str, ro_iter->second.value.c_str(),
               system_var.m_value_length);
        system_var.m_value_str[system_var.m_value_length] = 0;

        m_cache.push_back(system_var);
      }
      pv->unlock();
    }
    /* Release lock taken in get_THD(). */
    mysql_mutex_unlock(&m_safe_thd->LOCK_thd_data);

    m_materialized = true;
    ret = 0;
  }

  mysql_mutex_unlock(&LOCK_plugin_delete);
  return ret;
}

/**
  CLASS System_variable
*/

/**
  Empty placeholder.
*/
System_variable::System_variable()
    : m_name(NULL),
      m_name_length(0),
      m_value_length(0),
      m_type(SHOW_UNDEF),
      m_scope(0),
      m_charset(NULL),
      m_source(enum_variable_source::COMPILED),
      m_path_length(0),
      m_min_value_length(0),
      m_max_value_length(0),
      m_set_time(0),
      m_set_user_str_length(0),
      m_set_host_str_length(0),
      m_initialized(false) {
  m_value_str[0] = '\0';
  m_path_str[0] = '\0';
  m_min_value_str[0] = '\0';
  m_max_value_str[0] = '\0';
  m_set_user_str[0] = '\0';
  m_set_host_str[0] = '\0';
}

/**
  GLOBAL or SESSION system variable.
*/
System_variable::System_variable(THD *target_thd, const SHOW_VAR *show_var,
                                 enum_var_type query_scope)
    : m_name(NULL),
      m_name_length(0),
      m_value_length(0),
      m_type(SHOW_UNDEF),
      m_scope(0),
      m_charset(NULL),
      m_source(enum_variable_source::COMPILED),
      m_path_length(0),
      m_min_value_length(0),
      m_max_value_length(0),
      m_set_time(0),
      m_set_user_str_length(0),
      m_set_host_str_length(0),
      m_initialized(false) {
  m_value_str[0] = '\0';
  m_path_str[0] = '\0';
  m_min_value_str[0] = '\0';
  m_max_value_str[0] = '\0';
  m_set_user_str[0] = '\0';
  m_set_host_str[0] = '\0';
  init(target_thd, show_var, query_scope);
}

/**
  GLOBAL and SESSION system variable.
*/
System_variable::System_variable(THD *target_thd, const SHOW_VAR *show_var)
    : m_name(NULL),
      m_name_length(0),
      m_value_length(0),
      m_type(SHOW_UNDEF),
      m_scope(0),
      m_charset(NULL),
      m_source(enum_variable_source::COMPILED),
      m_path_length(0),
      m_min_value_length(0),
      m_max_value_length(0),
      m_set_time(0),
      m_set_user_str_length(0),
      m_set_host_str_length(0),
      m_initialized(false) {
  m_value_str[0] = '\0';
  m_path_str[0] = '\0';
  m_min_value_str[0] = '\0';
  m_max_value_str[0] = '\0';
  m_set_user_str[0] = '\0';
  m_set_host_str[0] = '\0';
  init(target_thd, show_var);
}

/**
  Get sys_var value from global or local source then convert to string.
*/
void System_variable::init(THD *target_thd, const SHOW_VAR *show_var,
                           enum_var_type query_scope) {
  if (show_var == NULL || show_var->name == NULL) {
    return;
  }

  enum_mysql_show_type show_var_type = show_var->type;
  DBUG_ASSERT(show_var_type == SHOW_SYS);
  THD *current_thread = current_thd;

  m_name = show_var->name;
  m_name_length = strlen(m_name);

  /* Block remote target thread from updating this system variable. */
  if (target_thd != current_thread) {
    mysql_mutex_lock(&target_thd->LOCK_thd_sysvar);
  }
  /* Block system variable additions or deletions. */
  mysql_mutex_lock(&LOCK_global_system_variables);

  sys_var *system_var = (sys_var *)show_var->value;
  DBUG_ASSERT(system_var != NULL);
  m_charset = system_var->charset(target_thd);
  m_type = system_var->show_type();
  m_scope = system_var->scope();

  /* Get the value of the system variable. */
  const char *value;
  value = get_one_variable_ext(current_thread, target_thd, show_var,
                               query_scope, show_var_type, NULL, &m_charset,
                               m_value_str, &m_value_length);

  m_value_length = MY_MIN(m_value_length, SHOW_VAR_FUNC_BUFF_SIZE);

  /* Returned value may reference a string other than m_value_str. */
  if (value != m_value_str) {
    memcpy(m_value_str, value, m_value_length);
  }
  m_value_str[m_value_length] = 0;

  mysql_mutex_unlock(&LOCK_global_system_variables);
  if (target_thd != current_thread) {
    mysql_mutex_unlock(&target_thd->LOCK_thd_sysvar);
  }

  m_initialized = true;
}

/**
  Get sys_var value from global/session and then convert to string.
*/
void System_variable::init(THD *target_thd, const SHOW_VAR *show_var) {
  if (show_var == NULL || show_var->name == NULL) {
    return;
  }

  THD *current_thread = current_thd;

  m_name = show_var->name;
  m_name_length = strlen(m_name);

  /* Block remote target thread from updating this system variable. */
  if (target_thd != current_thread) {
    mysql_mutex_lock(&target_thd->LOCK_thd_sysvar);
  }
  /* Block system variable additions or deletions. */
  mysql_mutex_lock(&LOCK_global_system_variables);

  sys_var *system_var = (sys_var *)show_var->value;
  DBUG_ASSERT(system_var != NULL);
  m_charset = system_var->charset(target_thd);
  m_type = system_var->show_type();
  m_scope = system_var->scope();

  m_value_str[0] = '\0';
  m_value_length = 0;

  memset(m_path_str, 0, sizeof(m_path_str));
  m_path_length = 0;

  if (system_var->get_source()) {
    if (system_var->get_source_name()) {
      string src_name = system_var->get_source_name();
      m_path_length = src_name.length();
      memcpy(m_path_str, src_name.c_str(), m_path_length);
      m_path_str[m_path_length] = 0;
    }
    m_source = system_var->get_source();
  }
  snprintf(m_min_value_str, sizeof(m_min_value_str), "%lld",
           system_var->get_min_value());
  m_min_value_length = strlen(m_min_value_str);
  snprintf(m_max_value_str, sizeof(m_max_value_str), "%llu",
           system_var->get_max_value());
  m_max_value_length = strlen(m_max_value_str);

  m_set_time = system_var->get_timestamp();
  m_set_user_str_length = strlen(system_var->get_user());
  memcpy(m_set_user_str, system_var->get_user(), m_set_user_str_length);
  m_set_host_str_length = strlen(system_var->get_host());
  memcpy(m_set_host_str, system_var->get_host(), m_set_host_str_length);

  /*
   Read only persisted variables are handled as part of command line options,
   this will not update variable properties like user/host/timestamp in the
   corresponding sys_var instance, thus we do a look up in
   m_persist_ro_variables
   which got populated while reading mysqld-auto.cnf. If variable is present in
   m_persist_ro_variables we copy the properties into system_var.
  */
  Persisted_variables_cache *pv = Persisted_variables_cache::get_instance();
  if (pv) {
    map<string, st_persist_var> *persist_ro_variables =
        pv->get_persist_ro_variables();
    pv->lock();
    auto ro_iter = persist_ro_variables->find(m_name);
    if (ro_iter != persist_ro_variables->end()) {
      m_set_time = ro_iter->second.timestamp;
      m_set_user_str_length = ro_iter->second.user.length();
      memcpy(m_set_user_str, ro_iter->second.user.c_str(),
             m_set_user_str_length);
      m_set_host_str_length = ro_iter->second.host.length();
      memcpy(m_set_host_str, ro_iter->second.host.c_str(),
             m_set_host_str_length);
    }
    pv->unlock();
  }
  mysql_mutex_unlock(&LOCK_global_system_variables);
  if (target_thd != current_thread) {
    mysql_mutex_unlock(&target_thd->LOCK_thd_sysvar);
  }

  m_initialized = true;
}

/**
  CLASS PFS_status_variable_cache
*/

PFS_status_variable_cache::PFS_status_variable_cache(bool external_init)
    : PFS_variable_cache<Status_variable>(external_init),
      m_show_command(false),
      m_sum_client_status(NULL) {
  /* Determine if the originating query is a SHOW command. */
  m_show_command = (m_current_thd->lex->sql_command == SQLCOM_SHOW_STATUS);
}

/**
  Build cache of SESSION status variables for a user.
*/
int PFS_status_variable_cache::materialize_user(PFS_user *pfs_user) {
  if (!pfs_user) {
    return 1;
  }

  if (is_materialized(pfs_user)) {
    return 0;
  }

  if (!pfs_user->m_lock.is_populated()) {
    return 1;
  }

  /* Set callback function. */
  m_sum_client_status = sum_user_status;
  return do_materialize_client((PFS_client *)pfs_user);
}

/**
  Build cache of SESSION status variables for a host.
*/
int PFS_status_variable_cache::materialize_host(PFS_host *pfs_host) {
  if (!pfs_host) {
    return 1;
  }

  if (is_materialized(pfs_host)) {
    return 0;
  }

  if (!pfs_host->m_lock.is_populated()) {
    return 1;
  }

  /* Set callback function. */
  m_sum_client_status = sum_host_status;
  return do_materialize_client((PFS_client *)pfs_host);
}

/**
  Build cache of SESSION status variables for an account.
*/
int PFS_status_variable_cache::materialize_account(PFS_account *pfs_account) {
  if (!pfs_account) {
    return 1;
  }

  if (is_materialized(pfs_account)) {
    return 0;
  }

  if (!pfs_account->m_lock.is_populated()) {
    return 1;
  }

  /* Set callback function. */
  m_sum_client_status = sum_account_status;
  return do_materialize_client((PFS_client *)pfs_account);
}
/**
  Compare status variable scope to desired scope.
  @param variable_scope         Scope of current status variable
  @param strict                 Strict mode, for compatibility with SHOW
  @return true if variable matches the query scope
*/
bool PFS_status_variable_cache::match_scope(SHOW_SCOPE variable_scope,
                                            bool strict) {
  switch (variable_scope) {
    case SHOW_SCOPE_GLOBAL:
      return (m_query_scope == OPT_GLOBAL) ||
             (!strict && (m_query_scope == OPT_SESSION));
      break;
    case SHOW_SCOPE_SESSION:
      /* Ignore session-only vars if aggregating by user, host or account. */
      if (m_aggregate) {
        return false;
      } else {
        return (m_query_scope == OPT_SESSION);
      }
      break;
    case SHOW_SCOPE_ALL:
      return (m_query_scope == OPT_GLOBAL || m_query_scope == OPT_SESSION);
      break;
    case SHOW_SCOPE_UNDEF:
    default:
      return false;
      break;
  }
  return false;
}

/*
  Exclude specific status variables from the query by name or prefix.
  Return true if variable should be filtered.
*/
bool PFS_status_variable_cache::filter_by_name(const SHOW_VAR *show_var) {
  DBUG_ASSERT(show_var);
  DBUG_ASSERT(show_var->name);

  if (show_var->type == SHOW_ARRAY) {
    /* The SHOW_ARRAY name is the prefix for the variables in the sub array. */
    const char *prefix = show_var->name;
    /* Exclude COM counters if not a SHOW STATUS command. */
    if (!my_strcasecmp(system_charset_info, prefix, "Com") && !m_show_command) {
      return true;
    }
  } else {
    /*
      Slave status resides in Performance Schema replication tables. Exclude
      these slave status variables from the SHOW STATUS command and from the
      status tables.
      Assume null prefix to ensure that only server-defined slave status
      variables are filtered.
    */
    const char *name = show_var->name;
    if (!my_strcasecmp(system_charset_info, name, "Slave_running") ||
        !my_strcasecmp(system_charset_info, name,
                       "Slave_retried_transactions") ||
        !my_strcasecmp(system_charset_info, name, "Slave_last_heartbeat") ||
        !my_strcasecmp(system_charset_info, name,
                       "Slave_received_heartbeats") ||
        !my_strcasecmp(system_charset_info, name, "Slave_heartbeat_period")) {
      return true;
    }
  }

  return false;
}

/**
  Check that the variable type is aggregatable.

  @param variable_type         Status variable type
  @return true if variable type can be aggregated
*/
bool PFS_status_variable_cache::can_aggregate(
    enum_mysql_show_type variable_type) {
  switch (variable_type) {
    /*
      All server status counters that are totaled across threads are defined in
      system_status_var as either SHOW_LONGLONG_STATUS or SHOW_LONG_STATUS.
      These data types are not available to plugins.
    */
    case SHOW_LONGLONG_STATUS:
    case SHOW_LONG_STATUS:
      return true;
      break;

    /* Server and plugin */
    case SHOW_UNDEF:
    case SHOW_BOOL:
    case SHOW_CHAR:
    case SHOW_CHAR_PTR:
    case SHOW_ARRAY:
    case SHOW_FUNC:
    case SHOW_INT:
    case SHOW_LONG:
    case SHOW_LONGLONG:
    case SHOW_DOUBLE:
    /* Server only */
    case SHOW_HAVE:
    case SHOW_MY_BOOL:
    case SHOW_SYS:
    case SHOW_LEX_STRING:
    case SHOW_KEY_CACHE_LONG:
    case SHOW_KEY_CACHE_LONGLONG:
    case SHOW_DOUBLE_STATUS:
    case SHOW_HA_ROWS:
    case SHOW_LONG_NOFLUSH:
    case SHOW_SIGNED_LONG:
    default:
      return false;
      break;
  }
}

/**
  Check if a status variable should be excluded from the query.
  Return true if the variable should be excluded.
*/
bool PFS_status_variable_cache::filter_show_var(const SHOW_VAR *show_var,
                                                bool strict) {
  /* Match the variable scope with the query scope. */
  if (!match_scope(show_var->scope, strict)) {
    return true;
  }

  /* Exclude specific status variables by name or prefix. */
  if (filter_by_name(show_var)) {
    return true;
  }

  /* For user, host or account, ignore variables having non-aggregatable types.
   */
  if (m_aggregate && !can_aggregate(show_var->type)) {
    return true;
  }

  return false;
}

/**
  Build an array of SHOW_VARs from the global status array. Expand nested
  sub arrays, filter unwanted variables.
  NOTE: Must be done inside of LOCK_status to guard against plugin load/unload.
*/
bool PFS_status_variable_cache::init_show_var_array(enum_var_type scope,
                                                    bool strict) {
  DBUG_ASSERT(!m_initialized);

  /* Resize if necessary. */
  m_show_var_array.reserve(all_status_vars.size() + 1);

  m_query_scope = scope;

  for (Status_var_array::iterator show_var_iter = all_status_vars.begin();
       show_var_iter != all_status_vars.end(); show_var_iter++) {
    SHOW_VAR show_var = *show_var_iter;

    /* Check if this status var should be excluded from the query. */
    if (filter_show_var(&show_var, strict)) {
      continue;
    }

    if (show_var.type == SHOW_ARRAY) {
      /* Expand nested sub array. The name is used as a prefix. */
      expand_show_var_array((SHOW_VAR *)show_var.value, show_var.name, strict);
    } else {
      show_var.name = make_show_var_name(NULL, show_var.name);
      m_show_var_array.push_back(show_var);
    }
  }

  /* Last element is NULL. */
  m_show_var_array.push_back(SHOW_VAR());

  /* Get the latest version of all_status_vars. */
  m_version = get_status_vars_version();

  /* Increase cache size if necessary. */
  m_cache.reserve(m_show_var_array.size());

  m_initialized = true;
  return true;
}

/**
  Expand a nested sub array of status variables, indicated by a type of
  SHOW_ARRAY.
*/
void PFS_status_variable_cache::expand_show_var_array(
    const SHOW_VAR *show_var_array, const char *prefix, bool strict) {
  for (const SHOW_VAR *show_var_ptr = show_var_array;
       show_var_ptr && show_var_ptr->name; show_var_ptr++) {
    SHOW_VAR show_var = *show_var_ptr;

    if (filter_show_var(&show_var, strict)) {
      continue;
    }

    if (show_var.type == SHOW_ARRAY) {
      char name_buf[SHOW_VAR_MAX_NAME_LEN];
      show_var.name =
          make_show_var_name(prefix, show_var.name, name_buf, sizeof(name_buf));
      /* Expand nested sub array. The name is used as a prefix. */
      expand_show_var_array((SHOW_VAR *)show_var.value, show_var.name, strict);
    } else {
      /* Add the SHOW_VAR element. Make a local copy of the name string. */
      show_var.name = make_show_var_name(prefix, show_var.name);
      m_show_var_array.push_back(show_var);
    }
  }
}

/**
  Build the complete status variable name, with prefix. Return in buffer
  provided.
*/
char *PFS_status_variable_cache::make_show_var_name(const char *prefix,
                                                    const char *name,
                                                    char *name_buf,
                                                    size_t buf_len) {
  DBUG_ASSERT(name_buf != NULL);
  char *prefix_end = name_buf;

  if (prefix && *prefix) {
    /* Drop the prefix into the front of the name buffer. */
    prefix_end = my_stpnmov(name_buf, prefix, buf_len - 1);
    *prefix_end++ = '_';
  }

  /* Restrict name length to remaining buffer size. */
  size_t max_name_len = name_buf + buf_len - prefix_end;

  /* Load the name into the buffer after the prefix. */
  my_stpnmov(prefix_end, name, max_name_len);
  name_buf[buf_len - 1] = 0;

  return (name_buf);
}

/**
  Make a copy of the name string prefixed with the sub array name if necessary.
*/
char *PFS_status_variable_cache::make_show_var_name(const char *prefix,
                                                    const char *name) {
  char name_buf[SHOW_VAR_MAX_NAME_LEN];
  size_t buf_len = sizeof(name_buf);
  make_show_var_name(prefix, name, name_buf, buf_len);
  return m_current_thd->mem_strdup(name_buf); /* freed at statement end */
}

/**
  Build an internal SHOW_VAR array from the external status variable array.
*/
bool PFS_status_variable_cache::do_initialize_session(void) {
  /* Acquire LOCK_status to guard against plugin load/unload. */
  if (m_current_thd->fill_status_recursion_level++ == 0) {
    mysql_mutex_lock(&LOCK_status);
  }

  bool ret = init_show_var_array(OPT_SESSION, true);

  if (m_current_thd->fill_status_recursion_level-- == 1) {
    mysql_mutex_unlock(&LOCK_status);
  }

  return ret;
}

/**
  For the current THD, use initial_status_vars taken from before the query
  start.
*/
System_status_var *PFS_status_variable_cache::set_status_vars(void) {
  System_status_var *status_vars;
  if (m_safe_thd == m_current_thd &&
      m_current_thd->initial_status_var != NULL) {
    status_vars = m_current_thd->initial_status_var;
  } else {
    status_vars = &m_safe_thd->status_var;
  }

  return status_vars;
}

/**
  Build cache for GLOBAL status variables using values totaled from all threads.
*/
int PFS_status_variable_cache::do_materialize_global(void) {
  System_status_var status_totals;

  m_materialized = false;
  DEBUG_SYNC(m_current_thd, "before_materialize_global_status_array");

  /* Acquire LOCK_status to guard against plugin load/unload. */
  if (m_current_thd->fill_status_recursion_level++ == 0) {
    mysql_mutex_lock(&LOCK_status);
  }

  /*
     Build array of SHOW_VARs from global status array. Do this within
     LOCK_status to ensure that the array remains unchanged during
     materialization.
   */
  if (!m_external_init) {
    init_show_var_array(OPT_GLOBAL, true);
  }

  /*
    Collect totals for all active threads. Start with global status vars as a
    baseline.
  */
  PFS_connection_status_visitor visitor(&status_totals);
  PFS_connection_iterator::visit_global(false, /* hosts */
                                        false, /* users */
                                        false, /* accounts */
                                        false, /* threads */
                                        true,  /* THDs */
                                        &visitor);
  /*
    Build the status variable cache using the SHOW_VAR array as a reference.
    Use the status totals collected from all threads.
  */
  manifest(m_current_thd, m_show_var_array.begin(), &status_totals, "", false,
           true);

  if (m_current_thd->fill_status_recursion_level-- == 1) {
    mysql_mutex_unlock(&LOCK_status);
  }

  m_materialized = true;
  DEBUG_SYNC(m_current_thd, "after_materialize_global_status_array");

  return 0;
}

/**
  Build GLOBAL and SESSION status variable cache using values for a
  non-instrumented thread.
*/
int PFS_status_variable_cache::do_materialize_all(THD *unsafe_thd) {
  int ret = 1;
  DBUG_ASSERT(unsafe_thd != NULL);

  m_unsafe_thd = unsafe_thd;
  m_materialized = false;
  m_cache.clear();

  /* Avoid recursive acquisition of LOCK_status. */
  if (m_current_thd->fill_status_recursion_level++ == 0) {
    mysql_mutex_lock(&LOCK_status);
  }

  /*
     Build array of SHOW_VARs from global status array. Do this within
     LOCK_status to ensure that the array remains unchanged while this
     thread is materialized.
   */
  if (!m_external_init) {
    init_show_var_array(OPT_SESSION, false);
  }

  /* Get and lock a validated THD from the thread manager. */
  if ((m_safe_thd = get_THD(unsafe_thd)) != NULL) {
    /*
      Build the status variable cache using the SHOW_VAR array as a reference.
      Use the status values from the THD protected by the thread manager lock.
    */
    System_status_var *status_vars = set_status_vars();
    manifest(m_safe_thd, m_show_var_array.begin(), status_vars, "", false,
             false);

    /* Release lock taken in get_THD(). */
    mysql_mutex_unlock(&m_safe_thd->LOCK_thd_data);

    m_materialized = true;
    ret = 0;
  }

  if (m_current_thd->fill_status_recursion_level-- == 1) {
    mysql_mutex_unlock(&LOCK_status);
  }
  return ret;
}

/**
  Build SESSION status variable cache using values for a non-instrumented
  thread.
*/
int PFS_status_variable_cache::do_materialize_session(THD *unsafe_thd) {
  int ret = 1;
  DBUG_ASSERT(unsafe_thd != NULL);

  m_unsafe_thd = unsafe_thd;
  m_materialized = false;
  m_cache.clear();

  /* Avoid recursive acquisition of LOCK_status. */
  if (m_current_thd->fill_status_recursion_level++ == 0) {
    mysql_mutex_lock(&LOCK_status);
  }

  /*
     Build array of SHOW_VARs from global status array. Do this within
     LOCK_status to ensure that the array remains unchanged while this
     thread is materialized.
   */
  if (!m_external_init) {
    init_show_var_array(OPT_SESSION, true);
  }

  /* Get and lock a validated THD from the thread manager. */
  if ((m_safe_thd = get_THD(unsafe_thd)) != NULL) {
    /*
      Build the status variable cache using the SHOW_VAR array as a reference.
      Use the status values from the THD protected by the thread manager lock.
    */
    System_status_var *status_vars = set_status_vars();
    manifest(m_safe_thd, m_show_var_array.begin(), status_vars, "", false,
             true);

    /* Release lock taken in get_THD(). */
    mysql_mutex_unlock(&m_safe_thd->LOCK_thd_data);

    m_materialized = true;
    ret = 0;
  }

  if (m_current_thd->fill_status_recursion_level-- == 1) {
    mysql_mutex_unlock(&LOCK_status);
  }
  return ret;
}

/**
  Build SESSION status variable cache using values for a PFS_thread.
  NOTE: Requires that init_show_var_array() has already been called.
*/
int PFS_status_variable_cache::do_materialize_session(PFS_thread *pfs_thread) {
  int ret = 1;
  DBUG_ASSERT(pfs_thread != NULL);

  m_pfs_thread = pfs_thread;
  m_materialized = false;
  m_cache.clear();

  /* Acquire LOCK_status to guard against plugin load/unload. */
  if (m_current_thd->fill_status_recursion_level++ == 0) {
    mysql_mutex_lock(&LOCK_status);
  }

  /* The SHOW_VAR array must be initialized externally. */
  DBUG_ASSERT(m_initialized);

  /* Get and lock a validated THD from the thread manager. */
  if ((m_safe_thd = get_THD(pfs_thread)) != NULL) {
    /*
      Build the status variable cache using the SHOW_VAR array as a reference.
      Use the status values from the THD protected by the thread manager lock.
    */
    System_status_var *status_vars = set_status_vars();
    manifest(m_safe_thd, m_show_var_array.begin(), status_vars, "", false,
             true);

    /* Release lock taken in get_THD(). */
    mysql_mutex_unlock(&m_safe_thd->LOCK_thd_data);

    m_materialized = true;
    ret = 0;
  }

  if (m_current_thd->fill_status_recursion_level-- == 1) {
    mysql_mutex_unlock(&LOCK_status);
  }
  return ret;
}

/**
  Build cache of SESSION status variables using the status values provided.
  The cache is associated with a user, host or account, but not with any
  particular thread.
  NOTE: Requires that init_show_var_array() has already been called.
*/
int PFS_status_variable_cache::do_materialize_client(PFS_client *pfs_client) {
  DBUG_ASSERT(pfs_client != NULL);
  System_status_var status_totals;

  m_pfs_client = pfs_client;
  m_materialized = false;
  m_cache.clear();

  /* Acquire LOCK_status to guard against plugin load/unload. */
  if (m_current_thd->fill_status_recursion_level++ == 0) {
    mysql_mutex_lock(&LOCK_status);
  }

  /* The SHOW_VAR array must be initialized externally. */
  DBUG_ASSERT(m_initialized);

  /*
    Generate status totals from active threads and from totals aggregated
    from disconnected threads.
  */
  m_sum_client_status(pfs_client, &status_totals);

  /*
    Build the status variable cache using the SHOW_VAR array as a reference and
    the status totals collected from threads associated with this client.
  */
  manifest(m_current_thd, m_show_var_array.begin(), &status_totals, "", false,
           true);

  if (m_current_thd->fill_status_recursion_level-- == 1) {
    mysql_mutex_unlock(&LOCK_status);
  }

  m_materialized = true;
  return 0;
}

/*
  Build the status variable cache from the expanded and sorted SHOW_VAR array.
  Resolve status values using the System_status_var struct provided.
*/
void PFS_status_variable_cache::manifest(THD *thd,
                                         const SHOW_VAR *show_var_array,
                                         System_status_var *status_vars,
                                         const char *prefix, bool nested_array,
                                         bool strict) {
  for (const SHOW_VAR *show_var_iter = show_var_array;
       show_var_iter && show_var_iter->name; show_var_iter++) {
    // work buffer, must be aligned to handle long/longlong values
    my_aligned_storage<SHOW_VAR_FUNC_BUFF_SIZE + 1, MY_ALIGNOF(longlong)>
        value_buf;
    SHOW_VAR show_var_tmp;
    const SHOW_VAR *show_var_ptr = show_var_iter; /* preserve array pointer */

    /*
      If the value is a function reference, then execute the function and
      reevaluate the new SHOW_TYPE and value. Handle nested case where
      SHOW_FUNC resolves to another SHOW_FUNC.
    */
    if (show_var_ptr->type == SHOW_FUNC) {
      show_var_tmp = *show_var_ptr;
      /*
        Execute the function reference in show_var_tmp->value, which returns
        show_var_tmp with a new type and new value.
      */
      for (const SHOW_VAR *var = show_var_ptr; var->type == SHOW_FUNC;
           var = &show_var_tmp) {
        ((mysql_show_var_func)(var->value))(thd, &show_var_tmp, value_buf.data);
      }
      show_var_ptr = &show_var_tmp;
    }

    /*
      If we are expanding a SHOW_ARRAY, filter variables that were not
      prefiltered by
      init_show_var_array().
    */
    if (nested_array && filter_show_var(show_var_ptr, strict)) {
      continue;
    }

    if (show_var_ptr->type == SHOW_ARRAY) {
      /*
        Status variables of type SHOW_ARRAY were expanded and filtered by
        init_show_var_array(), except where a SHOW_FUNC resolves into a
        SHOW_ARRAY, such as with InnoDB. Recurse to expand the sub array.
      */
      manifest(thd, (SHOW_VAR *)show_var_ptr->value, status_vars,
               show_var_ptr->name, true, strict);
    } else {
      /* Add the materialized status variable to the cache. */
      SHOW_VAR show_var = *show_var_ptr;
      /*
        For nested array expansions, make a copy of the variable name, just as
        done in init_show_var_array().
      */
      if (nested_array) {
        show_var.name = make_show_var_name(prefix, show_var_ptr->name);
      }

      /* Convert status value to string format. Add to the cache. */
      Status_variable status_var(&show_var, status_vars, m_query_scope);
      m_cache.push_back(status_var);
    }
  }
}

/**
  CLASS Status_variable
*/
Status_variable::Status_variable(const SHOW_VAR *show_var,
                                 System_status_var *status_vars,
                                 enum_var_type query_scope)
    : m_name_length(0),
      m_value_length(0),
      m_type(SHOW_UNDEF),
      m_scope(SHOW_SCOPE_UNDEF),
      m_charset(NULL),
      m_initialized(false) {
  init(show_var, status_vars, query_scope);
}

/**
  Resolve status value, convert to string.
  show_var->value is an offset into status_vars.
  NOTE: Assumes LOCK_status is held.
*/
void Status_variable::init(const SHOW_VAR *show_var,
                           System_status_var *status_vars,
                           enum_var_type query_scope) {
  if (show_var == NULL || show_var->name == NULL) {
    return;
  }
  m_name = show_var->name;
  m_name_length = strlen(m_name);
  m_type = show_var->type;
  m_scope = show_var->scope;

  /* Get the value of the status variable. */
  const char *value;
  value =
      get_one_variable(current_thd, show_var, query_scope, m_type, status_vars,
                       &m_charset, m_value_str, &m_value_length);
  m_value_length = MY_MIN(m_value_length, SHOW_VAR_FUNC_BUFF_SIZE);

  /* Returned value may reference a string other than m_value_str. */
  if (value != m_value_str) {
    memcpy(m_value_str, value, m_value_length);
  }
  m_value_str[m_value_length] = 0;

  m_initialized = true;
}

/*
  Get status totals for this user from active THDs and related accounts.
*/
void sum_user_status(PFS_client *pfs_user, System_status_var *status_totals) {
  PFS_connection_status_visitor visitor(status_totals);
  PFS_connection_iterator::visit_user((PFS_user *)pfs_user, true, /* accounts */
                                      false,                      /* threads */
                                      true,                       /* THDs */
                                      &visitor);
}

/*
  Get status totals for this host from active THDs and related accounts.
*/
void sum_host_status(PFS_client *pfs_host, System_status_var *status_totals) {
  PFS_connection_status_visitor visitor(status_totals);
  PFS_connection_iterator::visit_host((PFS_host *)pfs_host, true, /* accounts */
                                      false,                      /* threads */
                                      true,                       /* THDs */
                                      &visitor);
}

/*
  Get status totals for this account from active THDs and from totals aggregated
  from disconnectd threads.
*/
void sum_account_status(PFS_client *pfs_account,
                        System_status_var *status_totals) {
  PFS_connection_status_visitor visitor(status_totals);
  PFS_connection_iterator::visit_account((PFS_account *)pfs_account,
                                         false, /* threads */
                                         true,  /* THDs */
                                         &visitor);
}

/**
  Reset aggregated status counter stats for account, user and host.
  NOTE: Assumes LOCK_status is held.
*/
void reset_pfs_status_stats() {
  reset_status_by_account();
  reset_status_by_user();
  reset_status_by_host();
  /* Clear again, updated by previous aggregations. */
  reset_global_status();
}

/**
  Warning issued if the version of the system variable hash table changes
  during a query. This can happen when a plugin is loaded or unloaded.
*/
void system_variable_warning(void) {
  THD *thd = current_thd;
  DBUG_ASSERT(thd != NULL);
  push_warning_printf(thd, Sql_condition::SL_WARNING, ER_WARN_TOO_FEW_RECORDS,
                      ER_THD(thd, ER_WARN_TOO_FEW_RECORDS),
                      "System variable hash changed during query.");
}

/**
  Warning issued if the global status variable array changes during a query.
  This can happen when a plugin is loaded or unloaded.
*/
void status_variable_warning(void) {
  THD *thd = current_thd;
  DBUG_ASSERT(thd != NULL);
  push_warning_printf(thd, Sql_condition::SL_WARNING, ER_WARN_TOO_FEW_RECORDS,
                      ER_THD(thd, ER_WARN_TOO_FEW_RECORDS),
                      "Global status variable array changed during query.");
}
/** @} */
