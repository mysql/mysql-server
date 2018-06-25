/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/include/gcs_plugin_messages.h"

#include "my_byteorder.h"
#include "my_dbug.h"

const int Plugin_gcs_message::PLUGIN_GCS_MESSAGE_VERSION = 1;

const unsigned int Plugin_gcs_message::WIRE_VERSION_SIZE = 4;
const unsigned int Plugin_gcs_message::WIRE_HD_LEN_SIZE = 2;
const unsigned int Plugin_gcs_message::WIRE_MSG_LEN_SIZE = 8;
const unsigned int Plugin_gcs_message::WIRE_CARGO_TYPE_SIZE = 2;

const unsigned int Plugin_gcs_message::WIRE_FIXED_HEADER_SIZE =
    Plugin_gcs_message::WIRE_VERSION_SIZE +
    Plugin_gcs_message::WIRE_HD_LEN_SIZE +
    Plugin_gcs_message::WIRE_MSG_LEN_SIZE +
    Plugin_gcs_message::WIRE_CARGO_TYPE_SIZE;

const unsigned int Plugin_gcs_message::WIRE_PAYLOAD_ITEM_TYPE_SIZE = 2;

const unsigned int Plugin_gcs_message::WIRE_PAYLOAD_ITEM_LEN_SIZE = 8;

const unsigned int Plugin_gcs_message::WIRE_PAYLOAD_ITEM_HEADER_SIZE =
    Plugin_gcs_message::WIRE_PAYLOAD_ITEM_TYPE_SIZE +
    Plugin_gcs_message::WIRE_PAYLOAD_ITEM_LEN_SIZE;

Plugin_gcs_message::Plugin_gcs_message(enum_cargo_type cargo_type)
    : m_version(PLUGIN_GCS_MESSAGE_VERSION),
      m_fixed_header_len(WIRE_FIXED_HEADER_SIZE),
      m_msg_len(WIRE_FIXED_HEADER_SIZE),
      m_cargo_type(cargo_type) {}

void Plugin_gcs_message::encode(std::vector<unsigned char> *buffer) const {
  DBUG_ENTER("Plugin_gcs_message::encode");
  unsigned char buf[WIRE_FIXED_HEADER_SIZE];
  unsigned char *slider = buf;

  int4store(slider, m_version);
  slider += WIRE_VERSION_SIZE;

  int2store(slider, m_fixed_header_len);
  slider += WIRE_HD_LEN_SIZE;

  int8store(slider, m_msg_len);
  slider += WIRE_MSG_LEN_SIZE;

  unsigned short s_cargo_type = (unsigned short)m_cargo_type;
  int2store(slider, s_cargo_type);
  slider += WIRE_CARGO_TYPE_SIZE;

  buffer->insert(buffer->end(), buf, buf + WIRE_FIXED_HEADER_SIZE);

  encode_payload(buffer);

  DBUG_VOID_RETURN;
}

void Plugin_gcs_message::decode(const unsigned char *buffer, size_t length) {
  DBUG_ENTER("Plugin_gcs_message::decode");
  const unsigned char *slider = buffer;
  const unsigned char *end = buffer + length;

  m_version = uint4korr(slider);
  slider += WIRE_VERSION_SIZE;

  m_fixed_header_len = uint2korr(slider);
  slider += WIRE_HD_LEN_SIZE;

  m_msg_len = uint8korr(slider);
  slider += WIRE_MSG_LEN_SIZE;

  unsigned short s_cargo_type = 0;
  s_cargo_type = uint2korr(slider);
  // enum may have 32bit storage
  m_cargo_type = (Plugin_gcs_message::enum_cargo_type)s_cargo_type;
  slider += WIRE_CARGO_TYPE_SIZE;

  decode_payload(slider, end);

  DBUG_VOID_RETURN;
}

Plugin_gcs_message::enum_cargo_type Plugin_gcs_message::get_cargo_type(
    const unsigned char *buffer) {
  DBUG_ENTER("Plugin_gcs_message::decode");
  const unsigned char *slider =
      buffer + WIRE_VERSION_SIZE + WIRE_HD_LEN_SIZE + WIRE_MSG_LEN_SIZE;

  unsigned short s_cargo_type = 0;
  s_cargo_type = uint2korr(slider);
  // enum may have 32bit storage
  Plugin_gcs_message::enum_cargo_type cargo_type =
      (Plugin_gcs_message::enum_cargo_type)s_cargo_type;

  DBUG_RETURN(cargo_type);
}

void Plugin_gcs_message::get_first_payload_item_raw_data(
    const unsigned char *buffer, const unsigned char **payload_item_data,
    size_t *payload_item_length) {
  DBUG_ENTER("Plugin_gcs_message::get_first_payload_item_raw_data");
  const unsigned char *slider =
      buffer + WIRE_FIXED_HEADER_SIZE + WIRE_PAYLOAD_ITEM_TYPE_SIZE;

  *payload_item_length = uint8korr(slider);
  slider += WIRE_PAYLOAD_ITEM_LEN_SIZE;
  *payload_item_data = slider;

  DBUG_VOID_RETURN;
}

void Plugin_gcs_message::encode_payload_item_type_and_length(
    std::vector<unsigned char> *buffer, uint16 payload_item_type,
    unsigned long long payload_item_length) const {
  DBUG_ENTER("Plugin_gcs_message::encode_payload_item_type_and_length");
  unsigned char buf[WIRE_PAYLOAD_ITEM_HEADER_SIZE];
  unsigned char *slider = buf;

  int2store(slider, payload_item_type);
  slider += WIRE_PAYLOAD_ITEM_TYPE_SIZE;

  int8store(slider, payload_item_length);
  slider += WIRE_PAYLOAD_ITEM_LEN_SIZE;

  buffer->insert(buffer->end(), buf, buf + WIRE_PAYLOAD_ITEM_HEADER_SIZE);

  DBUG_VOID_RETURN;
}

void Plugin_gcs_message::decode_payload_item_type_and_length(
    const unsigned char **buffer, uint16 *payload_item_type,
    unsigned long long *payload_item_length) {
  DBUG_ENTER("Plugin_gcs_message::decode_payload_item_type_and_length");

  *payload_item_type = uint2korr(*buffer);
  *buffer += WIRE_PAYLOAD_ITEM_TYPE_SIZE;

  *payload_item_length = uint8korr(*buffer);
  *buffer += WIRE_PAYLOAD_ITEM_LEN_SIZE;

  DBUG_VOID_RETURN;
}

void Plugin_gcs_message::encode_payload_item_char(
    std::vector<unsigned char> *buffer, uint16 type,
    unsigned char value) const {
  DBUG_ENTER("Plugin_gcs_message::encode_payload_item_char");
  unsigned char buf[1];

  encode_payload_item_type_and_length(buffer, type, 1);
  buf[0] = value;
  buffer->insert(buffer->end(), buf, buf + 1);

  DBUG_VOID_RETURN;
}

void Plugin_gcs_message::decode_payload_item_char(const unsigned char **buffer,
                                                  uint16 *type,
                                                  unsigned char *value) {
  DBUG_ENTER("Plugin_gcs_message::decode_payload_item_char");

  unsigned long long length = 0;
  decode_payload_item_type_and_length(buffer, type, &length);
  *value = **buffer;
  *buffer += 1;

  DBUG_VOID_RETURN;
}

void Plugin_gcs_message::encode_payload_item_int2(
    std::vector<unsigned char> *buffer, uint16 type, uint16 value) const {
  DBUG_ENTER("Plugin_gcs_message::encode_payload_item_int2");
  unsigned char buf[2];

  encode_payload_item_type_and_length(buffer, type, 2);
  int2store(buf, value);
  buffer->insert(buffer->end(), buf, buf + 2);

  DBUG_VOID_RETURN;
}

void Plugin_gcs_message::decode_payload_item_int2(const unsigned char **buffer,
                                                  uint16 *type, uint16 *value) {
  DBUG_ENTER("Plugin_gcs_message::decode_payload_item_int2");

  unsigned long long length = 0;
  decode_payload_item_type_and_length(buffer, type, &length);
  *value = uint2korr(*buffer);
  *buffer += 2;

  DBUG_VOID_RETURN;
}

void Plugin_gcs_message::encode_payload_item_int4(
    std::vector<unsigned char> *buffer, uint16 type, uint32 value) const {
  DBUG_ENTER("Plugin_gcs_message::encode_payload_item_int4");
  unsigned char buf[4];

  encode_payload_item_type_and_length(buffer, type, 4);
  int4store(buf, value);
  buffer->insert(buffer->end(), buf, buf + 4);

  DBUG_VOID_RETURN;
}

void Plugin_gcs_message::decode_payload_item_int4(const unsigned char **buffer,
                                                  uint16 *type, uint32 *value) {
  DBUG_ENTER("Plugin_gcs_message::decode_payload_item_int4");

  unsigned long long length = 0;
  decode_payload_item_type_and_length(buffer, type, &length);
  *value = uint4korr(*buffer);
  *buffer += 4;

  DBUG_VOID_RETURN;
}

void Plugin_gcs_message::encode_payload_item_int8(
    std::vector<unsigned char> *buffer, uint16 type, ulonglong value) const {
  DBUG_ENTER("Plugin_gcs_message::encode_payload_item_int8");
  unsigned char buf[8];

  encode_payload_item_type_and_length(buffer, type, 8);
  int8store(buf, value);
  buffer->insert(buffer->end(), buf, buf + 8);

  DBUG_VOID_RETURN;
}

void Plugin_gcs_message::decode_payload_item_int8(const unsigned char **buffer,
                                                  uint16 *type,
                                                  ulonglong *value) {
  DBUG_ENTER("Plugin_gcs_message::decode_payload_item_int8");

  unsigned long long length = 0;
  decode_payload_item_type_and_length(buffer, type, &length);
  *value = uint8korr(*buffer);
  *buffer += 8;

  DBUG_VOID_RETURN;
}

void Plugin_gcs_message::encode_payload_item_string(
    std::vector<unsigned char> *buffer, uint16 type, const char *value,
    unsigned long long length) const {
  DBUG_ENTER("Plugin_gcs_message::encode_payload_item_string");

  encode_payload_item_type_and_length(buffer, type, length);
  buffer->insert(buffer->end(), value, value + length);

  DBUG_VOID_RETURN;
}

void Plugin_gcs_message::decode_payload_item_string(
    const unsigned char **buffer, uint16 *type, std::string *value,
    unsigned long long *length) {
  DBUG_ENTER("Plugin_gcs_message::decode_payload_item_string");

  decode_payload_item_type_and_length(buffer, type, length);
  value->assign(reinterpret_cast<const char *>(*buffer), (size_t)*length);
  *buffer += *length;

  DBUG_VOID_RETURN;
}