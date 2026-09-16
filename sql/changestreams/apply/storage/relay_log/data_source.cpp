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

#include "sql/changestreams/apply/storage/relay_log/data_source.h"

#include <cassert>

namespace mysql::csa {

std::atomic<std::size_t> Data_source::m_used_bytes = 0;

Data_source::Data_source(Data_source::Data_type &&data,
                         const std::string &file_name, std::size_t file_offset,
                         std::size_t file_length, bool ends_file)
    : m_data(std::move(data)),
      m_file_name(file_name),
      m_file_offset(file_offset),
      m_file_length(file_length),
      m_is_eof(ends_file) {
  m_used_bytes += m_data.size();
}

uint8_t *Data_source::data_raw() { return m_data.data(); }

const Data_source::Data_type &Data_source::data() const { return m_data; }

Data_source::~Data_source() { m_used_bytes -= m_data.size(); }

std::size_t Data_source::size() const { return m_data.size(); }

const std::string &Data_source::get_file_name() const { return m_file_name; }

std::size_t Data_source::get_file_length() const { return m_file_length; }

std::size_t Data_source::get_file_offset() const { return m_file_offset; }

bool Data_source::ends_file() const { return m_is_eof; }

}  // namespace mysql::csa
