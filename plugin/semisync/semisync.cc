/* Copyright (C) 2007 Google Inc.
   Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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

#include "plugin/semisync/semisync.h"
#include "mysql/components/services/component_sys_var_service.h"

const unsigned char ReplSemiSyncBase::kPacketMagicNum = 0xef;
const unsigned char ReplSemiSyncBase::kPacketFlagSync = 0x01;

const unsigned long Trace::kTraceGeneral = 0x0001;
const unsigned long Trace::kTraceDetail = 0x0010;
const unsigned long Trace::kTraceNetWait = 0x0020;
const unsigned long Trace::kTraceFunction = 0x0040;

const unsigned char ReplSemiSyncBase::kSyncHeader[2] = {
    ReplSemiSyncBase::kPacketMagicNum, 0};

bool is_sysvar_defined(const char *name) {
  char buffer[256];
  void *value = buffer;
  size_t value_length = sizeof(buffer) - 1;
  auto registry_handle = mysql_plugin_registry_acquire();
  assert(registry_handle != nullptr);
  my_service<SERVICE_TYPE(component_sys_variable_register)> svc(
      "component_sys_variable_register", registry_handle);
  // Returns true on error, i.e., if the variable does *not* exist.
  bool get_var_error =
      svc->get_variable("mysql_server", name, &value, &value_length);
  mysql_plugin_registry_release(registry_handle);
  return !get_var_error;
}
