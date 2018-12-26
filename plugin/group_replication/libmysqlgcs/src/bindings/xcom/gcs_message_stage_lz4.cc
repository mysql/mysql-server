/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stage_lz4.h"

#include <lz4.h>
#include <string.h>
#include <cassert>
#include <limits>
#include <map>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/byteorder.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_internal_message.h"

const unsigned long long Gcs_message_stage_lz4::DEFAULT_THRESHOLD;

unsigned long long Gcs_message_stage_lz4::max_input_compression() {
  /*
   The code expects that the following assumption will always hold.
   */
  static_assert(
      LZ4_MAX_INPUT_SIZE <= std::numeric_limits<int>::max(),
      "Maximum input size for lz compression exceeds the expected value");
  return LZ4_MAX_INPUT_SIZE;
}

Gcs_message_stage::stage_status Gcs_message_stage_lz4::skip_apply(
    const Gcs_packet &packet) const {
  /*
   Check if the packet really needs to be compressed.
   */
  if (packet.get_payload_length() < m_threshold) {
    return stage_status::skip;
  }

  /*
   Currently, this function can just compress packets smaller than
   Gcs_message_stage_lz4::max_input_compression(). Note that we are
   disregarding the header because only the palyload is compressed.
   */
  if (packet.get_payload_length() >
      Gcs_message_stage_lz4::max_input_compression()) {
    MYSQL_GCS_LOG_ERROR(
        "Gcs_packet's payload is too big. Only packets smaller than "
        << Gcs_message_stage_lz4::max_input_compression()
        << " bytes can "
           "be compressed. Payload size is "
        << packet.get_payload_length() << ".");
    return stage_status::abort;
  }

  return stage_status::apply;
}

unsigned long long Gcs_message_stage_lz4::calculate_payload_length(
    Gcs_packet &packet) const {
  return LZ4_compressBound(static_cast<int>(packet.get_payload_length()));
}

std::pair<bool, unsigned long long>
Gcs_message_stage_lz4::transform_payload_apply(
    unsigned int, unsigned char *new_payload_ptr,
    unsigned long long new_payload_length, unsigned char *old_payload_ptr,
    unsigned long long old_payload_length) {
  int compressed_len = LZ4_compress_default(
      reinterpret_cast<char *>(old_payload_ptr),
      reinterpret_cast<char *>(new_payload_ptr),
      static_cast<int>(old_payload_length), new_payload_length);

  return std::make_pair(false, static_cast<unsigned long long>(compressed_len));
}

Gcs_message_stage::stage_status Gcs_message_stage_lz4::skip_revert(
    const Gcs_packet &packet) const {
  /*
   If the payload's length is greater than the maximum allowed compressed
   information an error is returned.
   */
  if (packet.get_payload_length() >
      Gcs_message_stage_lz4::max_input_compression()) {
    MYSQL_GCS_LOG_ERROR(
        "Gcs_packet's payload is too big. Only packets smaller than "
        << Gcs_message_stage_lz4::max_input_compression()
        << " bytes can "
           "be uncompressed. Payload size is "
        << packet.get_payload_length() << ".");

    return stage_status::abort;
  }

  return stage_status::apply;
}

std::pair<bool, unsigned long long>
Gcs_message_stage_lz4::transform_payload_revert(
    unsigned int, unsigned char *new_payload_ptr,
    unsigned long long new_payload_length, unsigned char *old_payload_ptr,
    unsigned long long old_payload_length) {
  // Decompress to the payload
  int src_len = static_cast<int>(old_payload_length);
  int dest_len = static_cast<int>(new_payload_length);
  int uncompressed_len = LZ4_decompress_safe(
      reinterpret_cast<char *>(old_payload_ptr),
      reinterpret_cast<char *>(new_payload_ptr), src_len, dest_len);

  if (uncompressed_len < 0) {
    MYSQL_GCS_LOG_ERROR("Error decompressing payload of size "
                        << new_payload_length << ".");
    return std::make_pair(true, 0);
  }
  assert(static_cast<unsigned long long>(uncompressed_len) ==
         new_payload_length);

  return std::make_pair(false, new_payload_length);
}
