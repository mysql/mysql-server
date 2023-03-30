/* Copyright (c) 2014, 2023, Oracle and/or its affiliates.

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

#ifndef GCS_PLUGIN_MESSAGES_INCLUDED
#define GCS_PLUGIN_MESSAGES_INCLUDED

/*
  Since this file is used on unit tests, through member_info.h,
  includes must set here and not through plugin_server_include.h.
*/
#include <string>
#include <vector>

#include "my_inttypes.h"

/**
 This is the base GCS plugin message.
 It is composed by a fixed header and 1 or more payload items.

 The on-the-wire layout looks like this:

   +-----------------------------------+
   | fixed header |          payload   |
   +-----------------------------------+

 The on-the-wire representation of the message is:

  +-------------------+-----------+--------------------------------------+
  | field             | wire size | description                          |
  +===================+===========+======================================+
  | version           |   4 bytes | protocol version                     |
  | fixed_hdr_len     |   2 bytes | length of the fixed header           |
  | message_len       |   8 bytes | length of the message                |
  | cargo_type        |   2 bytes | the cargo type in the payload        |
  +-------------------+-----------+--------------------------------------+
  | payload_item_type |   2 bytes | the item type in the payload         |
  | payload_item_len  |   8 bytes | length of the payload item           |
  | payload_item      |   X bytes | payload item                         |
  +-------------------+-----------+--------------------------------------+

 The last tree lines can occur one or more times.
*/

class Plugin_gcs_message {
 public:
  /**
   The protocol version number.
   */
  static const int PLUGIN_GCS_MESSAGE_VERSION;

  /**
   The protocol version number.
   */
  static const unsigned int WIRE_VERSION_SIZE;

  /**
   The on-the-wire size of the header length field.
   */
  static const unsigned int WIRE_HD_LEN_SIZE;

  /**
   The on-the-wire size of the message size field.
   */
  static const unsigned int WIRE_MSG_LEN_SIZE;

  /**
   The on-the-wire size of the cargo type field.
   */
  static const unsigned int WIRE_CARGO_TYPE_SIZE;

  /**
   The on-the-wire size of the fixed header.
   */
  static const unsigned int WIRE_FIXED_HEADER_SIZE;

  /**
   The on-the-wire size of the each payload item type field.
   */
  static const unsigned int WIRE_PAYLOAD_ITEM_TYPE_SIZE;

  /**
   The on-the-wire size of the each payload item size field.
   */
  static const unsigned int WIRE_PAYLOAD_ITEM_LEN_SIZE;

  /**
   The on-the-wire size of the payload item header.
   */
  static const unsigned int WIRE_PAYLOAD_ITEM_HEADER_SIZE;

  /**
   The different cargo type codes.

   NOTE: all type values must fit into WIRE_CARGO_TYPE_SIZE bytes storage.
   */
  enum enum_cargo_type {
    // This type should not be used anywhere.
    CT_UNKNOWN = 0,

    // This cargo type is used for certification events, GTID_EXECUTED
    // broadcast.
    CT_CERTIFICATION_MESSAGE = 1,

    // This cargo type is used for transaction data.
    CT_TRANSACTION_MESSAGE = 2,

    // This cargo type is used for recovery events, signal when a given member
    // becomes online.
    CT_RECOVERY_MESSAGE = 3,

    // This cargo type is used for messaging related to stage exchanges,
    // on which it represents one member.
    CT_MEMBER_INFO_MESSAGE = 4,

    // This cargo type is used for messaging related to stage exchanges,
    // on which it represents a set of members.
    CT_MEMBER_INFO_MANAGER_MESSAGE = 5,

    // This cargo type is used for messaging related to members pipeline
    // stats.
    CT_PIPELINE_STATS_MEMBER_MESSAGE = 6,

    // This cargo type is used for messaging related to single primary
    // mode.
    CT_SINGLE_PRIMARY_MESSAGE = 7,

    // This cargo type is used for messaging related to group coordinated
    // actions.
    CT_GROUP_ACTION_MESSAGE = 8,

    // This cargo type is used for messaging when checking if a group is valid
    // for some task
    CT_GROUP_VALIDATION_MESSAGE = 9,

    // This cargo type is used for synchronization before executing a
    // transaction.
    CT_SYNC_BEFORE_EXECUTION_MESSAGE = 10,

    // This cargo type is used for transaction data with guarantee.
    CT_TRANSACTION_WITH_GUARANTEE_MESSAGE = 11,

    // This cargo type is used to inform about prepared transactions.
    CT_TRANSACTION_PREPARED_MESSAGE = 12,

    // This cargo type is used for messages that are for
    // senders/consumers outside the GR plugin.
    CT_MESSAGE_SERVICE_MESSAGE = 13,

    // No valid type codes can appear after this one.
    CT_MAX = 14
  };

 private:
  /**
   This header instance protocol version.
   */
  int m_version;

  /**
   This header instance length.
   */
  unsigned short m_fixed_header_len;

  /**
   This is the message length field.
   */
  unsigned long long m_msg_len;

  /**
   The cargo type code.
   */
  enum_cargo_type m_cargo_type;

 public:
  virtual ~Plugin_gcs_message() = default;

  /**
   @return the value of the version field.
   */
  int get_version() { return m_version; }

  /**
   @return the value of the header length field value.
   */
  unsigned short get_header_length() { return m_fixed_header_len; }

  /**
    @return the cargo type.
  */
  enum_cargo_type get_cargo_type() const { return m_cargo_type; }

  /**
   @return the message length field value.
   */
  unsigned long long get_msg_length() { return m_msg_len; }

  /**
    Encodes the contents of this instance into the buffer.

    @param[out] buffer the buffer to encode to.
  */
  void encode(std::vector<unsigned char> *buffer) const;

  /**
    Decodes the contents of the buffer and sets the field values
    according to the values decoded.

    @param[in] buffer the buffer to decode from.
    @param[in] length the length of the buffer.
  */
  void decode(const unsigned char *buffer, size_t length);

  /**
    Return the cargo type of a given message buffer, without decode
    the complete message.

    @param[in] buffer the buffer to decode from.

    @return the cargo type of a given message buffer
   */
  static enum_cargo_type get_cargo_type(const unsigned char *buffer);

  /**
    Return the raw data of the first payload item of a given message buffer,
    without decode the complete message.

    @param[out] buffer              the buffer to decode from.
    @param[out] payload_item_data   the data.
    @param[out] payload_item_length the length of the data.
  */
  static void get_first_payload_item_raw_data(
      const unsigned char *buffer, const unsigned char **payload_item_data,
      size_t *payload_item_length);

 protected:
  /**
    Plugin_gcs_message constructor. Only to be called by derivative classes

    @param[in] cargo_type Message type to be sent
   */
  explicit Plugin_gcs_message(enum_cargo_type cargo_type);

  /**
    Encodes the header of this instance into the buffer.

    @param[out] buffer the buffer to encode to.
  */
  void encode_header(std::vector<unsigned char> *buffer) const;

  /**
    Decodes the header of the buffer into this instance.

    @param[out] slider before call `decode_header`: the start of the buffer
                       after call `decode_header`: the position on which the
                                                   header ends on the buffer.
  */
  void decode_header(const unsigned char **slider);

  /**
    Encodes the contents of this instance payload into the buffer.

    @param[out] buffer the buffer to encode to.
  */
  virtual void encode_payload(std::vector<unsigned char> *buffer) const = 0;

  /**
    Decodes the contents of the buffer and sets the payload field
    values according to the values decoded.

    @param[in] buffer the buffer to decode from.
    @param[in] end    the end of the buffer.
  */
  virtual void decode_payload(const unsigned char *buffer,
                              const unsigned char *end) = 0;

  /**
    Encodes the given payload item type and length into the buffer.

    @param[out] buffer              the buffer to encode to
    @param[in]  payload_item_type   the type of the payload item
    @param[in]  payload_item_length the length of the payload item
  */
  void encode_payload_item_type_and_length(
      std::vector<unsigned char> *buffer, uint16 payload_item_type,
      unsigned long long payload_item_length) const;

  /**
    Decodes the given payload item type and length from the buffer.

    @param[in]  buffer              the buffer to encode from
    @param[out] payload_item_type   the type of the payload item
    @param[out] payload_item_length the length of the payload item
  */
  static void decode_payload_item_type_and_length(
      const unsigned char **buffer, uint16 *payload_item_type,
      unsigned long long *payload_item_length);

  /**
    Encodes the given payload item (type, length and value) into the buffer as
    a char (1 byte).

    @param[out] buffer the buffer to encode to
    @param[in]  type   the type of the payload item
    @param[in]  value  the value of the payload item
  */
  void encode_payload_item_char(std::vector<unsigned char> *buffer, uint16 type,
                                unsigned char value) const;

  /**
    Decodes the given payload item (type, length and value) from the buffer as
    a char (1 byte).

    @param[in]  buffer the buffer to encode from
    @param[out] type   the type of the payload item
    @param[out] value  the value of the payload item
  */
  static void decode_payload_item_char(const unsigned char **buffer,
                                       uint16 *type, unsigned char *value);

  /**
    Encodes the given payload item (type, length and value) into the buffer as
    a 2 bytes integer.

    @param[out] buffer the buffer to encode to
    @param[in]  type   the type of the payload item
    @param[in]  value  the value of the payload item
  */
  void encode_payload_item_int2(std::vector<unsigned char> *buffer, uint16 type,
                                uint16 value) const;

  /**
    Decodes the given payload item (type, length and value) from the buffer as
    a 2 bytes integer.

    @param[in]  buffer the buffer to encode from
    @param[out] type   the type of the payload item
    @param[out] value  the value of the payload item
  */
  void decode_payload_item_int2(const unsigned char **buffer, uint16 *type,
                                uint16 *value);

  /**
    Encodes the given payload item (type, length and value) into the buffer as
    a 4 bytes integer.

    @param[out] buffer the buffer to encode to
    @param[in]  type   the type of the payload item
    @param[in]  value  the value of the payload item
  */
  void encode_payload_item_int4(std::vector<unsigned char> *buffer, uint16 type,
                                uint32 value) const;

  /**
    Decodes the given payload item (type, length and value) from the buffer as
    a 4 bytes integer.

    @param[in]  buffer the buffer to encode from
    @param[out] type   the type of the payload item
    @param[out] value  the value of the payload item
  */
  void decode_payload_item_int4(const unsigned char **buffer, uint16 *type,
                                uint32 *value);

  /**
    Encodes the given payload item (type, length and value) into the buffer as
    a 8 bytes integer.

    @param[out] buffer the buffer to encode to
    @param[in]  type   the type of the payload item
    @param[in]  value  the value of the payload item
  */
  void encode_payload_item_int8(std::vector<unsigned char> *buffer, uint16 type,
                                ulonglong value) const;

  /**
    Decodes the given payload item (type, length and value) from the buffer as
    a 8 bytes integer.

    @param[in]  buffer the buffer to encode from
    @param[out] type   the type of the payload item
    @param[out] value  the value of the payload item
  */
  void decode_payload_item_int8(const unsigned char **buffer, uint16 *type,
                                uint64 *value);

  /**
    Encodes the given payload item (type, length and value) into the buffer as
    a char array (variable size).

    @param[out] buffer the buffer to encode to
    @param[in]  type   the type of the payload item
    @param[in]  value  the value of the payload item
    @param[in]  length the length of the payload item
  */
  void encode_payload_item_string(std::vector<unsigned char> *buffer,
                                  uint16 type, const char *value,
                                  unsigned long long length) const;

  /**
    Decodes the given payload item (type, length and value) from the buffer as
    a char array (variable size).

    @param[in]  buffer the buffer to encode from
    @param[out] type   the type of the payload item
    @param[out] value  the value of the payload item
    @param[out] length the length of the payload item
  */
  void decode_payload_item_string(const unsigned char **buffer, uint16 *type,
                                  std::string *value,
                                  unsigned long long *length);

  /**
    Encodes the given payload item (type, length and value) into the buffer as
    a byte buffer (variable size).

    @param[out] buffer the buffer to encode to
    @param[in]  type   the type of the payload item
    @param[in]  value  the value of the payload item
    @param[in]  length the length of the payload item
  */
  void encode_payload_item_bytes(std::vector<unsigned char> *buffer,
                                 uint16 type, const unsigned char *value,
                                 unsigned long long length) const;

  /**
    Decodes the given payload item (type, length and value) from the buffer as
    a byte buffer (variable size).

    @param[in]  buffer the buffer to encode from
    @param[out] type   the type of the payload item
    @param[out] value  the value of the payload item
    @param[out] length the length of the payload item
  */
  void decode_payload_item_bytes(const unsigned char **buffer, uint16 *type,
                                 unsigned char *value,
                                 unsigned long long *length);
};

#endif /* GCS_PLUGIN_MESSAGES_INCLUDED */
