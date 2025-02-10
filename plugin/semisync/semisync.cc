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
#include "mysql/components/services/mysql_system_variable.h"

const unsigned char ReplSemiSyncBase::kPacketMagicNum = 0xef;
const unsigned char ReplSemiSyncBase::kPacketFlagSync = 0x01;

const unsigned long Trace::kTraceGeneral = 0x0001;
const unsigned long Trace::kTraceDetail = 0x0010;
const unsigned long Trace::kTraceNetWait = 0x0020;
const unsigned long Trace::kTraceFunction = 0x0040;

const unsigned char ReplSemiSyncBase::kSyncHeader[2] = {
    ReplSemiSyncBase::kPacketMagicNum, 0};

/* clang-format off */
/**
  @page page_protocol_semisync Semisync

  Semisync is a set of two plugins, a server one and a client one.
  Both plugins enhance the replication protocol.

  @section sect_semisync_source Semisync source

  A source with semisync enabled will prepend binlog events with
  a semisync request.

  <table>
  <caption>Semisync Request</caption>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref a_protocol_type_int1 "int&lt;1&gt;"</td>
      <td>semisync marker</td>
      <td>Marker to indicate semisync</td></tr>
  <tr><td>@ref a_protocol_type_int1 "int&lt;1&gt;"</td>
      <td>semisync flag</td>
      <td>Set to 0x1 to request semisync acknowledgement, otherwise 0x0.</td></tr>
  </table>

  @section sect_semisync_replica Semisync replica

  A replica with semisync enabled will send acknowledgements to Semisync Requests

  <table>
  <caption>Semisync Request</caption>
  <tr><th>Type</th><th>Name</th><th>Description</th></tr>
  <tr><td>@ref a_protocol_type_int1 "int&lt;1&gt;"</td>
      <td>semisync marker</td>
      <td>Marker to indicate semisync</td></tr>
  <tr><td>@ref a_protocol_type_int8 "int&lt;8&gt;"</td>
      <td>Binlog Position</td>
      <td>Binlog position of the acknowledgement.</td></tr>
  <tr><td>@ref sect_protocol_basic_dt_string_eof "string[EOF]"</td>
      <td>Binlog File Name</td>
      <td>Binlog file name of the acknowledgement.</td></tr>
  </table>
*/

bool is_sysvar_defined(const char *name) {
  char buffer[256];
  void *value = buffer;
  size_t value_length = sizeof(buffer) - 1;
  auto registry_handle = mysql_plugin_registry_acquire();
  assert(registry_handle != nullptr);
  my_service<SERVICE_TYPE(mysql_system_variable_reader)> svc(
      "mysql_system_variable_reader", registry_handle);
  // Returns true on error, i.e., if the variable does *not* exist.
  bool get_var_error =
      svc->get(nullptr, "GLOBAL", "mysql_server", name, &value, &value_length);
  mysql_plugin_registry_release(registry_handle);
  return !get_var_error;
}
