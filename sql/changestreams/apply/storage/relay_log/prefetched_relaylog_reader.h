// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_CSA_STORAGE_RELAY_LOG_PREFETCHED_RELAYLOG_READER_H
#define MYSQL_CSA_STORAGE_RELAY_LOG_PREFETCHED_RELAYLOG_READER_H

#include <atomic>
#include <cstring>
#include <memory>
#include <vector>

#include "sql/basic_istream.h"  // Basic_istream
#include "sql/binlog_reader.h"
#include "sql/changestreams/apply/storage/relay_log/prefetched_ifile.h"
#include "sql/log_event.h"  // Log_event

namespace mysql::csa {

using Prefetched_relaylog_reader =
    Basic_binlog_file_reader<Prefetched_ifile, Binlog_event_data_istream,
                             Binlog_event_object_istream,
                             Default_binlog_event_allocator>;

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_RELAY_LOG_PREFETCHED_RELAYLOG_READER_H
