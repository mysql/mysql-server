/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef GCS_MSG_H
#define GCS_MSG_H

#include <stdlib.h>
#include <string>

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
  | type_code        |   4 bytes | protocol version                     |
  +------------------+-----------+--------------------------------------+

 Each dynamic header may have its own metadata, thence following the field
 type_code there could be more metadata that serves as input for the stage
 that is processing this header. @c Gcs_msg_stage for additional details.
 */



class Gcs_internal_message_header
{
public:

  /**
   The protocol version number.
   */
  static const int GCS_PROTO_VERSION;

  /**
   The protocol version number.
   */
  static const unsigned  short WIRE_VERSION_SIZE;

  /**
   The on-the-wire size of the header length field.
   */
  static const unsigned  short WIRE_HD_LEN_SIZE;

  /**
   The on-the-wire size of the message size field.
   */
  static const unsigned  short WIRE_MSG_LEN_SIZE;

  /**
   The on-the-wire size of the cargo type field.
   */
  static const unsigned  short WIRE_CARGO_TYPE_SIZE;

  /**
   The on-the-wire size of the dynamic headers length field.
   */
  static const unsigned  short WIRE_DYNAMIC_HDRS_LEN_SIZE;

  /**
   The on-the-wire offset of the dynamic headers length field.
   */
  static const unsigned short WIRE_DYNAMIC_HDRS_LEN_OFFSET;

  /**
   The on-the-wire offset of the message length field.
   */
  static const unsigned short WIRE_MSG_LEN_OFFSET;

  /**
   The on-the-wire size of the fixed header.
   */
  static const unsigned  short WIRE_FIXED_HEADER_SIZE;

  /**
   The different cargo type codes.

   NOTE: all type values must fit into WIRE_CARGO_TYPE_SIZE bytes storage.
   */
  enum enum_cargo_type
  {
    // this type should not be used anywhere.
    CT_UNKNOWN= 0,

    // this cargo type is used for internal messaging related to stage exchanges.
    CT_INTERNAL_STATE_EXCHANGE= 1,

    // this cargo type is used for messages from the application
    CT_USER_DATA= 2,

    /**
     No valid type codes can appear after this one. If a type code is to
     be added, this value needs to be incremented and the lowest type code
     available be assigned to the new stage.
     */
    CT_MAX= 3
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
   The length of the dynamic headers.
   */
  unsigned int m_dynamic_headers_len;

  /**
   The cargo type code.
   */
  enum_cargo_type m_cargo_type;

public:
  explicit Gcs_internal_message_header()
  : m_version(GCS_PROTO_VERSION),
    m_fixed_header_len(WIRE_FIXED_HEADER_SIZE),
    m_msg_len(WIRE_FIXED_HEADER_SIZE),
    m_dynamic_headers_len(0),
    m_cargo_type(CT_UNKNOWN)
  { }

  explicit Gcs_internal_message_header(enum_cargo_type cargo_type)
  : m_version(GCS_PROTO_VERSION),
    m_fixed_header_len(WIRE_FIXED_HEADER_SIZE),
    m_msg_len(WIRE_FIXED_HEADER_SIZE),
    m_dynamic_headers_len(0),
    m_cargo_type(cargo_type)
 { }

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
  enum_cargo_type get_cargo_type() { return m_cargo_type; }

  /**
   @return the message length field value.
   */
  unsigned long long get_msg_length() { return m_msg_len; }

  /**
   @return sets the message length field value.
   */
  void set_msg_length(unsigned long long len) { m_msg_len= len; }

  /**
   Sets the dynamic headers length field value.

   @param len the dynamic headers value.
   */
  void set_dynamic_headers_length(unsigned int len) { m_dynamic_headers_len= len; }

  /**
   @return the dynamic headers length field value.
  */
  unsigned int get_dynamic_headers_length() { return m_dynamic_headers_len; }

  /**
   Sets the cargo type field value.

   @param len the dynamic headers value.
  */
  void set_cargo_type(Gcs_internal_message_header::enum_cargo_type type) { m_cargo_type = type; }

  /**
   Decodes the contents of the buffer and sets the field values according to
   the values decoded. The buffer MUST be encoded in little endian format.

   @param buffer the buffer to decode from.
   @return false on success, true otherwise.
   */
  bool encode(unsigned char* buffer);

  /**
   Encodes the contents of this instance into the buffer. The encoding SHALL
   be done in little endian format.

   @param buffer the buffer to encode to.
   @return false on success, true otherwise.
   */
  bool decode(const unsigned char* buffer);
};

/**
 This class is an abstraction for the packet concept. It is used to manipulate
 the contents of a buffer that is to be sent to the network in an optimal way.
 */
class Gcs_packet
{
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
  unsigned long long m_length;

  /**
   The header size.
   */
  unsigned int m_header_len;

  /**
   The offset of the current dynamic header within the buffer.
   */
  unsigned int m_dyn_headers_len;   // always points to the next header

  /**
   The length of the payload.
   */
  unsigned long long m_payload_len;

public:

  /**
   Reallocations are done in chunks. This is the minimum amount of memory
   that is reallocated each time.
   */
  const static unsigned short BLOCK_SIZE;

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
    m_length(0),
    m_header_len(0),
    m_dyn_headers_len(0),
    m_payload_len(0)
  {
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
  explicit Gcs_packet(unsigned long long capacity)
  : m_buffer(NULL),
    m_capacity(0),
    m_length(0),
    m_header_len(0),
    m_dyn_headers_len(0),
    m_payload_len(0)
  {
    if (capacity > 0)
    {
      m_capacity= (((capacity + BLOCK_SIZE) / BLOCK_SIZE) + 1) * BLOCK_SIZE;
      m_buffer= (unsigned char *) malloc(m_capacity);
    }
  }

  virtual ~Gcs_packet() { }

  void set_payload_length(unsigned long long pl)
  {
    m_payload_len= pl;
  }

  unsigned long long get_payload_length()
  {
    return m_payload_len;
  }

  void set_length(unsigned long long l)
  {
    m_length= l;
  }

  unsigned long long get_length()
  {
    return m_length;
  }

  const unsigned char *get_header()
  {
    return m_buffer;
  }

  unsigned char *get_payload()
  {
    return m_buffer + m_header_len;
  }

  void set_dyn_headers_length(unsigned int o)
  {
    m_dyn_headers_len= o;
  }

  unsigned int get_dyn_headers_length()
  {
    return m_dyn_headers_len;
  }

  unsigned char *get_buffer()
  {
    return m_buffer;
  }

  unsigned char *swap_buffer(unsigned char *b, unsigned long long c)
  {
    unsigned char *cur= m_buffer;
    m_buffer= b;
    m_capacity= c;
    return cur;
  }

  void set_header_length(unsigned int hd_len)
  {
    m_header_len= hd_len;
  }

  unsigned int get_header_length()
  {
    return m_header_len;
  }

  unsigned long long get_capacity()
  {
    return m_capacity;
  }

  void reload_header(Gcs_internal_message_header &hd);
private:
  // make copy and assignment constructors private
  Gcs_packet(Gcs_packet &p);
  Gcs_packet& operator=(const Gcs_packet& p);
};
#endif
