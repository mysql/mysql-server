/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include <array>
#include <cstring>

#include "libchangestreams/include/mysql/cs/reader/binary/mysqlproto.h"
#include "my_byteorder.h"
#include "mysql/gtid/gtid.h"

namespace cs::reader::binary {

Mysql_protocol::Mysql_protocol(MYSQL *mysql, uint32_t server_id, uint32_t flags)
    : m_mysql{mysql}, m_server_id(server_id), m_flags(flags) {
  memset(&m_rpl_ctx, 0, sizeof(m_rpl_ctx));
  reset();
}

Mysql_protocol::~Mysql_protocol() {
  if (m_is_binlog_conn_open) {
    close(); /* purecov: inspected */
  }
}

void Mysql_protocol::reset() {
  m_rpl_ctx.file_name_length = 0;
  m_rpl_ctx.file_name = "";
  m_rpl_ctx.start_position = 4;
  m_rpl_ctx.server_id = m_server_id;
  m_rpl_ctx.flags = m_flags | MYSQL_RPL_SKIP_HEARTBEAT | MYSQL_RPL_GTID;
  m_rpl_ctx.gtid_set_encoded_size = 0;
  m_rpl_ctx.fix_gtid_set = nullptr;
  if (m_rpl_ctx.gtid_set_arg) free(m_rpl_ctx.gtid_set_arg);
  m_rpl_ctx.gtid_set_arg = nullptr;
  m_rpl_ctx.size = 0;
  m_rpl_ctx.buffer = nullptr;
}

bool Mysql_protocol::setup() {
  /*
   Make a notice to the server that this client is checksum-aware. It does
   not need the first fake Rotate necessary checksummed. That preference
   is specified below.
  */
  std::string query{
      "SET @master_binlog_checksum = 'NONE', @source_binlog_checksum = 'NONE'"};
  if (mysql_real_query(m_mysql, query.c_str(), query.size())) {
    return true; /* purecov: inspected */
  }
  return false;
}

uint64_t calculate_encoded_num_tsids_value(
    const mysql::gtid::Gtid_set &gtid_set,
    const mysql::gtid::Gtid_format &format) {
  uint64_t num_tsids = gtid_set.get_num_tsids();
  uint64_t format_encoded =
      static_cast<uint64_t>(mysql::utils::to_underlying(format));
  uint64_t value_encoded = num_tsids | (format_encoded << 56);
  if (format == mysql::gtid::Gtid_format::tagged) {
    value_encoded = (format_encoded << 56) | (num_tsids << 8) | format_encoded;
  }
  return value_encoded;
}

bool Mysql_protocol::encode_gtid_set_to_mysql_protocol(
    const mysql::gtid::Gtid_set &gtid_set, std::string &buffer) const {
  char tmp[8];
  char tsid_tmp[mysql::gtid::Tsid::get_max_encoded_length()];
  const auto &contents = gtid_set.get_gtid_set();

  auto format = gtid_set.get_gtid_set_format();

  if (contents.size() == 0) {
    return false;
  }

  // serialize number of tsids
  int8store(tmp, calculate_encoded_num_tsids_value(gtid_set, format));
  buffer.append(tmp, 8);

  // for every uuid, serialize it and its intervals
  for (auto const &[uuid, tag_map] : contents) {
    for (auto const &[tag, intervals] : tag_map) {
      mysql::gtid::Tsid tsid(uuid, tag);
      auto tsid_bytes =
          tsid.encode_tsid(reinterpret_cast<unsigned char *>(tsid_tmp), format);
      buffer.append(tsid_tmp, tsid_bytes);

      // serialize the number of intervals and append the intervals data
      int8store(tmp, intervals.size());
      buffer.append(tmp, 8);

      // for every interval serialize start, end
      for (auto const &intv : intervals) {
        int8store(tmp, intv.get_start());
        buffer.append(tmp, 8);

        int8store(tmp, intv.get_end() + 1);
        buffer.append(tmp, 8);
      }
    }
  }
  return false;
}

bool Mysql_protocol::open(std::shared_ptr<State> state) {
  if (m_mysql == nullptr) return true;
  if (state.get() == nullptr) {
    m_state = std::make_shared<State>();
  } else {
    m_state = state;
  }

  std::string serialized;
  encode_gtid_set_to_mysql_protocol(m_state->get_gtids(), serialized);
  void *ptr = malloc(serialized.size());
  std::memcpy(ptr, serialized.c_str(), serialized.size());
  m_rpl_ctx.gtid_set_arg = ptr;
  m_rpl_ctx.gtid_set_encoded_size = serialized.size();

  if (setup()) return true;

  if (mysql_binlog_open(m_mysql, &m_rpl_ctx)) return true;

  m_is_binlog_conn_open = true;
  return false;
}

bool Mysql_protocol::close() {
  if (m_is_binlog_conn_open) {
    mysql_binlog_close(m_mysql, &m_rpl_ctx);
    m_is_binlog_conn_open = false;
  }

  reset();
  return false;
}

bool Mysql_protocol::read(std::vector<uint8_t> &buffer) {
  if (mysql_binlog_fetch(m_mysql, &m_rpl_ctx)) return true;

  if (m_rpl_ctx.size == 0) {
    buffer.clear();
    return false;
  }

  auto char_buffer{m_rpl_ctx.buffer + 1};
  auto char_buffer_size{m_rpl_ctx.size - 1};

  // copy the receive buffer to the destination buffer
  buffer.assign(char_buffer, char_buffer + char_buffer_size);

  if (m_tracker.track_and_update(m_state, buffer)) {
    return true; /* purecov: inspected */
  }

  return false;
}

std::shared_ptr<State> Mysql_protocol::get_state() const { return m_state; }

MYSQL *Mysql_protocol::get_mysql_connection() const { return m_mysql; }

}  // namespace cs::reader::binary
