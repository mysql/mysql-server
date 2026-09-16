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

#include "sql/changestreams/apply/storage/relay_log/prefetched_ifile.h"

namespace mysql::csa {

std::unique_ptr<Basic_seekable_istream> Prefetched_ifile::open_file(
    const char *file_name) {
  assert(m_prefetcher);
  m_current_file = file_name;
  return std::unique_ptr<Basic_seekable_istream>(
      new Istream_prefetched(m_prefetcher, file_name));
}

void Prefetched_ifile::set_prefetcher(Log_prefetcher_sptr prefetcher) {
  m_prefetcher = prefetcher;
}

}  // namespace mysql::csa
