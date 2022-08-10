/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_internal_message_headers.h"

#include <stdlib.h>
#include <string.h>
#include <memory>
#include <string>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/byteorder.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stages.h"

Gcs_protocol_version Gcs_internal_message_header::get_maximum_version() const {
  return m_max_version;
}

Gcs_protocol_version Gcs_internal_message_header::get_used_version() const {
  return m_used_version;
}

void Gcs_internal_message_header::set_maximum_version(
    Gcs_protocol_version version) {
  m_max_version = version;
}

void Gcs_internal_message_header::set_used_version(
    Gcs_protocol_version version) {
  m_used_version = version;
}

Cargo_type Gcs_internal_message_header::get_cargo_type() const {
  return m_cargo_type;
}

void Gcs_internal_message_header::set_cargo_type(Cargo_type type) {
  m_cargo_type = type;
}

unsigned int Gcs_internal_message_header::get_dynamic_headers_length() const {
  return m_dynamic_headers_len;
}

void Gcs_internal_message_header::set_dynamic_headers_length(
    unsigned int length) {
  m_dynamic_headers_len = length;
}

void Gcs_internal_message_header::set_payload_length(
    unsigned long long length) {
  m_payload_len = length;
}

unsigned long long Gcs_internal_message_header::get_total_length() const {
  return m_payload_len + m_dynamic_headers_len + m_fixed_header_len;
}

unsigned long long Gcs_internal_message_header::encode(
    unsigned char *buffer) const {
  unsigned char *slider = buffer;
  unsigned short s_cargo_type = static_cast<unsigned short>(m_cargo_type);
  unsigned short s_used_version = static_cast<unsigned short>(m_used_version);
  unsigned short s_max_version = static_cast<unsigned short>(m_max_version);

  s_used_version = htole16(s_used_version);
  memcpy(slider, &s_used_version, WIRE_USED_VERSION_SIZE);
  slider += WIRE_USED_VERSION_SIZE;
  static_assert(
      sizeof(decltype(m_used_version)) == sizeof(decltype(s_used_version)),
      "The m_used_version size is not equal to s_used_version size");

  s_max_version = htole16(s_max_version);
  memcpy(slider, &s_max_version, WIRE_MAX_VERSION_SIZE);
  slider += WIRE_MAX_VERSION_SIZE;
  static_assert(
      sizeof(decltype(m_max_version)) == sizeof(decltype(s_max_version)),
      "The m_max_version size is not equal to s_max_version size");

  unsigned short le_hdr_len = htole16(m_fixed_header_len);
  memcpy(slider, &le_hdr_len, WIRE_HD_LEN_SIZE);
  slider += WIRE_HD_LEN_SIZE;
  static_assert(
      sizeof(decltype(m_fixed_header_len)) == sizeof(decltype(le_hdr_len)),
      "The m_fixed_header_len size is not equal to le_hdr_len size");

  /*
   Calculate the total size as we are storing it and not the payload size.
   */
  unsigned long long le_total_len =
      htole64(m_payload_len + m_fixed_header_len + m_dynamic_headers_len);
  memcpy(slider, &le_total_len, WIRE_TOTAL_LEN_SIZE);
  slider += WIRE_TOTAL_LEN_SIZE;
  static_assert(
      sizeof(decltype(m_payload_len)) == sizeof(decltype(le_total_len)),
      "The m_total_len size is not equal to le_total_len size");

  unsigned int le_dyn_len = htole32(m_dynamic_headers_len);
  memcpy(slider, &le_dyn_len, WIRE_DYNAMIC_HDRS_LEN_SIZE);
  slider += WIRE_DYNAMIC_HDRS_LEN_SIZE;
  static_assert(
      sizeof(decltype(m_dynamic_headers_len)) == sizeof(decltype(le_dyn_len)),
      "The m_dynamic_headers_len size is not equal to le_dyn_len size");

  s_cargo_type = htole16(s_cargo_type);
  memcpy(slider, &s_cargo_type, WIRE_CARGO_TYPE_SIZE);
  slider += WIRE_CARGO_TYPE_SIZE;
  static_assert(
      sizeof(decltype(m_cargo_type)) == sizeof(decltype(s_cargo_type)),
      "The m_cargo_type size is not equal to s_cargo_type size");

  return slider - buffer;
}

unsigned long long Gcs_internal_message_header::decode(
    const unsigned char *buffer) {
  const unsigned char *slider = buffer;
  unsigned short s_cargo_type = 0;
  unsigned long long s_total_len = 0;
  unsigned short s_max_version = 0;
  unsigned short s_used_version = 0;

  memcpy(&s_used_version, slider, WIRE_USED_VERSION_SIZE);
  s_used_version = le16toh(s_used_version);
  m_used_version = static_cast<Gcs_protocol_version>(s_used_version);
  slider += WIRE_USED_VERSION_SIZE;

  memcpy(&s_max_version, slider, WIRE_MAX_VERSION_SIZE);
  s_max_version = le16toh(s_max_version);
  m_max_version = static_cast<Gcs_protocol_version>(s_max_version);
  slider += WIRE_MAX_VERSION_SIZE;

  /* Take care of old nodes, which send max_version = 0 but support version 1 */
  if (m_max_version == Gcs_protocol_version::UNKNOWN) {
    assert(m_used_version == Gcs_protocol_version::V1);
    m_max_version = m_used_version;
  }

  memcpy(&m_fixed_header_len, slider, WIRE_HD_LEN_SIZE);
  m_fixed_header_len = le16toh(m_fixed_header_len);
  slider += WIRE_HD_LEN_SIZE;

  memcpy(&s_total_len, slider, WIRE_TOTAL_LEN_SIZE);
  s_total_len = le64toh(s_total_len);
  slider += WIRE_TOTAL_LEN_SIZE;

  memcpy(&m_dynamic_headers_len, slider, WIRE_DYNAMIC_HDRS_LEN_SIZE);
  m_dynamic_headers_len = le32toh(m_dynamic_headers_len);
  slider += WIRE_DYNAMIC_HDRS_LEN_SIZE;

  /*
   Calculate the payload size as we are storing the total size.
   */
  m_payload_len = s_total_len - m_fixed_header_len - m_dynamic_headers_len;

  memcpy(&s_cargo_type, slider, WIRE_CARGO_TYPE_SIZE);
  s_cargo_type = le16toh(s_cargo_type);

  m_cargo_type = static_cast<Cargo_type>(s_cargo_type);
  static_assert(
      sizeof(decltype(m_cargo_type)) == sizeof(decltype(s_cargo_type)),
      "The m_cargo_type size is not equal to s_cargo_type size");
  slider += WIRE_CARGO_TYPE_SIZE;

  return slider - buffer;
}

void Gcs_internal_message_header::dump(std::ostringstream &output) const {
  output << "fixed header<used_version=("
         << static_cast<unsigned short>(get_used_version())
         << "), max_version=("
         << static_cast<unsigned short>(get_maximum_version())
         << ") header length=(" << get_fixed_header_length()
         << "), total length=(" << get_total_length() << "), dynamic length=("
         << get_dynamic_headers_length() << "), cargo type=("
         << static_cast<unsigned short>(get_cargo_type()) << ")> ";
}

Gcs_dynamic_header::Gcs_dynamic_header(
    Stage_code stage_code, unsigned long long payload_length) noexcept
    : m_dynamic_header_length(calculate_length()),
      m_stage_code(stage_code),
      m_payload_length(payload_length) {}

unsigned short Gcs_dynamic_header::get_dynamic_header_length() const {
  return m_dynamic_header_length;
}

Stage_code Gcs_dynamic_header::get_stage_code() const { return m_stage_code; }

unsigned long long Gcs_dynamic_header::get_payload_length() const {
  return m_payload_length;
}

void Gcs_dynamic_header::set_payload_length(unsigned long long new_length) {
  m_payload_length = new_length;
}

unsigned long long Gcs_dynamic_header::encode(unsigned char *buffer) const {
  unsigned char *slider = buffer;
  unsigned int stage_code_enc = static_cast<unsigned int>(m_stage_code);

  unsigned short buffer_length_enc = htole16(m_dynamic_header_length);
  memcpy(slider, &buffer_length_enc, WIRE_HD_LEN_SIZE);
  slider += WIRE_HD_LEN_SIZE;
  static_assert(sizeof(decltype(m_dynamic_header_length)) == WIRE_HD_LEN_SIZE,
                "The header_length size does not match storage capacity");
  static_assert(sizeof(decltype(m_dynamic_header_length)) ==
                    sizeof(decltype(buffer_length_enc)),
                "The header_length size is not equal to hd_len_enc size");

  stage_code_enc = htole32(stage_code_enc);
  memcpy(slider, &stage_code_enc, WIRE_HD_TYPE_SIZE);
  slider += WIRE_HD_TYPE_SIZE;
  static_assert(sizeof(decltype(stage_code_enc)) == WIRE_HD_TYPE_SIZE,
                "The stage_code_enc size does not match the storage capacity");

  unsigned long long payload_length_enc = htole64(m_payload_length);
  memcpy(slider, &payload_length_enc, WIRE_HD_PAYLOAD_LEN_SIZE);
  slider += WIRE_HD_PAYLOAD_LEN_SIZE;
  static_assert(
      sizeof(decltype(m_payload_length)) == WIRE_HD_PAYLOAD_LEN_SIZE,
      "The old_payload_length size does not match the storage capacity");
  static_assert(sizeof(decltype(m_payload_length)) ==
                    sizeof(decltype(payload_length_enc)),
                "The old_payload_length size is not equal to "
                "old_payload_length_enc size");

  return slider - buffer;
}

unsigned long long Gcs_dynamic_header::decode(const unsigned char *buffer) {
  const unsigned char *slider = buffer;
  unsigned int stage_code_enc;

  memcpy(&m_dynamic_header_length, slider, WIRE_HD_LEN_SIZE);
  m_dynamic_header_length = le16toh(m_dynamic_header_length);
  slider += WIRE_HD_LEN_SIZE;
  static_assert(sizeof(decltype(m_dynamic_header_length)) == WIRE_HD_LEN_SIZE,
                "The header_length size does not match the storage capacity");

  memcpy(&stage_code_enc, slider, WIRE_HD_TYPE_SIZE);
  stage_code_enc = le32toh(stage_code_enc);
  m_stage_code = static_cast<Stage_code>(stage_code_enc);
  slider += WIRE_HD_TYPE_SIZE;
  static_assert(sizeof(decltype(stage_code_enc)) == WIRE_HD_TYPE_SIZE,
                "The type_code_enc size does not match the storage capacity");

  memcpy(&m_payload_length, slider, WIRE_HD_PAYLOAD_LEN_SIZE);
  m_payload_length = le64toh(m_payload_length);
  static_assert(
      sizeof(decltype(m_payload_length)) == WIRE_HD_PAYLOAD_LEN_SIZE,
      "The old_payload_length size does not match the storage capacity");
  slider += WIRE_HD_PAYLOAD_LEN_SIZE;

  return slider - buffer;
}

void Gcs_dynamic_header::dump(std::ostringstream &output) const {
  output << "dynamic header<header length=(" << get_dynamic_header_length()
         << "), stage code=(" << static_cast<unsigned short>(get_stage_code())
         << "), payload length=(" << get_payload_length() << ")> ";
}

Gcs_stage_metadata::~Gcs_stage_metadata() = default;

std::unique_ptr<Gcs_stage_metadata> Gcs_empty_stage_metadata::clone() {
  return std::make_unique<Gcs_empty_stage_metadata>(*this);
}

unsigned long long Gcs_empty_stage_metadata::calculate_encode_length() const {
  return 0;
}

unsigned long long Gcs_empty_stage_metadata::encode(unsigned char *) const {
  return 0;
}

unsigned long long Gcs_empty_stage_metadata::decode(unsigned char const *) {
  return 0;
}

void Gcs_empty_stage_metadata::dump(std::ostringstream &) const {}
