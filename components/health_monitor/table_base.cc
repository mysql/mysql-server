/* Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#include "table_base.h"
#include "health_monitor_option_usage.h"

using namespace hm;

int table_base::rnd_init(PSI_table_handle *handle, bool) {
  if (handle == nullptr) return PFS_HA_ERR_WRONG_COMMAND;
  auto *that = reinterpret_cast<table_base *>(handle);
  // Track table usage
  ++Option_Usage::usage_count;
  return that->rnd_init(false);
}

int table_base::rnd_next(PSI_table_handle *handle) {
  if (handle == nullptr) return PFS_HA_ERR_WRONG_COMMAND;
  auto *that = reinterpret_cast<table_base *>(handle);
  return that->rnd_next();
}

int table_base::rnd_pos(PSI_table_handle *handle) {
  if (handle == nullptr) return PFS_HA_ERR_WRONG_COMMAND;
  auto *that = reinterpret_cast<table_base *>(handle);
  return that->rnd_pos();
}

void table_base::reset_position(PSI_table_handle *handle) {
  if (handle == nullptr) return;
  auto *that = reinterpret_cast<table_base *>(handle);
  return that->reset_position();
}

int table_base::read_column_value(PSI_table_handle *handle, PSI_field *field,
                                  unsigned int index) {
  if (handle == nullptr) return PFS_HA_ERR_WRONG_COMMAND;
  auto *that = reinterpret_cast<table_base *>(handle);
  return that->read_column_value(field, index);
}

void table_base::close_table(PSI_table_handle *handle) {
  if (handle == nullptr) return;
  auto *that = reinterpret_cast<table_base *>(handle);
  that->close_table();
  delete that;
}
