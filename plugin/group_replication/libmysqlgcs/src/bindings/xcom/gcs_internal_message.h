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

#ifndef GCS_MSG_H
#define GCS_MSG_H

#include <stdlib.h>
#include <cassert>
#include <limits>
#include <string>
#include <type_traits>

/**
 This is the fixed header of a GCS message. This header is internal to the
 MySQL GCS library and contains metadata information about the contents of
 the message. Moreover it contains additional dynamic headers that are
 created by internal protocols that are built on top of the basic send/receive
 procedure.

 These protocols add dynamic headers after this header and before the payload,
 then the on-the-wire layout looks like this:

   +----------------------------------------------------+
   | fixed header | dynamic headers |         payload   |
   +----------------------------------------------------+

 The on-the-wire representation of the fixed header header is:

  +------------------+-----------+--------------------------------------+
  | field            | wire size | description                          |
  +==================+===========+======================================+
  | version          |   4 bytes | protocol version                     |
  | fixed_hdr_len    |   2 bytes | length of the fixed header           |
  | message_len      |   8 bytes | length of the message                |
  | dyn_hdr_len      |   4 bytes | length of the dynamic headers        |
  | cargo_type       |   2 bytes | the cargo type in the payload        |
  +------------------+-----------+--------------------------------------+

 The on-the-wire-layout representation of a dynamic header is:

  +------------------+-----------+--------------------------------------+
  | field            | wire size | description                          |
  +==================+===========+======================================+
  | dyn_hdr_len      |   2 bytes | length of the dynamic header         |
  | stage_code       |   4 bytes | stage code                           |
  | old_payload_len  |   8 bytes | length of the previous stage payload |
  +------------------+-----------+--------------------------------------+

 Each dynamic header may have its own metadata, thence following the field
 old_payload_len there could be more metadata that serves as input for the
 stage that is processing this header. @c Gcs_message_stage for additional
 details.
 */
class Gcs_internal_message_header {
 public:
  /**
   The protocol version number length field.
   */
  static const unsigned short WIRE_VERSION_SIZE = 4;

  /**
   On-the-wire size of the fixed header length field.
   */
  static const unsigned short WIRE_HD_LEN_SIZE = 2;

  /**
   On-the-wire size of the message size field.
   */
  static const unsigned short WIRE_TOTAL_LEN_SIZE = 8;

  /**
   On-the-wire size of the cargo type field.
   */
  static const unsigned short WIRE_CARGO_TYPE_SIZE = 2;

  /**
   On-the-wire size of the dynamic headers length field.
   */
  static const unsigned short WIRE_DYNAMIC_HDRS_LEN_SIZE = 4;

  /**
   On-the-wire offset of the dynamic headers length field.
   */
  static const unsigned short WIRE_DYNAMIC_HDRS_LEN_OFFSET =
      WIRE_VERSION_SIZE + WIRE_HD_LEN_SIZE + WIRE_TOTAL_LEN_SIZE;

  /**
   On-the-wire offset of the message length field.
   */
  static const unsigned short WIRE_MSG_LEN_OFFSET =
      WIRE_VERSION_SIZE + WIRE_HD_LEN_SIZE;

  /**
   On-the-wire size of the fixed header.
   */
  static const unsigned short WIRE_TOTAL_FIXED_HEADER_SIZE =
      WIRE_VERSION_SIZE + WIRE_HD_LEN_SIZE + WIRE_TOTAL_LEN_SIZE +
      WIRE_DYNAMIC_HDRS_LEN_SIZE + WIRE_CARGO_TYPE_SIZE;

  /**
   The different cargo type codes.

   NOTE: all type values must fit into WIRE_CARGO_TYPE_SIZE bytes storage.
   */
  enum class cargo_type : unsigned short {
    // this type should not be used anywhere.
    CT_UNKNOWN = 0,

    // this cargo type is used for internal messaging related to stage
    // exchanges.
    CT_INTERNAL_STATE_EXCHANGE = 1,

    // this cargo type is used for messages from the application
    CT_USER_DATA = 2,

    /**
     No valid type codes can appear after this one. If a type code is to
     be added, this value needs to be incremented and the lowest type code
     available be assigned to the new stage.
     */
    CT_MAX = 3
  };

 private:
  /**
   The header instance protocol version.
   */
  unsigned int m_version;
  static_assert(sizeof(decltype(m_version)) == WIRE_VERSION_SIZE,
                "The m_version size does not match the storage capacity");

  /**
   The header instance length.
   */
  unsigned short m_fixed_header_len;
  static_assert(
      sizeof(decltype(m_fixed_header_len)) == WIRE_HD_LEN_SIZE,
      "The m_fixed_header_len size does not match the storage capacity");

  /**
   The message length field.
   */
  unsigned long long m_total_len;
  static_assert(sizeof(decltype(m_total_len)) <= WIRE_TOTAL_LEN_SIZE,
                "The m_total_len size does not match the storage capacity");

  /**
   The length of the dynamic headers.
   */
  unsigned int m_dynamic_headers_len;
  static_assert(
      sizeof(decltype(m_dynamic_headers_len)) == WIRE_DYNAMIC_HDRS_LEN_SIZE,
      "The m_dynamic_headers_len size does not match the storage capacity");

  /**
   The cargo type code.
   */
  cargo_type m_cargo_type;
  static_assert(sizeof(decltype(m_cargo_type)) == WIRE_CARGO_TYPE_SIZE,
                "The m_cargo_type size does not match the storage capacity");

 public:
  explicit Gcs_internal_message_header()
      : m_version(0),
        m_fixed_header_len(WIRE_TOTAL_FIXED_HEADER_SIZE),
        m_total_len(WIRE_TOTAL_FIXED_HEADER_SIZE),
        m_dynamic_headers_len(0),
        m_cargo_type() {}

  /**
   @return the value of the protocol version field.
   */
  unsigned int get_version() const { return m_version; }

  /**
   Set the current protocol version.

   @param version Current protocol version.
   */
  void set_version(unsigned int version) { m_version = version; }

  /**
   @return the value of the header length field value.
   */
  unsigned short get_fixed_header_length() const { return m_fixed_header_len; }

  /**
   @return the cargo type.
   */
  cargo_type get_cargo_type() const { return m_cargo_type; }

  /**
   @return the message total length field value.
   */
  unsigned long long get_total_length() const { return m_total_len; }

  /**
   @return the dynamic headers length field value.
   */
  unsigned int get_dynamic_headers_length() const {
    return m_dynamic_headers_len;
  }

  /**
   Set the message length attribute value according to the payload length and
   the header length. Only the payload information is provided because the
   header length is fixed.

   @param lenght payload length
   */
  void set_payload_length(unsigned long long length) {
    m_total_len = length + m_fixed_header_len;
  }

  /**
   Set the message total length attribute value.

   @param lenght message length
   */
  void set_total_length(unsigned long long length) { m_total_len = length; }

  /**
   Set the dynamic headers length field value.

   @param len the dynamic headers value.
   */
  void set_dynamic_headers_length(unsigned int len) {
    m_dynamic_headers_len = len;
  }

  /**
   Set the cargo type field value.

   @param type cargo type to set
   */
  void set_cargo_type(Gcs_internal_message_header::cargo_type type) {
    m_cargo_type = type;
  }

  /**
   Decode the contents of the buffer and sets the field values according to
   the values decoded. The buffer MUST be encoded in little endian format.

   @param buffer the buffer to decode from.
   @return false on success, true otherwise.
   */
  bool decode(const unsigned char *buffer);

  /**
   Encode the contents of this instance into the buffer. The encoding SHALL
   be done in little endian format.

   @param buffer the buffer to encode to.
   @return false on success, true otherwise.
   */
  bool encode(unsigned char *buffer) const;
};

/**
 This class is an abstraction for the packet concept. It is used to manipulate
 the contents of a buffer that is to be sent to the network in an optimal way.
 */
class Gcs_packet {
 private:
  /**
   The buffer containing the data for this packet.
   */
  unsigned char *m_buffer;

  /**
   The capacity of the packet.
   */
  unsigned long long m_capacity;

  /**
   The length of the data in this packet.
   */
  unsigned long long m_total_len;

  /**
   The header size.
   */
  unsigned int m_header_len;

  /**
   The offset of the current dynamic header within the buffer.
   */
  unsigned int m_dyn_headers_len;  // always points to the next header

  /**
   The length of the payload.
   */
  unsigned long long m_payload_len;

  /**
   Version in use by the packet
   */
  unsigned int m_version;

 public:
  /**
   Reallocations are done in chunks. This is the minimum amount of memory
   that is reallocated each time.
   */
  static const unsigned short BLOCK_SIZE = 1024;

  /**
   This is the constructor that SHALL be used for incoming messages from the
   group. This will decode the buffer and set internal cursors and offsets
   according to the information in the headers in the buffer.

   @param buffer the buffer with the data that has arrived from the group.
   @param capacity the size of the buffer.
   */
  explicit Gcs_packet(unsigned char *buffer, unsigned long long capacity)
      : m_buffer(buffer),
        m_capacity(capacity),
        m_total_len(0),
        m_header_len(0),
        m_dyn_headers_len(0),
        m_payload_len(0),
        m_version(0) {
    Gcs_internal_message_header hd;
    hd.decode(buffer);

    // update the internal pointers
    reload_header(hd);
  }

  /**
   This constructor is to be used when sending a message. This builds a packet
   with an internal buffer that is used to prepare the data to be sent.

   @param capacity the capacity that this packet should be set to.
   */
  explicit Gcs_packet(const Gcs_internal_message_header &hd)
      : m_buffer(nullptr), m_capacity(0) {
    reload_header(hd);
    if (m_total_len > 0) {
      m_capacity = calculate_capacity(m_total_len);
      m_buffer = create_buffer(m_capacity);
      if (m_buffer == nullptr) m_capacity = 0;
    }
  }

  Gcs_packet(Gcs_packet &p) = delete;

  Gcs_packet &operator=(const Gcs_packet &p) = delete;

  Gcs_packet(Gcs_packet &&p) = delete;

  Gcs_packet &operator=(Gcs_packet &&p) = delete;

  virtual ~Gcs_packet() {}

  /**
   @return the value of the version in use.
   */
  unsigned int get_version() const { return m_version; }

  /**
   This method sets the payload length.

   @param length payload length.
   */
  void set_payload_length(unsigned long long length) { m_payload_len = length; }

  /**
   Return the payload length.
   */
  unsigned long long get_payload_length() const { return m_payload_len; }

  /**
   Return the total data length which includes all headers and also the
   payload.
   */
  unsigned long long get_total_length() const {
    assert((m_payload_len + m_header_len) == m_total_len);
    return m_total_len;
  }

  /**
   Return a pointer to the header.
   */
  const unsigned char *get_header() const { return m_buffer; }

  /**
   Return a pointer to the payload.
   */
  unsigned char *get_payload() const { return m_buffer + m_header_len; }

  /**
   Set the dynamic headers length.

   @param length dynamic headers length.
   */
  void set_dyn_headers_length(unsigned int length) {
    m_dyn_headers_len = length;
  }

  /**
   Return the dynamic headers length.
   */
  unsigned int get_dyn_headers_length() const { return m_dyn_headers_len; }

  /**
   Return a pointer to the buffer which has the same address as the header
   in the current implementation.
   */
  unsigned char *get_buffer() const { return m_buffer; }

  /**
   Release the memory allocated to the current buffer.
   */
  void free_buffer() { free(swap_buffer(nullptr, 0)); }

  /**
   Swap the current buffer for another and return the pointer to the old buffer.

   @param buffer pointer to the new buffer
   @param capacity the capacity of the new buffer.
   */
  unsigned char *swap_buffer(unsigned char *buffer,
                             unsigned long long capacity) {
    unsigned char *cur = m_buffer;
    m_buffer = buffer;
    m_capacity = capacity;
    return cur;
  }

  /**
   Allocate memory to a new buffer whose capacity is automically converted to
   a multiple of BLOCK_SIZE and return a pointer to it.

   @param capacity size of the new buffer in bytes.
   */
  static unsigned char *create_buffer(unsigned long long capacity) {
    return static_cast<unsigned char *>(malloc(calculate_capacity(capacity)));
  }

  /**
   Return a given a capacity converted to the next multiple of BLOCK_SIZE.

   @param capacity this is the capacity in bytes.
   */
  static unsigned long long calculate_capacity(unsigned long long capacity) {
    return ((capacity + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
  }

  /**
   Set the header length.

   @param length the header length.
   */
  void set_header_length(unsigned int length) { m_header_len = length; }

  /**
   Return the header length
   */
  unsigned int get_header_length() const { return m_header_len; }

  /**
   Return the buffer capacity.
   */
  unsigned long long get_capacity() const { return m_capacity; }

  /**
   Reprocess a header and automatically set all attributes accordingly.

   @param hd This is a reference to the header object.
   */
  void reload_header(const Gcs_internal_message_header &hd);
};
#endif
