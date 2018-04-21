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

#include "load_data_events.h"
#include <stdlib.h>
#include <string.h>
#include "event_reader_macros.h"

namespace binary_log {
/**
  The constructor is called by MySQL slave, while applying the events.
*/
Execute_load_query_event::Execute_load_query_event(
    uint32_t file_id_arg, uint32_t fn_pos_start_arg, uint32_t fn_pos_end_arg,
    enum_load_dup_handling dup_arg)
    : Query_event(EXECUTE_LOAD_QUERY_EVENT),
      file_id(file_id_arg),
      fn_pos_start(fn_pos_start_arg),
      fn_pos_end(fn_pos_end_arg),
      dup_handling(dup_arg) {}

/**
  The constructor used in order to decode EXECUTE_LOAD_QUERY_EVENT from a
  packet. It is used on the MySQL server acting as a slave.
*/
Execute_load_query_event::Execute_load_query_event(
    const char *buf, const Format_description_event *fde)
    : Query_event(buf, fde, EXECUTE_LOAD_QUERY_EVENT),
      file_id(0),
      fn_pos_start(0),
      fn_pos_end(0) {
  uint8_t dup_temp;
  BAPI_ENTER(
      "Execute_load_query_event::Execute_load_query_event(const char*, const "
      "Format_description_event*)");
  READER_TRY_INITIALIZATION;

  READER_TRY_CALL(go_to, fde->common_header_len);
  READER_TRY_CALL(forward, ELQ_FILE_ID_OFFSET);
  READER_TRY_SET(file_id, read_and_letoh<int32_t>);
  READER_TRY_SET(fn_pos_start, read_and_letoh<uint32_t>);
  READER_TRY_SET(fn_pos_end, read_and_letoh<uint32_t>);
  READER_TRY_SET(dup_temp, read<uint8_t>);
  dup_handling = (enum_load_dup_handling)(dup_temp);

  /* Sanity check */
  if (fn_pos_start > q_len || fn_pos_end > q_len ||
      dup_handling > LOAD_DUP_REPLACE)
    READER_THROW("Invalid Execute_load_query_event.");

  READER_CATCH_ERROR;
  BAPI_VOID_RETURN;
}

/**
  Delete_file_event constructor
*/
Delete_file_event::Delete_file_event(
    const char *buf, unsigned int len,
    const Format_description_event *description_event)
    : Binary_log_event(&buf, description_event->binlog_version), file_id(0) {
  // buf is advanced in Binary_log_event constructor to point to
  // beginning of post-header
  unsigned char common_header_len = description_event->common_header_len;
  unsigned char delete_file_header_len =
      description_event->post_header_len[DELETE_FILE_EVENT - 1];
  if (len < (unsigned int)(common_header_len + delete_file_header_len)) return;
  memcpy(&file_id, buf + DF_FILE_ID_OFFSET, 4);
  file_id = le32toh(file_id);
}

/**
  Constructor receives a packet from the MySQL master or the binary
  log, containing a block of data to be appended to the file being loaded via
  LOAD_DATA_INFILE query; and decodes it to create an Append_block_event.
*/
Append_block_event::Append_block_event(
    const char *buf, unsigned int len,
    const Format_description_event *description_event)
    : Binary_log_event(&buf, description_event->binlog_version), block(0) {
  // buf is advanced in Binary_log_event constructor to point to
  // beginning of post-header
  unsigned char common_header_len = description_event->common_header_len;
  unsigned char append_block_header_len =
      description_event->post_header_len[APPEND_BLOCK_EVENT - 1];
  unsigned int total_header_len = common_header_len + append_block_header_len;
  if (len < total_header_len) return;

  memcpy(&file_id, buf + AB_FILE_ID_OFFSET, 4);
  file_id = le32toh(file_id);

  block = (unsigned char *)buf + append_block_header_len;
  block_len = len - total_header_len;
}

Begin_load_query_event::Begin_load_query_event(
    const char *buf, unsigned int len,
    const Format_description_event *desc_event)
    : Append_block_event(buf, len, desc_event) {}
}  // end namespace binary_log
