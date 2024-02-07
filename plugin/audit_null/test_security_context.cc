/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#include <mysql/plugin.h>
#include <mysql/plugin_audit.h>
#include <mysql/service_my_plugin_log.h>
#include <mysql/service_security_context.h>
#include <stdio.h>
#include <string.h>

static MYSQL_THDVAR_STR(get_field, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
                        "Get specified security context field.", nullptr,
                        nullptr, nullptr);

static MYSQL_THDVAR_STR(get_value, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
                        "Get specified security context field value.", nullptr,
                        nullptr, nullptr);

/**
  Tests the security context service

  Do not run this in multiple concurrent threads !

  @param thd           the thread to work with
  @param event_class   audit event class
  @param event         event data
*/
static int test_security_context_notify(MYSQL_THD thd,
                                        mysql_event_class_t event_class,
                                        const void *event) {
  if (event_class != MYSQL_AUDIT_COMMAND_CLASS) {
    return 0;
  }

  const struct mysql_event_command *event_command =
      (const struct mysql_event_command *)event;

  if (event_command->command_id != COM_STMT_PREPARE &&
      event_command->command_id != COM_QUERY) {
    return 0;
  }

  int result = 0;
  const char *get_field = (const char *)THDVAR(thd, get_field);
  const char *get_value = (const char *)THDVAR(thd, get_value);
  MYSQL_LEX_CSTRING field_value = {nullptr, 0};
  MYSQL_SECURITY_CONTEXT orig_thd_ctx;
  MYSQL_SECURITY_CONTEXT new_thd_ctx;

  if (get_field == nullptr) {
    return 0;
  }

  if (thd_get_security_context(thd, &orig_thd_ctx)) {
    result = 1;
  }

  if (result == 0 && strcmp(get_field, "sec_ctx_test") == 0) {
    result = 1;

    /* Security Context Creation. */
    if (!security_context_create(&new_thd_ctx)) {
      if (!security_context_destroy(new_thd_ctx)) {
        if (!security_context_copy(orig_thd_ctx, &new_thd_ctx)) {
          if (!security_context_destroy(new_thd_ctx)) result = 0;
        }
      }
    }

    THDVAR(thd, get_field) = nullptr;
    THDVAR(thd, get_value) = nullptr;

    return result;
  }

  if (result == 0 &&
      security_context_get_option(orig_thd_ctx, get_field, &field_value)) {
    result = 1;
  }

  if (result == 0 && get_value != nullptr &&
      strcmp(field_value.str, get_value) != 0) {
    result = 1;
  }

  THDVAR(thd, get_field) = nullptr;
  THDVAR(thd, get_value) = nullptr;

  return result;
}

/** Plugin descriptor structure */

static struct st_mysql_audit test_security_context_descriptor = {
    MYSQL_AUDIT_INTERFACE_VERSION, /* interface version    */
    nullptr,                       /* release_thd function */
    test_security_context_notify,  /* notify function      */
    {
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        (unsigned long)MYSQL_AUDIT_COMMAND_START,
    } /* class mask           */
};

static SYS_VAR *system_variables[] = {

    MYSQL_SYSVAR(get_field), MYSQL_SYSVAR(get_value), nullptr};

/** Plugin declaration */

mysql_declare_plugin(test_security_context){
    MYSQL_AUDIT_PLUGIN,                /* type                            */
    &test_security_context_descriptor, /* descriptor                      */
    "test_security_context",           /* name                            */
    PLUGIN_AUTHOR_ORACLE,              /* author                          */
    "Test security context service",   /* description                     */
    PLUGIN_LICENSE_GPL,
    nullptr,          /* init function (when loaded)     */
    nullptr,          /* check uninstall function        */
    nullptr,          /* deinit function (when unloaded) */
    0x0001,           /* version                         */
    nullptr,          /* status variables                */
    system_variables, /* system variables                */
    nullptr,
    0,
} mysql_declare_plugin_end;
