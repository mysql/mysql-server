/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#ifndef GCS_MSG_HEADERS_H
#define GCS_MSG_HEADERS_H

#include <limits>
#include <memory>
#include <sstream>
#include <type_traits>
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"

/**
 The different stages that are currently available.

 Any time a new stage is created, the old stages must be added to this
 enumeration class as new stages as follows. Let us assume that these
 are the current stages:

   enum class Stage_code : unsigned int {
     ST_UNKNOWN = 0,

     ST_LZ4_V1 = 1,

     ST_LZ4_V2 = 2,

     ST_SPLIT_V2 = 3,

     ST_MAX_STAGES = 4
   };

 If a new stage is added meaning that the protocol has changed, the
 enumeration class should be changed as follows:

   enum class Stage_code : unsigned int {
     ST_UNKNOWN = 0,

     ST_LZ4_V1 = 1,

     ST_LZ4_V2 = 2,

     ST_SPLIT_V2 = 3,

     ST_LZ4_V3 = 4,

     ST_SPLIT_V3 = 5,

     ST_NEW_V3 = 6,

     ST_MAX_STAGES = 7
   };
 */
enum class Stage_code : unsigned int {
  /*
   This type should not be used anywhere.
   */
  ST_UNKNOWN = 0,

  /*
   This type represents the compression stage v1.
   */
  ST_LZ4_V1 = 1,

  /*
   This type represents the compression stage v2.
   */
  ST_LZ4_V2 = 2,

  /*
     This type represents the split stage v2.
   */
  ST_SPLIT_V2 = 3,

  /*
   This type represents the compression stage v2.
   */
  ST_LZ4_V3 = 4,

  /*
     This type represents the split stage v2.
   */
  ST_SPLIT_V3 = 5,

  /*
   No valid state codes can appear after this one. If a stage code is to
   be added, this value needs to be incremented and the lowest type code
   available be assigned to the new stage.
   */
  ST_MAX_STAGES = 6
};

/**
 The different cargo type codes.
*/
enum class Cargo_type : unsigned short {
  /*
   This type should not be used anywhere.
   */
  CT_UNKNOWN = 0,

  /*
   This cargo type is used for internal messaging related to stage
   exchanges.
   */
  CT_INTERNAL_STATE_EXCHANGE = 1,

  /*
   This cargo type is used for messages from the application.
   */
  CT_USER_DATA = 2,

  /*
   No valid type codes can appear after this one. If a type code is to
   be added, this value needs to be incremented and the lowest type code
   available be assigned to the new type.
  */
  CT_MAX = 3
};

/**
 This header is internal to the MySQL GCS library and contains metadata
 information about the message content.

 The on-the-wire representation of the fixed header is:

 +------------------+-----------+------------------------------------------+
 | field            | wire size | description                              |
 +==================+===========+==========================================+
 | used_version     |   2 bytes | protocol version in use by sender        |
 | max_version      |   2 bytes | max protocol version supported by sender |
 | fixed_hdr_len    |   2 bytes | length of the fixed header               |
 | message_len      |   8 bytes | length of the message                    |
 | dyn_hdr_len      |   4 bytes | length of the dynamic headers            |
 | cargo_type       |   2 bytes | the cargo type in the payload            |
 +------------------+-----------+------------------------------------------+

 Be aware that previously there was a single 4-byte version field.
 Its semantics were the same of the used_version field.

 Older nodes will continue to send messages that, from their point of view,
 contain the single version field.
 Older nodes only know protocol version 1.
 Messages sent by older nodes will be encoded as follows:

    used_version = 1 and max_version = 0

 This is due to two factors:

    1. The combined size of {used,max}_version is 4 bytes, which is the same
       size of the old version field.
    2. The fixed header is encoded in little endian. Therefore, the used_version
       (max_version) field will contain the least (most) significant 2 bytes of
       the old version field.

 This class takes care of messages from old nodes by decoding such messages as:

        used_version = 1 and max_version = 1
*/
class Gcs_internal_message_header {
 public:
  /**
   The used protocol version number length field.
   */
  static constexpr unsigned short WIRE_USED_VERSION_SIZE = 2;

  /**
   The maximum protocol version number length field.
   */
  static constexpr unsigned short WIRE_MAX_VERSION_SIZE = 2;

  /**
   The length of the combined version fields.
   */
  static constexpr unsigned short WIRE_VERSION_SIZE = 4;
  static_assert(WIRE_VERSION_SIZE ==
                    WIRE_USED_VERSION_SIZE + WIRE_MAX_VERSION_SIZE,
                "The two version fields must have a combined size of 4 bytes");

  /**
   On-the-wire size of the fixed header length field.
   */
  static constexpr unsigned short WIRE_HD_LEN_SIZE = 2;

  /**
   On-the-wire size of the message size field.
   */
  static constexpr unsigned short WIRE_TOTAL_LEN_SIZE = 8;

  /**
   On-the-wire size of the cargo type field.
   */
  static constexpr unsigned short WIRE_CARGO_TYPE_SIZE = 2;

  /**
   On-the-wire size of the dynamic headers length field.
   */
  static constexpr unsigned short WIRE_DYNAMIC_HDRS_LEN_SIZE = 4;

  /**
   On-the-wire offset of the dynamic headers length field.
   */
  static constexpr unsigned short WIRE_DYNAMIC_HDRS_LEN_OFFSET =
      WIRE_VERSION_SIZE + WIRE_HD_LEN_SIZE + WIRE_TOTAL_LEN_SIZE;

  /**
   On-the-wire offset of the message length field.
   */
  static constexpr unsigned short WIRE_MSG_LEN_OFFSET =
      WIRE_VERSION_SIZE + WIRE_HD_LEN_SIZE;

  /**
   On-the-wire size of the fixed header.
   */
  static constexpr unsigned short WIRE_TOTAL_FIXED_HEADER_SIZE =
      WIRE_VERSION_SIZE + WIRE_HD_LEN_SIZE + WIRE_TOTAL_LEN_SIZE +
      WIRE_DYNAMIC_HDRS_LEN_SIZE + WIRE_CARGO_TYPE_SIZE;

 private:
  /**
   The header instance used protocol version.
   */
  Gcs_protocol_version m_used_version{Gcs_protocol_version::UNKNOWN};
  static_assert(sizeof(decltype(m_used_version)) == WIRE_USED_VERSION_SIZE,
                "The m_used_version size does not match the storage capacity");

  /**
   The header instance maximum protocol version.
   */
  Gcs_protocol_version m_max_version{Gcs_protocol_version::UNKNOWN};
  static_assert(sizeof(decltype(m_max_version)) == WIRE_MAX_VERSION_SIZE,
                "The m_max_version size does not match the storage capacity");

  static_assert(
      sizeof(decltype(m_max_version)) + sizeof(decltype(m_used_version)) ==
          WIRE_VERSION_SIZE,
      "The m_{max,used}_version sizes does not match the storage capacity");

  /**
   The header instance length.
   */
  unsigned short m_fixed_header_len{WIRE_TOTAL_FIXED_HEADER_SIZE};
  static_assert(
      sizeof(decltype(m_fixed_header_len)) == WIRE_HD_LEN_SIZE,
      "The m_fixed_header_len size does not match the storage capacity");

  /**
   The payload length field.

   Note that we keep track of the total length indirectly and the storage
   capacity is determined by the payload length which is the dominant factor.
   */
  unsigned long long m_payload_len{0};
  static_assert(sizeof(decltype(m_payload_len)) <= WIRE_TOTAL_LEN_SIZE,
                "The m_payload_len size does not match the storage capacity");

  /**
   The length of the dynamic headers.
   */
  unsigned int m_dynamic_headers_len{0};
  static_assert(
      sizeof(decltype(m_dynamic_headers_len)) == WIRE_DYNAMIC_HDRS_LEN_SIZE,
      "The m_dynamic_headers_len size does not match the storage capacity");

  /**
   The cargo type code.
   */
  Cargo_type m_cargo_type{Cargo_type::CT_UNKNOWN};
  static_assert(sizeof(decltype(m_cargo_type)) == WIRE_CARGO_TYPE_SIZE,
                "The m_cargo_type size does not match the storage capacity");

 public:
  /**
   Default constructor which is the only one provided.
   */
  explicit Gcs_internal_message_header() noexcept = default;

  /**
   These constructors are to be used when move semantics may be needed.
   */
  Gcs_internal_message_header(Gcs_internal_message_header &&) noexcept =
      default;
  Gcs_internal_message_header &operator=(
      Gcs_internal_message_header &&) noexcept = default;

  /**
   These constructors are to be used when copy semantics may be needed.
   */
  Gcs_internal_message_header(const Gcs_internal_message_header &) noexcept =
      default;
  Gcs_internal_message_header &operator=(
      const Gcs_internal_message_header &) noexcept = default;

  /**
   @return the value of the maximum protocol version field.
   */
  Gcs_protocol_version get_maximum_version() const;

  /**
   @return the value of the used protocol version field.
   */
  Gcs_protocol_version get_used_version() const;

  /**
   Set the maximum protocol version.

   @param version Maximum protocol version.
   */
  void set_maximum_version(Gcs_protocol_version version);

  /**
   Set the current protocol version.

   @param version Current protocol version.
   */
  void set_used_version(Gcs_protocol_version version);

  /**
   @return the value of the header length field value.
   */
  constexpr unsigned short get_fixed_header_length() const {
    return m_fixed_header_len;
  }

  /**
   @return the cargo type.
   */
  Cargo_type get_cargo_type() const;

  /**
   Set the cargo type field value.

   @param type Cargo type to set.
   */
  void set_cargo_type(Cargo_type type);

  /**
   @return The dynamic headers length field value.
   */
  unsigned int get_dynamic_headers_length() const;

  /**
   Set the dynamic headers length field value.

   @param length The dynamic headers value.
   */
  void set_dynamic_headers_length(unsigned int length);

  /**
   Set the message length attribute value according to the payload length and
   the header length. Only the payload information is provided because the
   header length is fixed.

   @param length Payload length.
   */
  void set_payload_length(unsigned long long length);

  /**
   @return The message total length field value.
   */
  unsigned long long get_total_length() const;

  /**
   Decode the contents of the buffer and sets the field values according to
   the values decoded. The buffer MUST be encoded in little endian format.

   @param buffer The buffer to decode from.
   @return Length of decoded information in bytes.
   */
  unsigned long long decode(const unsigned char *buffer);

  /**
   Encode the contents of this instance into the buffer. The encoding SHALL
   be done in little endian format.

   @param buffer The buffer to encode to.
   @return Length of encoded information in bytes.
   */
  unsigned long long encode(unsigned char *buffer) const;

  /**
   Create a string representation of the fixed header to be logged.

   @param output Reference to the output stream where the string will be
   created.
   */
  void dump(std::ostringstream &output) const;

  static constexpr unsigned short calculate_length() {
    return WIRE_TOTAL_FIXED_HEADER_SIZE;
  }
};

/**
 This is a default header created per stage and contains information to
 decode it.

 The on-the-wire-layout representation of a dynamic header is:

 +------------------+-----------+--------------------------------------+
 | field            | wire size | description                          |
 +==================+===========+======================================+
 | dyn_hdr_len      |   2 bytes | length of the dynamic header         |
 | stage_code       |   4 bytes | stage code                           |
 | old_payload_len  |   8 bytes | length of the previous stage payload |
 +------------------+-----------+--------------------------------------+
*/
class Gcs_dynamic_header {
 private:
  /**
   Dynamic header length.
   */
  unsigned short m_dynamic_header_length{0};

  /**
   Stage that created the dynamic header.
   */
  Stage_code m_stage_code{Stage_code::ST_UNKNOWN};

  /**
   Payload length when the packet went through the stage.
   */
  unsigned long long m_payload_length{0};

 public:
  /**
   The offset of the header length within the stage header.
   */
  static constexpr unsigned short WIRE_HD_LEN_OFFSET = 0;

  /**
   On-the-wire field size for the stage type code.
   */
  static constexpr unsigned short WIRE_HD_LEN_SIZE = 2;

  /**
   The offset of the stage type code within the stage header.
   */
  static constexpr unsigned short WIRE_HD_TYPE_OFFSET = WIRE_HD_LEN_SIZE;

  /**
   On-the-wire field size for the stage type code.
   */
  static constexpr unsigned short WIRE_HD_TYPE_SIZE = 4;

  /**
   The offset of the payload length within the stage header.
   */
  static constexpr unsigned short WIRE_HD_PAYLOAD_LEN_OFFSET =
      WIRE_HD_TYPE_OFFSET + WIRE_HD_TYPE_SIZE;

  /**
   On-the-wire field size for the stage payload length.
   */
  static constexpr unsigned short WIRE_HD_PAYLOAD_LEN_SIZE = 8;

  /**
   Constructor for the dynamic header.

   @param stage_code Stage code.
   @param payload_length Payload length while the packet is in the stage.
   */
  explicit Gcs_dynamic_header(Stage_code stage_code,
                              unsigned long long payload_length) noexcept;

  /**
   Default constructor is to be used when creating an empty dynamic header.
   */
  explicit Gcs_dynamic_header() noexcept = default;

  /**
   These constructors are to be used when move semantics may be needed.
   */
  Gcs_dynamic_header(Gcs_dynamic_header &&) noexcept = default;
  Gcs_dynamic_header &operator=(Gcs_dynamic_header &&) noexcept = default;

  /**
   These constructors are to be used when copy semantics may be needed.
   */
  Gcs_dynamic_header(const Gcs_dynamic_header &) noexcept = default;
  Gcs_dynamic_header &operator=(const Gcs_dynamic_header &) noexcept = default;

  /**
   Return the dynamic header length.
   */
  unsigned short get_dynamic_header_length() const;

  /**
   Return the stage code.
   */
  Stage_code get_stage_code() const;

  /**
   Return the payload length the packet had before this stage was applied.
   */
  unsigned long long get_payload_length() const;

  /**
   Set the payload length the packet had before this stage was applied.

   @param new_length payload length before this stage
   */
  void set_payload_length(unsigned long long new_length);

  /**
   Decode the contents of the buffer and sets the field values according to
   the values decoded. The buffer MUST be encoded in little endian format.

   @param buffer The buffer to decode from.
   @return Length of decoded information in bytes.
   */
  unsigned long long decode(const unsigned char *buffer);

  /**
   Encode the contents of this instance into the buffer. The encoding SHALL
   be done in little endian format.

   @param buffer The buffer to encode to.
   @return Length of encoded information in bytes.
   */
  unsigned long long encode(unsigned char *buffer) const;

  /**
   Return length of dynamic header.
   */
  static constexpr unsigned long long calculate_length() {
    return WIRE_HD_LEN_SIZE + WIRE_HD_TYPE_SIZE + WIRE_HD_PAYLOAD_LEN_SIZE;
  }

  /**
   Create a string representation of the dynamic header to be logged.

   @param output Reference to the output stream where the string will be
   created.
   */
  void dump(std::ostringstream &output) const;
};

/**
 Abstract class that defines specific metadata associated to a stage if it
 decides to extend it.
 */
class Gcs_stage_metadata {
 public:
  Gcs_stage_metadata() = default;

  virtual ~Gcs_stage_metadata();

  Gcs_stage_metadata(const Gcs_stage_metadata &) = default;
  Gcs_stage_metadata &operator=(const Gcs_stage_metadata &) = default;

  Gcs_stage_metadata(Gcs_stage_metadata &&) = default;
  Gcs_stage_metadata &operator=(Gcs_stage_metadata &&) = default;

  /**
   Creates a deep copy of this object.

   @returns a deep copy of this object.
   */
  virtual std::unique_ptr<Gcs_stage_metadata> clone() = 0;

  /**
   Calculate the length required to encode this object.
   */
  virtual unsigned long long calculate_encode_length() const = 0;

  /**
   Encode the contents of this instance into the buffer. The encoding SHALL
   be done in little endian format.

   @param buffer The buffer to encode to.
   @return Length of the encoded information.
   */
  virtual unsigned long long encode(unsigned char *buffer) const = 0;

  /**
   Decode the contents of the buffer and sets the field values according to
   the values decoded. The buffer MUST be encoded in little endian format.

   @param buffer The buffer to decode from.
   @return Encoded size.
   */
  virtual unsigned long long decode(const unsigned char *buffer) = 0;

  /**
   Create a string representation of the header to be logged.

   @param output Reference to the output stream where the string will be created
   */
  virtual void dump(std::ostringstream &output) const = 0;
};

/**
 Empty metadata for stages that do not require any metadata.
 */
class Gcs_empty_stage_metadata final : public Gcs_stage_metadata {
 public:
  Gcs_empty_stage_metadata() = default;

  std::unique_ptr<Gcs_stage_metadata> clone() final;

  unsigned long long calculate_encode_length() const final;

  unsigned long long encode(unsigned char *) const final;

  unsigned long long decode(unsigned char const *) final;

  void dump(std::ostringstream &) const final;

  Gcs_empty_stage_metadata(Gcs_empty_stage_metadata &&) = default;
  Gcs_empty_stage_metadata &operator=(Gcs_empty_stage_metadata &&) = default;

  Gcs_empty_stage_metadata(Gcs_empty_stage_metadata const &) = default;
  Gcs_empty_stage_metadata &operator=(Gcs_empty_stage_metadata const &) =
      default;
};

#endif  // GCS_MSG_HEADERS_H
