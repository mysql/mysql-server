/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stage_lz4.h"

#include <lz4.h>
#include <string.h>
#include <cassert>
#include <limits>
#include <map>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/byteorder.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_internal_message.h"

Gcs_message_stage::stage_status Gcs_message_stage_lz4::skip_apply(
    uint64_t const &original_payload_size) const {
  /*
   Check if the packet really needs to be compressed.
   */
  if (original_payload_size < m_threshold) {
    return stage_status::skip;
  }

  /*
   Currently, this function can just compress packets smaller than
   Gcs_message_stage_lz4::max_input_compression(). Note that we are
   disregarding the header because only the palyload is compressed.
   */
  if (original_payload_size > Gcs_message_stage_lz4::max_input_compression()) {
    MYSQL_GCS_LOG_ERROR(
        "Gcs_packet's payload is too big. Only packets smaller than "
        << Gcs_message_stage_lz4::max_input_compression()
        << " bytes can "
           "be compressed. Payload size is "
        << original_payload_size << ".");
    return stage_status::abort;
  }

  return stage_status::apply;
}

std::unique_ptr<Gcs_stage_metadata> Gcs_message_stage_lz4::get_stage_header() {
  return std::unique_ptr<Gcs_stage_metadata>(new Gcs_empty_stage_metadata());
}

std::pair<bool, std::vector<Gcs_packet>>
Gcs_message_stage_lz4::apply_transformation(Gcs_packet &&packet) {
  bool constexpr ERROR = true;
  bool constexpr OK = false;
  auto result = std::make_pair(ERROR, std::vector<Gcs_packet>());
  char *new_payload_pointer = nullptr;
  int compressed_len = 0;
  std::vector<Gcs_packet> packets_out;

  /* Get the original payload information. */
  int original_payload_length = packet.get_payload_length();
  char const *original_payload_pointer =
      reinterpret_cast<char const *>(packet.get_payload_pointer());

  /* Get an upper-bound on the transformed payload size and create a packet big
     enough to hold it. */
  unsigned long long new_payload_length =
      LZ4_compressBound(original_payload_length);
  bool packet_ok;
  Gcs_packet new_packet;
  std::tie(packet_ok, new_packet) =
      Gcs_packet::make_from_existing_packet(packet, new_payload_length);
  if (!packet_ok) goto end;

  /* Compress the old payload into the new packet. */
  new_payload_pointer =
      reinterpret_cast<char *>(new_packet.get_payload_pointer());
  compressed_len =
      LZ4_compress_default(original_payload_pointer, new_payload_pointer,
                           original_payload_length, new_payload_length);
  MYSQL_GCS_LOG_TRACE("Compressing payload from size %llu to output %llu.",
                      static_cast<unsigned long long>(original_payload_length),
                      static_cast<unsigned long long>(compressed_len))

  /* Since the actual compressed payload size may be smaller than the estimate
     given by LZ4_compressBound, update the packet information accordingly. */
  new_packet.set_payload_length(compressed_len);

  packets_out.push_back(std::move(new_packet));
  result = std::make_pair(OK, std::move(packets_out));

end:
  return result;
}

std::pair<Gcs_pipeline_incoming_result, Gcs_packet>
Gcs_message_stage_lz4::revert_transformation(Gcs_packet &&packet) {
  auto &dynamic_header = packet.get_current_dynamic_header();
  auto result =
      std::make_pair(Gcs_pipeline_incoming_result::ERROR, Gcs_packet());
  char *new_payload_pointer = nullptr;
  int uncompressed_len = 0;

  /* Get the compressed payload information. */
  int original_payload_length = packet.get_payload_length();
  char const *original_payload_pointer =
      reinterpret_cast<char const *>(packet.get_payload_pointer());

  /*
   Create a packet big enough to hold the uncompressed payload.

   The size of the uncompressed payload is stored in the dynamic header, i.e.
   the payload size before the stage was applied.
   */
  unsigned long long expected_new_payload_length =
      dynamic_header.get_payload_length();
  bool packet_ok;
  Gcs_packet new_packet;
  std::tie(packet_ok, new_packet) = Gcs_packet::make_from_existing_packet(
      packet, expected_new_payload_length);
  if (!packet_ok) goto end;

  /* Decompress the payload into the new packet. */
  new_payload_pointer =
      reinterpret_cast<char *>(new_packet.get_payload_pointer());
  uncompressed_len =
      LZ4_decompress_safe(original_payload_pointer, new_payload_pointer,
                          original_payload_length, expected_new_payload_length);

  if (uncompressed_len < 0) {
    MYSQL_GCS_LOG_ERROR("Error decompressing payload from size "
                        << original_payload_length << " to "
                        << expected_new_payload_length);
    goto end;
  } else {
    MYSQL_GCS_LOG_TRACE(
        "Decompressing payload from size %llu to output %llu.",
        static_cast<unsigned long long>(original_payload_length),
        static_cast<unsigned long long>(uncompressed_len))

    assert(static_cast<unsigned long long>(uncompressed_len) ==
           expected_new_payload_length);
  }

  result = std::make_pair(Gcs_pipeline_incoming_result::OK_PACKET,
                          std::move(new_packet));
end:
  return result;
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
