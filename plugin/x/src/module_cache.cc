/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/module_cache.h"

#include "plugin/x/src/interface/sha256_password_cache.h"
#include "plugin/x/src/module_mysqlx.h"
#include "plugin/x/src/sha256_password_cache.h"

namespace modules {

volatile bool Module_cache::m_is_sha256_password_cache_enabled = false;

/**
  Handle an authentication audit event.

  @param [in] thd         MySQL Thread Handle
  @param [in] event_class Event class information
  @param [in] event       Event structure

  @returns Success always.
*/

static int audit_cache_clean_event_notify(MYSQL_THD thd,
                                          mysql_event_class_t event_class,
                                          const void *event) {
  if (event_class == MYSQL_AUDIT_SERVER_STARTUP_CLASS) {
    auto server_obj_with_lock = modules::Module_mysqlx::get_instance_server();
    if (server_obj_with_lock.container())
      server_obj_with_lock->delayed_start_tasks();

    return 0;
  }

  if (event_class == MYSQL_AUDIT_SERVER_SHUTDOWN_CLASS) {
    auto server_obj_with_lock = modules::Module_mysqlx::get_instance_server();

    if (server_obj_with_lock.container())
      server_obj_with_lock->gracefull_shutdown();
    return 0;
  }

  if (event_class == MYSQL_AUDIT_AUTHENTICATION_CLASS) {
    auto *authentication_event =
        reinterpret_cast<const struct mysql_event_authentication *>(event);

    mysql_event_authentication_subclass_t subclass =
        authentication_event->event_subclass;

    /*
      If status is set to true, it indicates an error.
      In which case, don't touch the cache.
    */
    if (authentication_event->status) return 0;

    auto sha256_password_cache =
        modules::Module_mysqlx::get_instance_sha256_password_cache();

    // Check if X Plugin was installed
    if (nullptr == sha256_password_cache.container()) return 0;

    if (subclass == MYSQL_AUDIT_AUTHENTICATION_FLUSH) {
      sha256_password_cache->clear();
      return 0;
    }

    if (subclass == MYSQL_AUDIT_AUTHENTICATION_CREDENTIAL_CHANGE ||
        subclass == MYSQL_AUDIT_AUTHENTICATION_AUTHID_RENAME ||
        subclass == MYSQL_AUDIT_AUTHENTICATION_AUTHID_DROP) {
#ifndef NDEBUG
      // "user" variable is going to be unused when the NDEBUG is defined
      auto user = authentication_event->user;
      assert(user.str[user.length] == '\0');
#endif  // NDEBUG
      sha256_password_cache->remove(authentication_event->user.str,
                                    authentication_event->host.str);
    }
  }
  return 0;
}

/** st_mysql_audit for sha2_cache_cleaner plugin */
static struct st_mysql_audit sha2_cache_cleaner_plugin_descriptor = {
    MYSQL_AUDIT_INTERFACE_VERSION,   // interface version
    nullptr,                         // release_thd()
    audit_cache_clean_event_notify,  // event_notify()
    {0,                              // MYSQL_AUDIT_GENERAL_CLASS
     0,                              // MYSQL_AUDIT_CONNECTION_CLASS
     0,                              // MYSQL_AUDIT_PARSE_CLASS
     0,                              // MYSQL_AUDIT_AUTHORIZATION_CLASS
     0,                              // MYSQL_AUDIT_TABLE_ACCESS_CLASS
     0,                              // MYSQL_AUDIT_GLOBAL_VARIABLE_CLASS
     // NOLINTNEXTLINE(runtime/int)
     static_cast<unsigned long>(MYSQL_AUDIT_SERVER_STARTUP_STARTUP),
     // NOLINTNEXTLINE(runtime/int)
     static_cast<unsigned long>(MYSQL_AUDIT_SERVER_SHUTDOWN_SHUTDOWN),
     0,  // MYSQL_AUDIT_COMMAND_CLASS
     0,  // MYSQL_AUDIT_QUERY_CLASS
     0,  // MYSQL_AUDIT_STORED_PROGRAM_CLASS
     // NOLINTNEXTLINE(runtime/int)
     static_cast<unsigned long>(MYSQL_AUDIT_AUTHENTICATION_ALL)}};

int Module_cache::initialize(MYSQL_PLUGIN p) {
  // If cache cleaner plugin is initialized before the X plugin we set this
  // flag so that we can enable cache when starting X plugin afterwards
  m_is_sha256_password_cache_enabled = true;
  auto sha256_password_cache =
      modules::Module_mysqlx::get_instance_sha256_password_cache();
  if (sha256_password_cache.container()) sha256_password_cache->enable();
  return 0;
}

int Module_cache::deinitialize(MYSQL_PLUGIN p) {
  m_is_sha256_password_cache_enabled = false;
  auto sha256_password_cache =
      modules::Module_mysqlx::get_instance_sha256_password_cache();
  if (sha256_password_cache.container()) sha256_password_cache->disable();
  return 0;
}

struct st_mysql_audit *Module_cache::get_audit_plugin_descriptor() {
  return &sha2_cache_cleaner_plugin_descriptor;
}

}  // namespace modules
