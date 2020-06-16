/*
 * Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
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

#include "my_inttypes.h"  // NOLINT(build/include_subdir)

#include "plugin/x/src/interface/sha256_password_cache.h"
#include "plugin/x/src/module_mysqlx.h"
#include "plugin/x/src/sha256_password_cache.h"
#include "plugin/x/src/xpl_regex.h"

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
  DBUG_TRACE;

  switch (event_class) {
    case MYSQL_AUDIT_SERVER_STARTUP_CLASS: {
      auto server_obj_with_lock = modules::Module_mysqlx::get_instance_server();
      if (server_obj_with_lock.container()) server_obj_with_lock->start_tasks();
      return 0;
    }

    case MYSQL_AUDIT_SERVER_SHUTDOWN_CLASS: {
      auto server_obj_with_lock = modules::Module_mysqlx::get_instance_server();

      if (server_obj_with_lock.container()) server_obj_with_lock->stop(false);
      return 0;
    }

    case MYSQL_AUDIT_QUERY_CLASS: {
      auto *query_event =
          reinterpret_cast<const struct mysql_event_query *>(event);
      if (query_event->status) return 0;

      if (query_event->event_subclass == MYSQL_AUDIT_QUERY_STATUS_END &&
          query_event->sql_command_id == SQLCOM_ALTER_USER) {
        DBUG_PRINT("info", ("Query: %s", query_event->query.str));
        static const xpl::Regex re{
            "ALTER USER '(\\w+)'@'(\\w*)'.+"
            "(FAILED_LOGIN_ATTEMPTS|PASSWORD_LOCK_TIME|ACCOUNT UNLOCK).*"};
        xpl::Regex::Group_list groups;
        if (re.match_groups(query_event->query.str, &groups, false) &&
            groups.size() == 4) {
          auto locker =
              modules::Module_mysqlx::get_instance_temporary_account_locker();
          if (nullptr == locker.container()) return 0;
          locker->clear(groups[1], groups[2]);
        }
      }
      return 0;
    }

    case MYSQL_AUDIT_AUTHENTICATION_CLASS: {
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

      auto locker =
          modules::Module_mysqlx::get_instance_temporary_account_locker();
      if (nullptr == locker.container()) return 0;

#ifndef DBUG_OFF
      // "user" variable is going to be unused when the DBUG_OFF is defined
      auto user = authentication_event->user;
      DBUG_ASSERT(user.str[user.length] == '\0');
#endif  // DBUG_OFF

      switch (subclass) {
        case MYSQL_AUDIT_AUTHENTICATION_FLUSH: {
          sha256_password_cache->clear();
          locker->clear();
          return 0;
        }

        case MYSQL_AUDIT_AUTHENTICATION_CREDENTIAL_CHANGE: {
          sha256_password_cache->remove(authentication_event->user.str,
                                        authentication_event->host.str);
          return 0;
        }

        case MYSQL_AUDIT_AUTHENTICATION_AUTHID_RENAME:
        case MYSQL_AUDIT_AUTHENTICATION_AUTHID_DROP: {
          sha256_password_cache->remove(authentication_event->user.str,
                                        authentication_event->host.str);
          locker->clear(authentication_event->user.str,
                        authentication_event->host.str);
          return 0;
        }

        default:
          return 0;
      }

      return 0;
    }

    default:
      return 0;
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
     static_cast<ulong>(MYSQL_AUDIT_SERVER_STARTUP_STARTUP),
     static_cast<ulong>(MYSQL_AUDIT_SERVER_SHUTDOWN_SHUTDOWN),
     0,  // MYSQL_AUDIT_COMMAND_CLASS
     static_cast<ulong>(MYSQL_AUDIT_QUERY_STATUS_END),
     0,  // MYSQL_AUDIT_STORED_PROGRAM_CLASS
     static_cast<ulong>(MYSQL_AUDIT_AUTHENTICATION_ALL)}};

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
