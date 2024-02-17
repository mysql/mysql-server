/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */
#include "sql/auth/dynamic_privileges_impl.h"

#include <ctype.h>
#include <mysql/components/my_service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/dynamic_privilege.h>
#include <mysql/service_plugin_registry.h>
#include <stddef.h>
#include <string>
#include <unordered_set>
#include <utility>

#include "m_string.h"
#include "mysql/components/service.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/components/services/registry.h"
#include "sql/auth/dynamic_privilege_table.h"
#include "sql/auth/sql_auth_cache.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/current_thd.h"
#include "sql/mysqld.h"
#include "sql/mysqld_thd_manager.h"
#include "sql/sql_thd_internal_api.h"  // create_internal_thd
#include "string_with_len.h"

class THD;

/**
  This helper class is used for either selecting a previous THD or
  if it's missing, create a new THD.
*/
class Thd_creator {
 public:
  Thd_creator(THD *thd) : m_thd(thd), m_tmp_thd(nullptr) {}

  /**
    Returns a THD handle either by creating a new one or by returning a
    previously created THD.
  */
  THD *operator()() {
    if (m_thd == nullptr && m_tmp_thd == nullptr) {
      /*
        Initiate a THD without plugins,
        without attaching to the Global_THD_manager, and without setting
        an OS thread ID.
        The global THD manager is still needed to create the thread through.
      */
      assert(Global_THD_manager::is_initialized());
      m_tmp_thd = create_internal_thd();
      return m_tmp_thd;
    } else if (m_thd == nullptr) {
      return m_tmp_thd;
    }
    return m_thd;
  }

  /**
    Automatically frees any THD handle created by this class.
  */
  ~Thd_creator() {
    if (m_thd == nullptr && m_tmp_thd != nullptr) {
      destroy_internal_thd(m_tmp_thd);
    }
  }

 private:
  THD *m_thd;
  THD *m_tmp_thd;
};

/**
  Register a privilege identifiers in the list of known identifiers. This
  enable the SQL syntax to recognize the identifier as a valid token.
  @param privilege_str The privilege identifier string
  @param privilege_str_len The length of the identifier string

  @note This function acquires the THD from the current_thd

  @returns Error flag
    @return true The privilege ID couldn't be inserted.
    @return false The privilege ID was successfully registered.
*/

DEFINE_BOOL_METHOD(dynamic_privilege_services_impl::register_privilege,
                   (const char *privilege_str, size_t privilege_str_len)) {
  try {
    std::string priv;
    const char *c = &privilege_str[0];
    for (size_t i = 0; i < privilege_str_len; ++i, ++c)
      priv.append(1, static_cast<char>(toupper(*c)));

    Thd_creator get_thd(current_thd);
    Acl_cache_lock_guard acl_cache_lock(get_thd(),
                                        Acl_cache_lock_mode::WRITE_MODE);
    acl_cache_lock.lock();
    /* If the privilege ID already is registered; report success */
    if (is_dynamic_privilege_defined(priv)) return false;

    return !get_dynamic_privilege_register()->insert(priv).second;
  } catch (...) {
    return true;
  }
}

/**
  Unregister a privilege identifiers in the list of known identifiers. This
  disables the SQL syntax from recognizing the identifier as a valid token.
  @param privilege_str The privilege identifier string
  @param privilege_str_len The length of the identifier string

  @note This function acquires the THD from the current_thd

  @returns Error flag
    @return true The privilege ID wasn't in the list or remove failed.
    @return false The privilege ID was successfully unregistered.
*/

DEFINE_BOOL_METHOD(dynamic_privilege_services_impl::unregister_privilege,
                   (const char *privilege_str, size_t privilege_str_len)) {
  try {
    std::string priv;
    const char *c = &privilege_str[0];
    for (size_t i = 0; i < privilege_str_len; ++i, ++c)
      priv.append(1, static_cast<char>(toupper(*c)));

    /*
      This function may be called after the thd manager is gone, e.g.
      from component deinitialization.
      In this case it can just remove the priv from the global list
      without taking locks.
    */
    if (Global_THD_manager::is_initialized()) {
      Thd_creator get_thd(current_thd);
      Acl_cache_lock_guard acl_cache_lock(get_thd(),
                                          Acl_cache_lock_mode::WRITE_MODE);
      DBUG_EXECUTE_IF("bug34594035_simulate_lock_failure",
                      DBUG_SET("+d,bug34594035_fail_acl_cache_lock"););
      acl_cache_lock.lock();
      /* do a best effort erase from the deprecations too */
      get_dynamic_privilege_deprecations()->erase(priv);

      return (get_dynamic_privilege_register()->erase(priv) == 0);
    } else {
      /* do a best effort erase from the deprecations too */
      get_dynamic_privilege_deprecations()->erase(priv);
      return (get_dynamic_privilege_register()->erase(priv) == 0);
    }
  } catch (...) {
    return true;
  }
}

/**
  Checks if a user has a specified privilege ID granted to it.

  @param handle The active security context of the user to be checked.
  @param privilege_str The privilege identifier string
  @param privilege_str_len The length of the identifier string

  @returns Success state
    @return true The user has the grant
    @return false The user hasn't the grant
*/

DEFINE_BOOL_METHOD(dynamic_privilege_services_impl::has_global_grant,
                   (Security_context_handle handle, const char *privilege_str,
                    size_t privilege_str_len)) {
  Security_context *sctx = reinterpret_cast<Security_context *>(handle);
  return sctx->has_global_grant(privilege_str, privilege_str_len).first;
}

DEFINE_BOOL_METHOD(dynamic_privilege_services_impl::add_deprecated,
                   (const char *priv_name, size_t priv_name_len)) {
  try {
    std::string priv;
    const char *c = &priv_name[0];
    for (size_t i = 0; i < priv_name_len; ++i, ++c)
      priv.append(1, static_cast<char>(toupper(*c)));

    if (Global_THD_manager::is_initialized()) {
      Thd_creator get_thd(current_thd);
      Acl_cache_lock_guard acl_cache_lock(get_thd(),
                                          Acl_cache_lock_mode::WRITE_MODE);
      if (!acl_cache_lock.lock()) return true;

      /* If the privilege ID isn't registered report failure */
      if (!is_dynamic_privilege_defined(priv)) return true;

      /* If the privilege ID already is deprecated; report success */
      if (is_dynamic_privilege_deprecated(priv)) return false;
      return !get_dynamic_privilege_deprecations()->insert(priv).second;
    } else {
      /* If the privilege ID isn't registered report failure */
      if (!is_dynamic_privilege_defined(priv)) return true;

      /* If the privilege ID already is deprecated; report success */
      if (is_dynamic_privilege_deprecated(priv)) return false;
      return !get_dynamic_privilege_deprecations()->insert(priv).second;
    }
  } catch (...) {
    return true;
  }
}

DEFINE_BOOL_METHOD(dynamic_privilege_services_impl::remove_deprecated,
                   (const char *priv_name, size_t priv_name_len)) {
  try {
    std::string priv;
    const char *c = &priv_name[0];
    for (size_t i = 0; i < priv_name_len; ++i, ++c)
      priv.append(1, static_cast<char>(toupper(*c)));

    /*
      This function may be called after the thd manager is gone, e.g.
      from component deinitialization.
      In this case it can just remove the priv from the global list
      without taking locks.
    */
    if (Global_THD_manager::is_initialized()) {
      Thd_creator get_thd(current_thd);
      Acl_cache_lock_guard acl_cache_lock(get_thd(),
                                          Acl_cache_lock_mode::WRITE_MODE);
      if (!acl_cache_lock.lock()) return true;

      /* If the privilege ID isn't registered report failure */
      if (!is_dynamic_privilege_defined(priv)) return true;

      return (get_dynamic_privilege_deprecations()->erase(priv) == 0);
    } else {
      /* If the privilege ID isn't registered report failure */
      if (!is_dynamic_privilege_defined(priv)) return true;
      return (get_dynamic_privilege_deprecations()->erase(priv) == 0);
    }
  } catch (...) {
    return true;
  }
}

/**
  Bootstrap the dynamic privilege service by seeding it with server
  implementation-specific data.
*/

bool dynamic_privilege_init(void) {
  int ret = false;

  // Set up default dynamic privileges
  my_service<SERVICE_TYPE(dynamic_privilege_register)> service(
      "dynamic_privilege_register.mysql_server", srv_registry);
  assert(service.is_valid());
  ret += service->register_privilege(STRING_WITH_LEN("ROLE_ADMIN"));
  ret += service->register_privilege(STRING_WITH_LEN("SYSTEM_VARIABLES_ADMIN"));
  ret += service->register_privilege(STRING_WITH_LEN("BINLOG_ADMIN"));
  ret +=
      service->register_privilege(STRING_WITH_LEN("REPLICATION_SLAVE_ADMIN"));
  ret +=
      service->register_privilege(STRING_WITH_LEN("GROUP_REPLICATION_ADMIN"));
  ret += service->register_privilege(STRING_WITH_LEN("ENCRYPTION_KEY_ADMIN"));
  ret += service->register_privilege(STRING_WITH_LEN("CONNECTION_ADMIN"));
  ret += service->register_privilege(STRING_WITH_LEN("XA_RECOVER_ADMIN"));
  ret += service->register_privilege(
      STRING_WITH_LEN("PERSIST_RO_VARIABLES_ADMIN"));
  ret += service->register_privilege(STRING_WITH_LEN("BACKUP_ADMIN"));
  ret += service->register_privilege(STRING_WITH_LEN("CLONE_ADMIN"));
  ret += service->register_privilege(STRING_WITH_LEN("RESOURCE_GROUP_ADMIN"));
  ret += service->register_privilege(STRING_WITH_LEN("RESOURCE_GROUP_USER"));
  ret +=
      service->register_privilege(STRING_WITH_LEN("SESSION_VARIABLES_ADMIN"));
  ret +=
      service->register_privilege(STRING_WITH_LEN("BINLOG_ENCRYPTION_ADMIN"));
  ret +=
      service->register_privilege(STRING_WITH_LEN("SERVICE_CONNECTION_ADMIN"));
  ret += service->register_privilege(
      STRING_WITH_LEN("APPLICATION_PASSWORD_ADMIN"));
  ret += service->register_privilege(STRING_WITH_LEN("SYSTEM_USER"));
  ret += service->register_privilege(STRING_WITH_LEN("TABLE_ENCRYPTION_ADMIN"));
  ret += service->register_privilege(STRING_WITH_LEN("AUDIT_ADMIN"));
  ret += service->register_privilege(STRING_WITH_LEN("TELEMETRY_LOG_ADMIN"));
  ret += service->register_privilege(STRING_WITH_LEN("REPLICATION_APPLIER"));
  ret += service->register_privilege(STRING_WITH_LEN("SHOW_ROUTINE"));
  ret += service->register_privilege(STRING_WITH_LEN("INNODB_REDO_LOG_ENABLE"));
  ret += service->register_privilege(STRING_WITH_LEN("FLUSH_OPTIMIZER_COSTS"));
  ret += service->register_privilege(STRING_WITH_LEN("FLUSH_STATUS"));
  ret += service->register_privilege(STRING_WITH_LEN("FLUSH_USER_RESOURCES"));
  ret += service->register_privilege(STRING_WITH_LEN("FLUSH_TABLES"));
  ret += service->register_privilege(STRING_WITH_LEN("FLUSH_PRIVILEGES"));
  ret +=
      service->register_privilege(STRING_WITH_LEN("GROUP_REPLICATION_STREAM"));
  ret += service->register_privilege(
      STRING_WITH_LEN("AUTHENTICATION_POLICY_ADMIN"));
  ret +=
      service->register_privilege(STRING_WITH_LEN("PASSWORDLESS_USER_ADMIN"));
  ret += service->register_privilege(
      STRING_WITH_LEN("SENSITIVE_VARIABLES_OBSERVER"));
  ret += service->register_privilege(STRING_WITH_LEN("SET_ANY_DEFINER"));
  ret +=
      service->register_privilege(STRING_WITH_LEN("ALLOW_NONEXISTENT_DEFINER"));
  ret += service->register_privilege(STRING_WITH_LEN("TRANSACTION_GTID_TAG"));
  ret += service->register_privilege(STRING_WITH_LEN("OPTIMIZE_LOCAL_TABLE"));

  return ret != 0;
}
