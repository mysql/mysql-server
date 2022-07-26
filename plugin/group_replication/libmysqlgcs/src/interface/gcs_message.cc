/* Copyright (c) 2014, 2022, Oracle and/or its affiliates.

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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "my_compiler.h"

#include "mysql/psi/mysql_memory.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_message.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/byteorder.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_util.h"

/**
  Gcs_message_data implementation
*/
Gcs_message_data::Gcs_message_data()
    : m_header(nullptr),
      m_header_slider(nullptr),
      m_header_len(0),
      m_header_capacity(0),
      m_payload(nullptr),
      m_payload_slider(nullptr),
      m_payload_len(0),
      m_payload_capacity(0),
      m_buffer(nullptr),
      m_buffer_len(0),
      m_owner(true) {}

Gcs_message_data::Gcs_message_data(const uint32_t header_capacity,
                                   const uint64_t payload_capacity)
    : m_header(nullptr),
      m_header_slider(nullptr),
      m_header_len(0),
      m_header_capacity(header_capacity),
      m_payload(nullptr),
      m_payload_slider(nullptr),
      m_payload_len(0),
      m_payload_capacity(payload_capacity),
      m_buffer_len(0),
      m_owner(true) {
  m_buffer_len = header_capacity + payload_capacity + get_encode_header_size();
  size_t buf_tmp_len = sizeof(uchar) * m_buffer_len;
  m_buffer = static_cast<uchar *>(malloc(buf_tmp_len));
  Gcs_message_data::report_allocate_memory(buf_tmp_len);
  m_header = m_header_slider = m_buffer + get_encode_header_size();
  m_payload = m_payload_slider =
      m_buffer + get_encode_header_size() + header_capacity;
}

Gcs_message_data::Gcs_message_data(const uint64_t data_len)
    : m_header(nullptr),
      m_header_slider(nullptr),
      m_header_len(0),
      m_header_capacity(0),
      m_payload(nullptr),
      m_payload_slider(nullptr),
      m_payload_len(0),
      m_payload_capacity(0),
      m_buffer(nullptr),
      m_buffer_len(data_len),
      m_owner(true) {
  size_t buf_tmp_len = sizeof(uchar) * m_buffer_len;
  m_buffer = static_cast<uchar *>(malloc(buf_tmp_len));
  Gcs_message_data::report_allocate_memory(buf_tmp_len);
}

Gcs_message_data::~Gcs_message_data() {
  if (m_owner) {
    free(m_buffer);
    Gcs_message_data::report_deallocate_memory(sizeof(uchar) * m_buffer_len);
  }
}

bool Gcs_message_data::report_allocate_memory(size_t size [[maybe_unused]]) {
#ifdef HAVE_PSI_MEMORY_INTERFACE
  PSI_thread *owner = nullptr;
  if (PSI_MEMORY_CALL(memory_alloc)(key_MEM_Gcs_message_data_m_buffer, size,
                                    &owner) == PSI_NOT_INSTRUMENTED) {
    return true;
  }
  /* This instrument is flagged global, so there should be no thread owner. */
  assert(owner == nullptr);
#endif /* HAVE_PSI_MEMORY_INTERFACE */
  return false;
}

void Gcs_message_data::report_deallocate_memory(size_t size [[maybe_unused]]) {
#ifdef HAVE_PSI_MEMORY_INTERFACE
  PSI_MEMORY_CALL(memory_free)
  (key_MEM_Gcs_message_data_m_buffer, size, nullptr);
#endif /* HAVE_PSI_MEMORY_INTERFACE */
}

const uchar *Gcs_message_data::get_header() const { return m_header; }

uint32_t Gcs_message_data::get_header_length() const { return m_header_len; }

const uchar *Gcs_message_data::get_payload() const { return m_payload; }

uint64_t Gcs_message_data::get_payload_length() const { return m_payload_len; }

uint64_t Gcs_message_data::get_encode_size() const {
  return get_encode_header_size() + get_encode_payload_size();
}

uint64_t Gcs_message_data::get_encode_payload_size() const {
  uint32_t header_len = get_header_length();
  uint64_t payload_len = get_payload_length();

  return header_len     /* header */
         + payload_len; /* payload */
}

uint64_t Gcs_message_data::get_encode_header_size() const {
  return WIRE_HEADER_LEN_SIZE     /* header length */
         + WIRE_PAYLOAD_LEN_SIZE; /* payload length */
}

/* purecov: begin deadcode */
bool Gcs_message_data::append_to_header(const uchar *to_append,
                                        uint32_t to_append_len) {
  if (m_header_capacity < to_append_len) {
    MYSQL_GCS_LOG_ERROR("Header reserved capacity is "
                        << m_header_capacity
                        << " but it "
                           "has been requested to add data whose size is "
                        << to_append_len);
    return true;
  }

  memcpy(m_header_slider, to_append, to_append_len);
  m_header_slider += to_append_len;
  m_header_len += to_append_len;

  return false;
}
/* purecov: end */

bool Gcs_message_data::append_to_payload(const uchar *to_append,
                                         uint64_t to_append_len) {
  if (m_payload_capacity < to_append_len) {
    MYSQL_GCS_LOG_ERROR("Payload reserved capacity is "
                        << m_payload_capacity
                        << " but it "
                           "has been requested to add data whose size is "
                        << to_append_len);
    return true;
  }

  memcpy(m_payload_slider, to_append, to_append_len);
  m_payload_slider += to_append_len;
  m_payload_len += to_append_len;

  return false;
}

/* purecov: begin deadcode */
void Gcs_message_data::release_ownership() { m_owner = false; }

bool Gcs_message_data::encode(uchar **buffer, uint64_t *buffer_len) const {
  uint32_t header_len = get_header_length();
  uint64_t payload_len = get_payload_length();
  uint32_t header_len_enc = htole32(header_len);
  uint64_t payload_len_enc = htole64(payload_len);
  unsigned char *slider = m_buffer;

  /*
    Note that the encoded_size will be greater than zero even when
    there are no header's nor payload's content.
  */
  assert(get_encode_size() > 0);
  assert(get_encode_size() == m_buffer_len);

  if (buffer == nullptr || buffer_len == nullptr) {
    MYSQL_GCS_LOG_ERROR(
        "Buffer to return information on encoded data or encoded data "
        "size is not properly configured.");
    return true;
  }

  memcpy(slider, &header_len_enc, WIRE_HEADER_LEN_SIZE);
  slider += WIRE_HEADER_LEN_SIZE;

  memcpy(slider, &payload_len_enc, WIRE_PAYLOAD_LEN_SIZE);

  *buffer = m_buffer;
  *buffer_len = m_buffer_len;

  return false;
}
/* purecov: end */

bool Gcs_message_data::encode(uchar *buffer, uint64_t *buffer_len) const {
  uint32_t header_len = get_header_length();
  uint64_t payload_len = get_payload_length();
  uint32_t header_len_enc = htole32(header_len);
  uint64_t payload_len_enc = htole64(payload_len);
  uint64_t encoded_size = get_encode_size();
  unsigned char *slider = buffer;

  if (buffer == nullptr || buffer_len == nullptr) {
    MYSQL_GCS_LOG_ERROR(
        "Buffer to return information on encoded data or encoded data "
        "size is not properly configured.");
    return true;
  }

  if (*buffer_len < encoded_size) {
    MYSQL_GCS_LOG_ERROR("Buffer reserved capacity is "
                        << *buffer_len
                        << " but it has "
                           "been requested to add data whose size is "
                        << encoded_size);
    return true;
  }

  *buffer_len = encoded_size;

  memcpy(slider, &header_len_enc, WIRE_HEADER_LEN_SIZE);
  slider += WIRE_HEADER_LEN_SIZE;
  assert(static_cast<uint64_t>(slider - buffer) < *buffer_len);

  memcpy(slider, &payload_len_enc, WIRE_PAYLOAD_LEN_SIZE);
  slider += WIRE_PAYLOAD_LEN_SIZE;
  assert(static_cast<uint64_t>(slider - buffer) <= *buffer_len);

  memcpy(slider, get_header(), header_len);
  slider += header_len;
  assert(static_cast<uint64_t>(slider - buffer) <= *buffer_len);

  memcpy(slider, get_payload(), payload_len);
  slider += payload_len;
  assert(static_cast<uint64_t>(slider - buffer) <= *buffer_len);

  MYSQL_GCS_DEBUG_EXECUTE(
      [[maybe_unused]] uint64_t encoded_header_size = get_encode_header_size();
      MYSQL_GCS_LOG_TRACE(
          "Encoded message: (header)= %llu (payload)= %llu",
          static_cast<unsigned long long>(encoded_header_size),
          static_cast<unsigned long long>(header_len + payload_len)););

  return false;
}

bool Gcs_message_data::decode(const uchar *data, uint64_t data_len) {
  uchar *slider = m_buffer;

  if (data == nullptr || data_len == 0 || m_buffer == nullptr) {
    MYSQL_GCS_LOG_ERROR(
        "Buffer to decode information from is not properly configured.");
    return true;
  }

  if (m_buffer_len < data_len) {
    MYSQL_GCS_LOG_ERROR("Buffer reserved capacity is "
                        << m_buffer_len
                        << " but it has "
                           "been requested to decode data whose size is "
                        << data_len);
    return true;
  }

  /*
    Copy the external buffer to the internal buffer.
  */
  memcpy(m_buffer, data, data_len);

  /*
    Get header metadata from the internal buffer.
  */
  memcpy(&m_header_len, slider, WIRE_HEADER_LEN_SIZE);
  m_header_len = le32toh(m_header_len);
  slider += WIRE_HEADER_LEN_SIZE;

  memcpy(&m_payload_len, slider, WIRE_PAYLOAD_LEN_SIZE);
  m_payload_len = le64toh(m_payload_len);
  slider += WIRE_PAYLOAD_LEN_SIZE;
  m_header = slider;

  /*
    Get header data and payload from the internal buffer.
  */
  if (static_cast<uint64_t>((slider + m_header_len) - m_buffer) > data_len)
    return true;
  slider += m_header_len;
  m_payload = slider;

  if (static_cast<uint64_t>((slider + m_payload_len) - m_buffer) > data_len)
    return true;

  MYSQL_GCS_LOG_TRACE(
      "Decoded message: (header)= %llu and (payload)= %llu",
      static_cast<unsigned long long>(m_header - m_buffer),
      static_cast<unsigned long long>(m_header_len + m_payload_len));

  return false;
}

/**
  Gcs_message implementation.
*/
Gcs_message::Gcs_message(const Gcs_member_identifier &origin,
                         const Gcs_group_identifier &destination,
                         Gcs_message_data *message_data)
    : m_origin(nullptr), m_destination(nullptr), m_data(nullptr) {
  init(&origin, &destination, message_data);
}

Gcs_message::Gcs_message(const Gcs_member_identifier &origin,
                         Gcs_message_data *message_data)
    : m_origin(nullptr), m_destination(nullptr), m_data(nullptr) {
  init(&origin, nullptr, message_data);
}

Gcs_message::~Gcs_message() {
  delete m_destination;
  delete m_origin;
  delete m_data;
}

const Gcs_member_identifier &Gcs_message::get_origin() const {
  return *m_origin;
}

/* purecov: begin deadcode */
const Gcs_group_identifier *Gcs_message::get_destination() const {
  return m_destination;
}
/* purecov: end */

Gcs_message_data &Gcs_message::get_message_data() const { return *m_data; }

void Gcs_message::init(const Gcs_member_identifier *origin,
                       const Gcs_group_identifier *destination,
                       Gcs_message_data *message_data) {
  if (origin != nullptr)
    m_origin = new Gcs_member_identifier(origin->get_member_id());

  if (destination != nullptr)
    m_destination = new Gcs_group_identifier(destination->get_group_id());

  if (message_data != nullptr)
    m_data = message_data;
  else
    assert(false);
}
