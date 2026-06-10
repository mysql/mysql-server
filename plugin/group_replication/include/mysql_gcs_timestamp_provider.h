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

#ifndef MYSQL_GCS_TIMESTAMP_PROVIDER_INCLUDED
#define MYSQL_GCS_TIMESTAMP_PROVIDER_INCLUDED

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging.h"

class Gr_clock_timestamp_provider : public Clock_timestamp_interface {
 public:
  Gr_clock_timestamp_provider() = default;
  ~Gr_clock_timestamp_provider() override = default;
  // non-copyable
  Gr_clock_timestamp_provider(const Gr_clock_timestamp_provider &other) =
      delete;
  Gr_clock_timestamp_provider &operator=(
      const Gr_clock_timestamp_provider &other) = delete;
  // non-movable
  Gr_clock_timestamp_provider(Gr_clock_timestamp_provider &&other) = delete;
  Gr_clock_timestamp_provider &operator=(Gr_clock_timestamp_provider &&other) =
      delete;

  enum_gcs_error initialize() override { return GCS_OK; }
  enum_gcs_error finalize() override { return GCS_OK; }

  void get_timestamp_as_c_string(char *buffer, size_t *size) override;
  void get_timestamp_as_string(std::string &str) override;
};

#endif /* MYSQL_GCS_TIMESTAMP_PROVIDER_INCLUDED */
