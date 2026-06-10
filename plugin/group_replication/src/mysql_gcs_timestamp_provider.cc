/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/mysql_gcs_timestamp_provider.h"
#include <mysql/components/services/log_shared.h>  // iso8601_size
#include <mysql/components/services/mysql_timestamp.h>

extern SERVICE_TYPE_NO_CONST(mysql_timestamp) * mysql_timestamp_service;

void Gr_clock_timestamp_provider::get_timestamp_as_c_string(char *buffer,
                                                            size_t *size) {
  *size = mysql_timestamp_service->make_iso8601_timestamp_now(buffer, *size);
}

void Gr_clock_timestamp_provider::get_timestamp_as_string(std::string &out) {
  char buffer[iso8601_size];
  size_t n = iso8601_size;
  get_timestamp_as_c_string(buffer, &n);
  out.assign(buffer, n);
}
