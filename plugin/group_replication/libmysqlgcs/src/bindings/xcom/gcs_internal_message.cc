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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_internal_message.h"

#include <stdlib.h>
#include <string.h>
#include <string>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/byteorder.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stages.h"

const int Gcs_internal_message_header::GCS_PROTO_VERSION = 1;

const unsigned short Gcs_internal_message_header::WIRE_VERSION_SIZE = 4;
const unsigned short Gcs_internal_message_header::WIRE_HD_LEN_SIZE = 2;
const unsigned short Gcs_internal_message_header::WIRE_MSG_LEN_SIZE = 8;
const unsigned short Gcs_internal_message_header::WIRE_DYNAMIC_HDRS_LEN_SIZE =
    4;
const unsigned short Gcs_internal_message_header::WIRE_CARGO_TYPE_SIZE = 2;

const unsigned short Gcs_internal_message_header::WIRE_MSG_LEN_OFFSET =
    Gcs_internal_message_header::WIRE_VERSION_SIZE +
    Gcs_internal_message_header::WIRE_HD_LEN_SIZE;

const unsigned short Gcs_internal_message_header::WIRE_DYNAMIC_HDRS_LEN_OFFSET =
    Gcs_internal_message_header::WIRE_VERSION_SIZE +
    Gcs_internal_message_header::WIRE_HD_LEN_SIZE +
    Gcs_internal_message_header::WIRE_MSG_LEN_SIZE;

const unsigned short Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE =
    Gcs_internal_message_header::WIRE_VERSION_SIZE +
    Gcs_internal_message_header::WIRE_HD_LEN_SIZE +
    Gcs_internal_message_header::WIRE_MSG_LEN_SIZE +
    Gcs_internal_message_header::WIRE_DYNAMIC_HDRS_LEN_SIZE +
    Gcs_internal_message_header::WIRE_CARGO_TYPE_SIZE;

const unsigned short Gcs_packet::BLOCK_SIZE = 1024;

void Gcs_packet::reload_header(Gcs_internal_message_header &hd) {
  m_length = hd.get_msg_length();
  m_payload_len = hd.get_msg_length() - hd.get_header_length();
  m_header_len = hd.get_header_length();
  m_dyn_headers_len = hd.get_dynamic_headers_length();
}

bool Gcs_internal_message_header::encode(unsigned char *buffer) {
  unsigned char *slider = buffer;
  unsigned short s_cargo_type = static_cast<unsigned short>(m_cargo_type);

  unsigned int le_version = htole32(m_version);
  memcpy(slider, &le_version, WIRE_VERSION_SIZE);
  slider += WIRE_VERSION_SIZE;

  unsigned short le_hdr_len = htole16(m_fixed_header_len);
  memcpy(slider, &le_hdr_len, WIRE_HD_LEN_SIZE);
  slider += WIRE_HD_LEN_SIZE;

  unsigned long long le_total_len = htole64(m_msg_len);
  memcpy(slider, &le_total_len, WIRE_MSG_LEN_SIZE);
  slider += WIRE_MSG_LEN_SIZE;

  unsigned int le_dyn_len = htole32(m_dynamic_headers_len);
  memcpy(slider, &le_dyn_len, WIRE_DYNAMIC_HDRS_LEN_SIZE);
  slider += WIRE_DYNAMIC_HDRS_LEN_SIZE;

  unsigned short le_ct = htole16(s_cargo_type);
  memcpy(slider, &le_ct, WIRE_CARGO_TYPE_SIZE);
  slider += WIRE_CARGO_TYPE_SIZE;

  return false;
}

bool Gcs_internal_message_header::decode(const unsigned char *buffer) {
  const unsigned char *slider = buffer;
  unsigned short s_cargo_type = 0;

  memcpy(&m_version, slider, WIRE_VERSION_SIZE);
  m_version = le32toh(m_version);
  slider += WIRE_VERSION_SIZE;

  memcpy(&m_fixed_header_len, slider, WIRE_HD_LEN_SIZE);
  m_fixed_header_len = le16toh(m_fixed_header_len);
  slider += WIRE_HD_LEN_SIZE;

  memcpy(&m_msg_len, slider, WIRE_MSG_LEN_SIZE);
  m_msg_len = le64toh(m_msg_len);
  slider += WIRE_MSG_LEN_SIZE;

  memcpy(&m_dynamic_headers_len, slider, WIRE_DYNAMIC_HDRS_LEN_SIZE);
  m_dynamic_headers_len = le32toh(m_dynamic_headers_len);
  slider += WIRE_DYNAMIC_HDRS_LEN_SIZE;

  memcpy(&s_cargo_type, slider, WIRE_CARGO_TYPE_SIZE);
  s_cargo_type = le16toh(s_cargo_type);

  // enum may have 32bit storage
  m_cargo_type = (Gcs_internal_message_header::enum_cargo_type)s_cargo_type;
  slider += WIRE_CARGO_TYPE_SIZE;

  return false;
}
