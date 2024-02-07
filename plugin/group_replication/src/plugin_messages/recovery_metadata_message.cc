/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/plugin_messages/recovery_metadata_message.h"
#include "plugin/group_replication/generated/protobuf_lite/replication_group_recovery_metadata.pb.h"
#include "plugin/group_replication/include/certifier.h"
#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_handlers/metrics_handler.h"
#include "plugin/group_replication/include/plugin_handlers/recovery_metadata.h"

#include "mutex_lock.h"
#include "my_dbug.h"

Recovery_metadata_message::Recovery_metadata_message(
    const std::string &view_id, Recovery_metadata_message_payload_error error,
    GR_compress::enum_compression_type compression_type)
    : Plugin_gcs_message(CT_RECOVERY_METADATA_MESSAGE),
      m_encode_view_id(view_id),
      m_encode_metadata_message_error(error),
      m_encode_metadata_compression_type(compression_type),
      m_decoded_view_id_error(
          enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED,
          std::ref(m_decoded_view_id)),
      m_decoded_message_send_error(
          enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED,
          RECOVERY_METADATA_NO_ERROR),
      m_decoded_compression_type_error(
          enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED,
          GR_compress::enum_compression_type::ZSTD_COMPRESSION),
      m_decoded_group_gtid_executed_error(
          enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED,
          std::ref(m_decoded_group_gtid_executed)),
      m_decoded_certification_info_packet_count_error(
          enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED,
          0),
      m_decoded_certification_info_uncompressed_length_error(
          enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED, 0,
          0),
      m_decoded_compressed_certification_info_error(
          enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED,
          nullptr, 0),
      m_decode_metadata_buffer(nullptr),
      m_decode_is_metadata_buffer_local_copy(false),
      m_decode_metadata_end(nullptr),
      m_decode_metadata_length(0) {
  DBUG_EXECUTE_IF("group_replication_no_vcle_no_compression", {
    m_encode_metadata_compression_type =
        GR_compress::enum_compression_type::NO_COMPRESSION;
  });
  DBUG_EXECUTE_IF("group_replication_no_vcle_ztsd", {
    m_encode_metadata_compression_type =
        GR_compress::enum_compression_type::ZSTD_COMPRESSION;
  });
}

Recovery_metadata_message::Recovery_metadata_message(const uchar *buf,
                                                     size_t len)
    : Plugin_gcs_message(CT_RECOVERY_METADATA_MESSAGE),
      m_decoded_view_id_error(
          enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED,
          std::ref(m_decoded_view_id)),
      m_decoded_message_send_error(
          enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED,
          RECOVERY_METADATA_NO_ERROR),
      m_decoded_compression_type_error(
          enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED,
          GR_compress::enum_compression_type::ZSTD_COMPRESSION),
      m_decoded_group_gtid_executed_error(
          enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED,
          std::ref(m_decoded_group_gtid_executed)),
      m_decoded_certification_info_packet_count_error(
          enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED,
          0),
      m_decoded_certification_info_uncompressed_length_error(
          enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED, 0,
          0),
      m_decoded_compressed_certification_info_error(
          enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED,
          nullptr, 0),
      m_decode_metadata_buffer(nullptr),
      m_decode_is_metadata_buffer_local_copy(false),
      m_decode_metadata_end(nullptr),
      m_decode_metadata_length(0) {
  decode(buf, len);
}

Recovery_metadata_message::~Recovery_metadata_message() {
  delete_copy_of_recovery_metadata_payload();
  for (std::vector<GR_compress *>::iterator it =
           m_encode_compressor_list.begin();
       it != m_encode_compressor_list.end(); ++it) {
    delete (*it);
  }
  m_encode_compressor_list.clear();
}

// Decode functions
void Recovery_metadata_message::decode_payload(const unsigned char *buffer,
                                               const unsigned char *end) {
  if (buffer == nullptr || end == nullptr) {
    m_decode_metadata_buffer = nullptr;
    m_decode_metadata_end = nullptr;
    m_decode_is_metadata_buffer_local_copy = false;
    m_decode_metadata_length = 0;
    LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_METADATA_PAYLOAD_EMPTY);
    return;
  }

  m_decode_metadata_buffer = buffer;
  m_decode_is_metadata_buffer_local_copy = false;
  m_decode_metadata_end = end;
  m_decode_metadata_length =
      static_cast<size_t>(m_decode_metadata_end - m_decode_metadata_buffer);
}

void Recovery_metadata_message::set_decoded_message_error() {
  m_decoded_message_send_error.second = RECOVERY_METADATA_ERROR;
}

bool Recovery_metadata_message::save_copy_of_recovery_metadata_payload() {
  if (m_decode_metadata_buffer == nullptr || m_decode_metadata_length <= 0) {
    LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_METADATA_PAYLOAD_EMPTY);
    return true;
  }

  unsigned char *metadata_buffer_aux = (unsigned char *)my_malloc(
      key_recovery_metadata_message_buffer, m_decode_metadata_length, MYF(0));
  if (metadata_buffer_aux == nullptr) {
    LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_METADATA_MEMORY_ALLOC,
                 "saving recovery metadata message payload");
    return true;
  }

  memcpy(metadata_buffer_aux, m_decode_metadata_buffer,
         m_decode_metadata_length);
  m_decode_metadata_buffer = metadata_buffer_aux;
  m_decode_is_metadata_buffer_local_copy = true;
  m_decode_metadata_end = m_decode_metadata_buffer + m_decode_metadata_length;
  return false;
}

void Recovery_metadata_message::delete_copy_of_recovery_metadata_payload() {
  if (m_decode_is_metadata_buffer_local_copy &&
      m_decode_metadata_buffer != nullptr) {
    my_free(const_cast<unsigned char *>(m_decode_metadata_buffer));

    m_decode_metadata_buffer = nullptr;
    m_decode_is_metadata_buffer_local_copy = false;
    m_decode_metadata_end = nullptr;
    m_decode_metadata_length = 0;
  }
}

std::tuple<Recovery_metadata_message::enum_recovery_metadata_message_error,
           const unsigned char *, unsigned long long>
Recovery_metadata_message::decode_payload_type(
    int payload_type, const unsigned char *payload_start) const {
  DBUG_TRACE;

  Recovery_metadata_message::enum_recovery_metadata_message_error error{
      enum_recovery_metadata_message_error::RECOVERY_METADATA_MESSAGE_OK};

  if (m_decode_metadata_buffer != nullptr && m_decode_metadata_length > 0) {
    /*
      If payload_start is provided then start searching payload types from
      that position, otherwise start from initial position
      i.e. m_decode_metadata_buffer. The Recovery Metadata Message may contain
      multiple compressed certification info packets, so to retrieve next
      certification info packet, end position of last certification info packet
      is provided.
    */
    const unsigned char *slider;
    if (payload_start != nullptr) {
      slider = payload_start;
    } else {
      slider = m_decode_metadata_buffer;
    }
    const unsigned char *end =
        m_decode_metadata_buffer + m_decode_metadata_length;
    unsigned long long payload_item_length{0};

    if (!get_payload_item_type_raw_data(slider, end, payload_type, &slider,
                                        &payload_item_length)) {
      switch (payload_type) {
        case PIT_VIEW_ID:
        case PIT_RECOVERY_METADATA_MESSAGE_ERROR:
        case PIT_RECOVERY_METADATA_COMPRESSION_TYPE:
        case PIT_UNTIL_CONDITION_AFTER_GTIDS:
        case PIT_COMPRESSED_CERTIFICATION_INFO_PACKET_COUNT:
        case PIT_COMPRESSED_CERTIFICATION_INFO_UNCOMPRESSED_LENGTH:
        case PIT_COMPRESSED_CERTIFICATION_INFO_PAYLOAD: {
          return std::make_tuple(error, slider, payload_item_length);
        } break;

        default: {
          // unkown payload type.
          return std::make_tuple(
              enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_UNKOWN,
              slider, 0);
        } break;
      }
    }

    // payload decoding error
    LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_METADATA_PAYLOAD_DECODING);
    return std::make_tuple(
        enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_DECODING,
        nullptr, 0);
  }

  // payload empty or payload length is 0.
  LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_METADATA_PAYLOAD_EMPTY);
  return std::make_tuple(
      enum_recovery_metadata_message_error::ERR_PAYLOAD_BUFFER_EMPTY, nullptr,
      0);
}

uint64_t Recovery_metadata_message::get_sent_timestamp(
    const unsigned char *buffer, size_t length) {
  DBUG_TRACE;
  return Plugin_gcs_message::get_sent_timestamp(buffer, length,
                                                PIT_SENT_TIMESTAMP);
}

std::pair<Recovery_metadata_message::enum_recovery_metadata_message_error,
          std::reference_wrapper<std::string>>
Recovery_metadata_message::get_decoded_view_id() {
  // 1. If View ID already decoded, return saved value
  if (m_decoded_view_id_error.first !=
      enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED) {
    return m_decoded_view_id_error;
  }

  // 2. If View ID not already decoded, decode and return saved value
  std::tuple<Recovery_metadata_message::enum_recovery_metadata_message_error,
             const unsigned char *, unsigned long long>
      payload_data = decode_payload_type(enum_payload_item_type::PIT_VIEW_ID);
  // make View ID empty
  m_decoded_view_id_error.second.get().clear();
  // set returned error
  m_decoded_view_id_error.first = std::get<0>(payload_data);

  // 3. Set m_decoded_view_id_error if no error
  if (m_decoded_view_id_error.first ==
      enum_recovery_metadata_message_error::RECOVERY_METADATA_MESSAGE_OK) {
    const unsigned char *slider = std::get<1>(payload_data);
    unsigned long long payload_item_length = std::get<2>(payload_data);

    if (payload_item_length > 0)
      m_decoded_view_id_error.second.get().assign(slider,
                                                  slider + payload_item_length);

    if (m_decoded_view_id_error.second.get().empty()) {
      m_decoded_view_id_error.first =
          enum_recovery_metadata_message_error::ERR_CERT_INFO_EMPTY;
      LogPluginErr(ERROR_LEVEL,
                   ER_GROUP_REPLICATION_METADATA_MESSAGE_PAYLOAD_EMPTY,
                   m_payload_item_type_string[PIT_VIEW_ID].c_str());
    }
  }

  return m_decoded_view_id_error;
}

std::pair<Recovery_metadata_message::enum_recovery_metadata_message_error,
          Recovery_metadata_message::Recovery_metadata_message_payload_error>
Recovery_metadata_message::get_decoded_message_error() {
  // 1. If Message Error already decoded, return saved value
  if (m_decoded_message_send_error.first !=
      enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED) {
    return m_decoded_message_send_error;
  }

  // 2. If Message Error not already decoded, decode and return saved value
  std::tuple<Recovery_metadata_message::enum_recovery_metadata_message_error,
             const unsigned char *, unsigned long long>
      payload_data = decode_payload_type(
          enum_payload_item_type::PIT_RECOVERY_METADATA_MESSAGE_ERROR);

  // set returned error
  m_decoded_message_send_error.first = std::get<0>(payload_data);

  // 3. Set m_decoded_message_send_error if no error
  if (m_decoded_message_send_error.first ==
      enum_recovery_metadata_message_error::RECOVERY_METADATA_MESSAGE_OK) {
    const unsigned char *slider = std::get<1>(payload_data);
    uint16 metadata_message_error_aux = uint2korr(slider);
    m_decoded_message_send_error.second =
        static_cast<Recovery_metadata_message_payload_error>(
            metadata_message_error_aux);
  }

  return m_decoded_message_send_error;
}

std::pair<Recovery_metadata_message::enum_recovery_metadata_message_error,
          GR_compress::enum_compression_type>
Recovery_metadata_message::get_decoded_compression_type() {
  // 1. If Compression type already decoded, return saved value.
  if (m_decoded_compression_type_error.first !=
      enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED) {
    return m_decoded_compression_type_error;
  }

  // 2. If Compression type not already decoded, decode and return saved value.
  std::tuple<Recovery_metadata_message::enum_recovery_metadata_message_error,
             const unsigned char *, unsigned long long>
      payload_data = decode_payload_type(
          enum_payload_item_type::PIT_RECOVERY_METADATA_COMPRESSION_TYPE);

  // set returned error.
  m_decoded_compression_type_error.first = std::get<0>(payload_data);

  // 3. Set m_decoded_compression_type_error if no error.
  if (m_decoded_compression_type_error.first ==
      enum_recovery_metadata_message_error::RECOVERY_METADATA_MESSAGE_OK) {
    const unsigned char *slider = std::get<1>(payload_data);
    uint16 metadata_compression_type_aux = uint2korr(slider);
    m_decoded_compression_type_error.second =
        static_cast<GR_compress::enum_compression_type>(
            metadata_compression_type_aux);
  }

  return m_decoded_compression_type_error;
}

std::pair<Recovery_metadata_message::enum_recovery_metadata_message_error,
          std::reference_wrapper<std::string>>
Recovery_metadata_message::get_decoded_group_gtid_executed() {
  // 1. If View ID already decoded, return saved value
  if (m_decoded_group_gtid_executed_error.first !=
      enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED) {
    return m_decoded_group_gtid_executed_error;
  }

  // 2. If View ID not already decoded, decode and return saved value
  std::tuple<Recovery_metadata_message::enum_recovery_metadata_message_error,
             const unsigned char *, unsigned long long>
      payload_data = decode_payload_type(
          enum_payload_item_type::PIT_UNTIL_CONDITION_AFTER_GTIDS);

  // make m_decoded_group_gtid_executed_error.second empty
  m_decoded_group_gtid_executed_error.second.get().clear();
  // set returned error
  m_decoded_group_gtid_executed_error.first = std::get<0>(payload_data);

  // 3. Set m_decoded_group_gtid_executed_error if no error
  if (m_decoded_group_gtid_executed_error.first ==
      enum_recovery_metadata_message_error::RECOVERY_METADATA_MESSAGE_OK) {
    const unsigned char *slider = std::get<1>(payload_data);
    unsigned long long payload_item_length = std::get<2>(payload_data);

    Tsid_map gtid_executed_tsid_map(nullptr);
    Gtid_set gtid_executed_set(&gtid_executed_tsid_map, nullptr);
    std::string gtid_executed_aux{};
    gtid_executed_aux.assign(slider, slider + payload_item_length);
    if (gtid_executed_set.add_gtid_encoding(
            reinterpret_cast<const uchar *>(gtid_executed_aux.c_str()),
            gtid_executed_aux.length()) != RETURN_STATUS_OK) {
      m_decoded_group_gtid_executed_error.first =
          enum_recovery_metadata_message_error::ERR_AFTER_GTID_SET_ENCODING;
      LogPluginErr(ERROR_LEVEL,
                   ER_GROUP_REPLICATION_METADATA_READ_GTID_EXECUTED);
    } else {
      char *gtid_executed_string = nullptr;
      gtid_executed_set.to_string(&gtid_executed_string, true);
      m_decoded_group_gtid_executed_error.second.get().assign(
          gtid_executed_string);
      my_free(gtid_executed_string);
      if (m_decoded_group_gtid_executed_error.second.get().empty()) {
        m_decoded_group_gtid_executed_error.first =
            enum_recovery_metadata_message_error::ERR_CERT_INFO_EMPTY;
        LogPluginErr(INFORMATION_LEVEL,
                     ER_GROUP_REPLICATION_METADATA_MESSAGE_PAYLOAD_EMPTY,
                     m_payload_item_type_string[PIT_UNTIL_CONDITION_AFTER_GTIDS]
                         .c_str());
      }
    }
  }

  return m_decoded_group_gtid_executed_error;
}

std::pair<Recovery_metadata_message::enum_recovery_metadata_message_error,
          unsigned int>
Recovery_metadata_message::
    get_decoded_compressed_certification_info_packet_count() {
  /*
    1. If Compressed Certification Info packet count already decoded,
       return saved value.
  */
  if (m_decoded_certification_info_packet_count_error.first !=
      enum_recovery_metadata_message_error::ERR_PAYLOAD_TYPE_NOT_DECODED) {
    return m_decoded_certification_info_packet_count_error;
  }

  /*
    2. If Compressed Certification Info packet count not already decoded,
       decode and return saved value.
  */
  std::tuple<Recovery_metadata_message::enum_recovery_metadata_message_error,
             const unsigned char *, unsigned long long>
      payload_data = decode_payload_type(
          enum_payload_item_type::
              PIT_COMPRESSED_CERTIFICATION_INFO_PACKET_COUNT);

  // set Compressed Certification Info packet count to 0.
  m_decoded_certification_info_packet_count_error.second = 0;
  // set returned error.
  m_decoded_certification_info_packet_count_error.first =
      std::get<0>(payload_data);

  // 3. Set m_decoded_certification_info_packet_count_error, if no error.
  if (m_decoded_certification_info_packet_count_error.first ==
      enum_recovery_metadata_message_error::RECOVERY_METADATA_MESSAGE_OK) {
    const unsigned char *slider = std::get<1>(payload_data);
    m_decoded_certification_info_packet_count_error.second = uint4korr(slider);
    if (m_decoded_certification_info_packet_count_error.second == 0) {
      m_decoded_certification_info_packet_count_error.first =
          enum_recovery_metadata_message_error::ERR_CERT_INFO_EMPTY;
      LogPluginErr(INFORMATION_LEVEL,
                   ER_GROUP_REPLICATION_METADATA_MESSAGE_PAYLOAD_EMPTY,
                   m_payload_item_type_string
                       [PIT_COMPRESSED_CERTIFICATION_INFO_PACKET_COUNT]
                           .c_str());
    }
  }

  return m_decoded_certification_info_packet_count_error;
}

std::tuple<Recovery_metadata_message::enum_recovery_metadata_message_error,
           unsigned long long, unsigned long long>
Recovery_metadata_message::
    get_decoded_compressed_certification_info_uncompressed_length(
        const unsigned char *payload_start_pos) {
  /*
    1. Decode and return value.
       The Recovery metadata message can have multiple compressed
       certification info packets. This function will not return saved value
       but will search for
       PIT_COMPRESSED_CERTIFICATION_INFO_UNCOMPRESSED_LENGTH after given
       position.
  */
  std::tuple<Recovery_metadata_message::enum_recovery_metadata_message_error,
             const unsigned char *, unsigned long long>
      payload_data = decode_payload_type(
          enum_payload_item_type::
              PIT_COMPRESSED_CERTIFICATION_INFO_UNCOMPRESSED_LENGTH,
          payload_start_pos);

  // set returned error
  std::get<0>(m_decoded_certification_info_uncompressed_length_error) =
      std::get<0>(payload_data);
  // set payload i.e. uncompressed_length to 0.
  std::get<1>(m_decoded_certification_info_uncompressed_length_error) = 0;
  // set payload length to 0.
  std::get<2>(m_decoded_certification_info_uncompressed_length_error) = 0;

  /*
    2. Set m_decoded_certification_info_uncompressed_length_error,
       if no error.
  */
  if (std::get<0>(m_decoded_certification_info_uncompressed_length_error) ==
      enum_recovery_metadata_message_error::RECOVERY_METADATA_MESSAGE_OK) {
    const unsigned char *slider = std::get<1>(payload_data);
    uint64 uncompressed_length_aux = uint8korr(slider);
    std::get<1>(m_decoded_certification_info_uncompressed_length_error) =
        (unsigned long long)uncompressed_length_aux;
    std::get<2>(m_decoded_certification_info_uncompressed_length_error) =
        (unsigned long long)std::get<2>(payload_data);
  }

  return m_decoded_certification_info_uncompressed_length_error;
}

std::tuple<Recovery_metadata_message::enum_recovery_metadata_message_error,
           const unsigned char *, unsigned long long>
Recovery_metadata_message::get_decoded_compressed_certification_info_payload(
    const unsigned char *payload_start_pos) {
  /*
    1. Decode and return value.
       The Recovery metadata message can have multiple compressed
       certification info packets. This function will not return saved value
       but will search for PIT_COMPRESSED_CERTIFICATION_INFO_PAYLOAD after
       given position.
  */
  std::tuple<Recovery_metadata_message::enum_recovery_metadata_message_error,
             const unsigned char *, unsigned long long>
      payload_data = decode_payload_type(
          enum_payload_item_type::PIT_COMPRESSED_CERTIFICATION_INFO_PAYLOAD,
          payload_start_pos);

  // set returned error
  std::get<0>(m_decoded_compressed_certification_info_error) =
      std::get<0>(payload_data);
  // set payload length to 0.
  std::get<2>(m_decoded_compressed_certification_info_error) = 0;

  // 3. Set m_decoded_compressed_certification_info_error, if no error.
  if (std::get<0>(m_decoded_compressed_certification_info_error) ==
      enum_recovery_metadata_message_error::RECOVERY_METADATA_MESSAGE_OK) {
    std::get<1>(m_decoded_compressed_certification_info_error) =
        std::get<1>(payload_data);
    std::get<2>(m_decoded_compressed_certification_info_error) =
        (unsigned long long)std::get<2>(payload_data);
  }

  return m_decoded_compressed_certification_info_error;
}

// Encode functions
void Recovery_metadata_message::encode_payload(
    std::vector<unsigned char> *buffer) const {
  DBUG_TRACE;
  bool error{false};

  // view id
  encode_payload_item_string(buffer, PIT_VIEW_ID, m_encode_view_id.c_str(),
                             m_encode_view_id.length());

  /* No need to send any more data if ERROR is set */
  if (m_encode_metadata_message_error == RECOVERY_METADATA_NO_ERROR) {
    // Metadata compression type
    uint16 metadata_compression_type_aux =
        static_cast<uint16>(m_encode_metadata_compression_type);
    encode_payload_item_int2(buffer, PIT_RECOVERY_METADATA_COMPRESSION_TYPE,
                             metadata_compression_type_aux);

    // after gtids
    encode_payload_item_string(buffer, PIT_UNTIL_CONDITION_AFTER_GTIDS,
                               m_encoded_group_gtid_executed.c_str(),
                               m_encoded_group_gtid_executed.length());

    // compressed certification info packet count
    encode_payload_item_int4(buffer,
                             PIT_COMPRESSED_CERTIFICATION_INFO_PACKET_COUNT,
                             (uint32)m_encode_compressor_list.size());

    // Compressed certification info
    if (encode_compressed_certification_info_payload(buffer)) {
      error = true;
      LogPluginErr(ERROR_LEVEL,
                   ER_GROUP_REPLICATION_METADATA_CERT_INFO_ENCODING_ERROR);
    }
  }

  // Metadata message error
  uint16 metadata_message_error_aux = (uint16)m_encode_metadata_message_error;
  if (error) {
    metadata_message_error_aux = (uint16)RECOVERY_METADATA_ERROR;
  }
  encode_payload_item_int2(buffer, PIT_RECOVERY_METADATA_MESSAGE_ERROR,
                           metadata_message_error_aux);

  // Sent timestamp
  encode_payload_item_int8(buffer, PIT_SENT_TIMESTAMP,
                           Metrics_handler::get_current_time());
}

bool Recovery_metadata_message::encode_compressed_certification_info_payload(
    std::vector<unsigned char> *buffer) const {
  bool error{false};

  // add payload data
  for (std::vector<GR_compress *>::iterator it =
           m_encode_compressor_list.begin();
       it != m_encode_compressor_list.end(); ++it) {
    unsigned char *payload_value{nullptr};
    std::size_t payload_length{0};

    std::tie(payload_value, payload_length) = (*it)->allocate_and_get_buffer();
    if (payload_value == nullptr || payload_length == 0) {
      error = true;
      break;
    }

    // add compressed certification info packet.
    encode_payload_item_bytes(buffer, PIT_COMPRESSED_CERTIFICATION_INFO_PAYLOAD,
                              payload_value, payload_length);

    // add uncompressed length of certification info packet.
    encode_payload_item_int8(
        buffer, PIT_COMPRESSED_CERTIFICATION_INFO_UNCOMPRESSED_LENGTH,
        (uint64)(*it)->get_uncompressed_data_size());

    my_free(payload_value);
  }

  // delete list of compressed certification info parts
  for (std::vector<GR_compress *>::iterator it =
           m_encode_compressor_list.begin();
       it != m_encode_compressor_list.end(); ++it) {
    delete (*it);
  }
  m_encode_compressor_list.clear();

  return error;
}

std::string &Recovery_metadata_message::get_encode_view_id() {
  return m_encode_view_id;
}

Recovery_metadata_message::Recovery_metadata_message_payload_error
Recovery_metadata_message::get_encode_message_error() {
  return m_encode_metadata_message_error;
}

GR_compress::enum_compression_type
Recovery_metadata_message::get_encode_compression_type() {
  return m_encode_metadata_compression_type;
}

std::string &Recovery_metadata_message::get_encode_group_gtid_executed() {
  return m_encoded_group_gtid_executed;
}

std::vector<GR_compress *>
    &Recovery_metadata_message::get_encode_compressor_list() {
  return m_encode_compressor_list;
}

void Recovery_metadata_message::set_encode_message_error() {
  m_encode_metadata_message_error = RECOVERY_METADATA_ERROR;
}

void Recovery_metadata_message::set_joining_members(
    std::vector<Gcs_member_identifier> &joining_members) {
  m_members_joined_in_view = std::move(joining_members);
}

void Recovery_metadata_message::set_valid_metadata_senders(
    std::vector<Gcs_member_identifier> &online_members) {
  m_valid_metadata_senders = std::move(online_members);
  sort_valid_metadata_sender_list_using_uuid();
}

void Recovery_metadata_message::sort_valid_metadata_sender_list_using_uuid() {
  struct {
    bool operator()(Gcs_member_identifier &a, Gcs_member_identifier &b) {
      std::pair<bool, std::string> first_member =
          group_member_mgr->get_group_member_uuid_from_member_id(a);
      std::pair<bool, std::string> second_member =
          group_member_mgr->get_group_member_uuid_from_member_id(b);
      if (!first_member.first && !second_member.first)
        return first_member.second < second_member.second;
      return true;  // no need to change data
    }
  } sort_using_member_id;
  std::sort(m_valid_metadata_senders.begin(), m_valid_metadata_senders.end(),
            sort_using_member_id);
}

std::pair<bool, Gcs_member_identifier>
Recovery_metadata_message::compute_and_get_current_metadata_sender() {
  bool status{false};
  if (m_valid_metadata_senders.size()) {
    m_member_id_sending_metadata = m_valid_metadata_senders[0];
  } else {
    m_member_id_sending_metadata = Gcs_member_identifier("");
    status = true;
  }
  return std::make_pair(status, m_member_id_sending_metadata);
}

bool Recovery_metadata_message::am_i_recovery_metadata_sender() {
  return (local_member_info->get_gcs_member_id() ==
          m_member_id_sending_metadata);
}

bool Recovery_metadata_message::donor_left() {
  return (std::find(
              m_valid_metadata_senders.begin(), m_valid_metadata_senders.end(),
              m_member_id_sending_metadata) == m_valid_metadata_senders.end());
}

void Recovery_metadata_message::delete_members_left(
    std::vector<Gcs_member_identifier> &member_left) {
  for (auto it : member_left) {
    m_members_joined_in_view.erase(
        std::remove(m_members_joined_in_view.begin(),
                    m_members_joined_in_view.end(), it),
        m_members_joined_in_view.end());
    m_valid_metadata_senders.erase(
        std::remove(m_valid_metadata_senders.begin(),
                    m_valid_metadata_senders.end(), it),
        m_valid_metadata_senders.end());
  }
  DBUG_EXECUTE_IF(
      "group_replication_recovery_metadata_message_member_is_being_deleted", {
        const char act[] =
            "now signal "
            "signal.group_replication_recovery_metadata_message_member_is_"
            "being_deleted_reached";
        assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
      });
}

bool Recovery_metadata_message::is_joiner_or_valid_sender_list_empty() {
  return m_members_joined_in_view.empty() || m_valid_metadata_senders.empty();
}
