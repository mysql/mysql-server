/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/plugin_messages/sync_before_execution_message.h"
#include "my_dbug.h"
#include "plugin/group_replication/include/plugin_handlers/metrics_handler.h"

Sync_before_execution_message::Sync_before_execution_message(
    my_thread_id thread_id)
    : Plugin_gcs_message(CT_SYNC_BEFORE_EXECUTION_MESSAGE),
      m_thread_id(thread_id) {}

Sync_before_execution_message::Sync_before_execution_message(
    const unsigned char *buf, size_t len)
    : Plugin_gcs_message(CT_SYNC_BEFORE_EXECUTION_MESSAGE), m_thread_id(0) {
  decode(buf, len);
}

Sync_before_execution_message::~Sync_before_execution_message() = default;

void Sync_before_execution_message::encode_payload(
    std::vector<unsigned char> *buffer) const {
  DBUG_TRACE;

  uint32 thread_id_aux = static_cast<uint32>(m_thread_id);
  encode_payload_item_int4(buffer, PIT_MY_THREAD_ID, thread_id_aux);

  encode_payload_item_int8(buffer, PIT_SENT_TIMESTAMP,
                           Metrics_handler::get_current_time());
}

void Sync_before_execution_message::decode_payload(const unsigned char *buffer,
                                                   const unsigned char *) {
  DBUG_TRACE;
  const unsigned char *slider = buffer;
  uint16 payload_item_type = 0;

  uint32 thread_id_aux = 0;
  decode_payload_item_int4(&slider, &payload_item_type, &thread_id_aux);
  m_thread_id = static_cast<my_thread_id>(thread_id_aux);
}

my_thread_id Sync_before_execution_message::get_thread_id() {
  return m_thread_id;
}

uint64_t Sync_before_execution_message::get_sent_timestamp(
    const unsigned char *buffer, size_t length) {
  DBUG_TRACE;
  return Plugin_gcs_message::get_sent_timestamp(buffer, length,
                                                PIT_SENT_TIMESTAMP);
}
