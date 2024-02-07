/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_LOGGER_SUPPORTED_OPTIONS_INCLUDED
#define MYSQL_HARNESS_LOGGER_SUPPORTED_OPTIONS_INCLUDED

#include <array>

#include "mysql/harness/logging/logging.h"

static constexpr std::array<const char *, 4> logger_sink_supported_options = {
    {mysql_harness::logging::kConfigOptionLogFilename,
     mysql_harness::logging::kConfigOptionLogDestination,
     mysql_harness::logging::kConfigOptionLogLevel,
     mysql_harness::logging::kConfigOptionLogTimestampPrecision}};

static constexpr std::array<const char *, 5> logger_supported_options = {
    {"sinks", mysql_harness::logging::kConfigOptionLogFilename,
     mysql_harness::logging::kConfigOptionLogDestination,
     mysql_harness::logging::kConfigOptionLogLevel,
     mysql_harness::logging::kConfigOptionLogTimestampPrecision}};

#endif /* MYSQL_HARNESS_LOGGER_SUPPORTED_OPTIONS_INCLUDED */
