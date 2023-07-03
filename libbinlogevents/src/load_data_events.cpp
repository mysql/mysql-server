<<<<<<< HEAD
/* Copyright (c) 2014, 2022, Oracle and/or its affiliates.
=======
/* Copyright (c) 2014, 2023, Oracle and/or its affiliates.
>>>>>>> pr/231

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
  READER_TRY_SET(file_id, read<int32_t>);
  READER_TRY_SET(fn_pos_start, read<uint32_t>);
  READER_TRY_SET(fn_pos_end, read<uint32_t>);
  READER_TRY_SET(dup_temp, read<uint8_t>);
  dup_handling = (enum_load_dup_handling)(dup_temp);

  /* Sanity check */
  if (fn_pos_start > q_len || fn_pos_end > q_len ||
      dup_handling > LOAD_DUP_REPLACE)
    READER_THROW("Invalid Execute_load_query_event.");

  READER_CATCH_ERROR;
  BAPI_VOID_RETURN;
}

<<<<<<< HEAD
Delete_file_event::Delete_file_event(const char *buf,
                                     const Format_description_event *fde)
    : Binary_log_event(&buf, fde), file_id(0) {
  BAPI_ENTER(
      "Delete_file_event::Delete_file_event(const char*, const "
      "Format_description_event*)");
  READER_TRY_INITIALIZATION;

  READER_ASSERT_POSITION(fde->common_header_len);
  READER_TRY_SET(file_id, read<uint32_t>);
  if (file_id == 0) READER_THROW("Invalid file_id");

  READER_CATCH_ERROR;
  BAPI_VOID_RETURN;
=======
<<<<<<< HEAD
=======

/**
  This method initializes the members of strcuture variable sql_ex_data_info,
  defined in a Load_event. The structure stores the data about processing
  the file loaded into tables using LOAD_DATA_INFILE, which is optionally
  specified in the LOAD_DATA_INFILE query.

  @param buf Buffer contained in the following format
      <pre>
      +-----------------------------------------------------------------------+
      |field_term_len|field_term|enclosed_len|enclosed|line_term_len|line_term|
      +-----------------------------------------------------------------------+
      +------------------------------------------------------------------+
      |line_start_len|line_start|escaped_len|escaped|opt_flags|empty_flag|
      +------------------------------------------------------------------+
      </pre>
  @param buf_end        pointer after the empty flag bitfield
  @param use_new_format flag indicating whther the new format is to be forced
  @return               the pointer to the first byte after the sql_ex
                        structure, which is the start of field lengths array.
*/
const char *binary_log::
sql_ex_data_info::init(const char *buf,
                       const char *buf_end, bool use_new_format)
{
  cached_new_format= use_new_format;
  if (use_new_format)
  {
    empty_flags= 0;
    /*
      The code below assumes that buf will not disappear from
      under our feet during the lifetime of the event. This assumption
      holds true in the slave thread if the log is in new format, but is not
      the case when we have old format because we will be reusing net buffer
      to read the actual file before we write out the Create_file event.
    */
    if (read_str_at_most_255_bytes(&buf, buf_end,
                                   &field_term, &field_term_len) ||
        read_str_at_most_255_bytes(&buf, buf_end,
                                   &enclosed,   &enclosed_len) ||
        read_str_at_most_255_bytes(&buf, buf_end,
                                   &line_term,  &line_term_len) ||
        read_str_at_most_255_bytes(&buf, buf_end,
                                   &line_start, &line_start_len) ||
        read_str_at_most_255_bytes(&buf, buf_end, &escaped, &escaped_len))
      return 0;
    opt_flags= *buf++;
  }
  else
  {
    /* For the old struct, only single character terminators are allowed */
    field_term_len= enclosed_len= line_term_len= line_start_len= escaped_len= 1;
    field_term=  buf++;                        // Use first byte in string
    enclosed=    buf++;
    line_term=   buf++;
    line_start=  buf++;
    escaped=     buf++;
    opt_flags =  *buf++;
    empty_flags= *buf++;
    if (empty_flags & FIELD_TERM_EMPTY)
      field_term_len= 0;
    if (empty_flags & ENCLOSED_EMPTY)
      enclosed_len= 0;
    if (empty_flags & LINE_TERM_EMPTY)
      line_term_len= 0;
    if (empty_flags & LINE_START_EMPTY)
      line_start_len= 0;
    if (empty_flags & ESCAPED_EMPTY)
      escaped_len= 0;
  }
  return buf;
}

/**
  @note
    The caller must do buf[event_len] = 0 before he starts using the
    constructed event.
*/
Load_event::Load_event(const char *buf, unsigned int event_len,
                       const Format_description_event *description_event)
  :Binary_log_event(&buf, description_event->binlog_version,
                    description_event->server_version), num_fields(0),
   fields(0), field_lens(0),field_block_len(0),
   table_name(0), db(0), fname(0), local_fname(false),
   /*
     Load_event which comes from the binary log does not contain
     information about the type of insert which was used on the master.
     Assume that it was an ordinary, non-concurrent LOAD DATA.
   */
   is_concurrent(false)
{
  //buf is advanced in Binary_log_event constructor to point to
  //beginning of post-header
  if (event_len)
    copy_load_event(buf, event_len,
                   ((header()->type_code == LOAD_EVENT) ?
                   LOAD_HEADER_LEN +
                   description_event->common_header_len :
                   LOAD_HEADER_LEN + LOG_EVENT_HEADER_LEN),
                   description_event);
  /* otherwise it's a derived class, will call copy_load_event() itself */
}

/**
  Load_event::copy_load_event()
  This fucntion decode the Load_event, and is called from from within the
  constructor Load event. This is moved out of the constructor since
  reconstructing the load event is required while decding create_file_event.

  @param buf Event         common header + data for Load_event
  @param event_len         Length of fixed + variable part of even data
  @param body_offset       Length indicating starting of variable
                           data part in buf
  @param description_event FDE read from the same binary log file

  @retval 0 on success
  @retval 1 on failure
*/
int Load_event::copy_load_event(const char *buf, unsigned long event_len,
                                int body_offset, const Format_description_event
                                *description_event)
{
  /**
  <pre>
    Fixed data part
  +---------------------------------------------------------------------------+
  |thread_id|exec_time|no. of lines to skip|tb_name_len |db_name_len|col_count|
  +---------------------------------------------------------------------------+
  </pre>
  */

  unsigned int data_len;
  char* buf_end = (char*)buf + (event_len - description_event->common_header_len);
  /* this is the beginning of the post-header */
  const char* data_head = buf;

  memcpy(&slave_proxy_id, data_head + L_THREAD_ID_OFFSET,
         sizeof(slave_proxy_id));
  slave_proxy_id= le32toh(slave_proxy_id);

  memcpy(&load_exec_time, data_head + L_EXEC_TIME_OFFSET,
         sizeof(load_exec_time));
  load_exec_time= le32toh(load_exec_time);

  memcpy(&skip_lines, data_head + L_SKIP_LINES_OFFSET, sizeof(skip_lines));
  skip_lines= le32toh(skip_lines);

  table_name_len = (unsigned int)data_head[L_TBL_LEN_OFFSET];
  db_len = (unsigned int)data_head[L_DB_LEN_OFFSET];

  memcpy(&num_fields, data_head + L_NUM_FIELDS_OFFSET, sizeof(num_fields));
  num_fields= le32toh(num_fields);

  /**
    <pre>
    Variable data part
  +---------------------------------------------------------------------------+
  |sql_ex_data struct|len of col names to load|col_names|tb_name|db_name|fname|
  +---------------------------------------------------------------------------+
    </pre>
  */
  if ((int) event_len < body_offset)
    return 1;
  /*
    Sql_ex_data.init() on success returns the pointer to the first byte after
    the sql_ex structure, which is the start of field lengths array.
  */
  if (!(field_lens=
          reinterpret_cast<unsigned char*>(const_cast<char*>(sql_ex_data.init(
                                           const_cast<char*>(buf) +
                                           body_offset -
                                           description_event->common_header_len,
                                           buf_end,
                                           buf[EVENT_TYPE_OFFSET] != LOAD_EVENT)))))
    return 1;

  data_len = event_len - body_offset;
  if (num_fields > data_len) // simple sanity check against corruption
    return 1;
  for (unsigned int i= 0; i < num_fields; i++)
    field_block_len+= (unsigned int)field_lens[i] + 1;

  fields= (char*)field_lens + num_fields;
  table_name= fields + field_block_len;
  if (strlen(table_name) > NAME_LEN)
    goto err;

  db= table_name + table_name_len + 1;

  #ifndef NDEBUG
  /*
    This is specific to mysql test run on the server
    for the keyword "simulate_invalid_address"
  */
  if (binary_log_debug::debug_simulate_invalid_address)
    db_len= data_len;
  #endif

  fname = db + db_len + 1;
  if ((db_len > data_len) || (fname > buf_end))
    goto err;
  fname_len = strlen(fname);
  if ((fname_len > data_len) || (fname + fname_len > buf_end))
    goto err;
  // null termination is accomplished by the caller doing buf[event_len]=0

  return 0;

err:
  // Invalid event.
  table_name = 0;
  return 1;
}


/**
  Create_file_log_event constructor
  This event tells the slave to create a temporary file and fill it with
  a first data block. Later, zero or more APPEND_BLOCK_EVENT events append
  blocks to this temporary file.

  @note The buffer contains fixed data for the corresponding Load_event
        prepended to the data of create file event.
*/
Create_file_event::Create_file_event(const char* buf, unsigned int len,
                                     const Format_description_event*
                                     description_event)
: Load_event(buf, 0, description_event),
  fake_base(0), block(0), inited_from_old(0)
{
  unsigned int block_offset;
  unsigned int header_len= description_event->common_header_len;
  unsigned char load_header_len=
                description_event->post_header_len[LOAD_EVENT - 1];
  unsigned char create_file_header_len=
                description_event->post_header_len[CREATE_FILE_EVENT - 1];
  if (!(event_buf= static_cast<char *>(bapi_memdup(buf, len))) ||
      copy_load_event(event_buf + header_len , len,
                     ((buf[EVENT_TYPE_OFFSET] == LOAD_EVENT) ?
                      load_header_len + header_len :
                      (fake_base ? (header_len + load_header_len) :
                                   (header_len + load_header_len) +
                                   create_file_header_len)),
                     description_event))
    return;
  if (description_event->binlog_version != 1)
  {
    /**
      file_id is the ID for the data file created on the slave.
      This is necessary in case several LOAD DATA INFILE statements occur in
      parallel on the master. In that case, the binary log may contain inter-
      mixed events for the statement. The ID resovles which file the blocks in
      each APPEND_BLOCK_EVENT must be appended, and the file must be loaded or
      deleted by EXEC_LOAD_EVENT or DELETE_FILE_EVENT.
    */
    memcpy(&file_id, buf + header_len + load_header_len + CF_FILE_ID_OFFSET,
           4);
    file_id= le32toh(file_id);

   /**
      @note
      Note that it's ok to use get_data_size() below, because it is computed
      with values we have already read from this event (because we called
      copy_log_event()); we are not using slave's format info to decode
      master's format, we are really using master's format info.
      Anyway, both formats should be identical (except the common_header_len)
      as these Load events are not changed between 4.0 and 5.0 (as logging of
      LOAD DATA INFILE does not use Load_log_event in 5.0).

      The + 1 is for \0 terminating fname
    */
    block_offset= (description_event->common_header_len +
                   Load_event::get_data_size() +
                   create_file_header_len + 1);
    if (len < block_offset)
      return;
    block = (unsigned char*)buf + block_offset;
    block_len = len - block_offset;
  }
  else
  {
    sql_ex_data.force_new_format();
    inited_from_old = 1;
  }
  return;
}


>>>>>>> upstream/cluster-7.6
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
>>>>>>> pr/231
}

Append_block_event::Append_block_event(const char *buf,
                                       const Format_description_event *fde)
    : Binary_log_event(&buf, fde), block(nullptr) {
  BAPI_ENTER(
      "Append_block_event::Append_block_event(const char*, const "
      "Format_description_event*)");
  READER_TRY_INITIALIZATION;
  READER_ASSERT_POSITION(fde->common_header_len);

  READER_TRY_SET(file_id, read<uint32_t>);
  block_len = READER_CALL(available_to_read);
  block = const_cast<unsigned char *>(
      reinterpret_cast<const unsigned char *>(READER_CALL(ptr, block_len)));

  READER_CATCH_ERROR;
  BAPI_VOID_RETURN;
}

Begin_load_query_event::Begin_load_query_event(
    const char *buf, const Format_description_event *fde)
    : Append_block_event(buf, fde) {
  BAPI_ENTER(
      "Begin_load_query_event::Begin_load_query_event(const char*, const "
      "Format_description_event*)");
  BAPI_VOID_RETURN;
}
}  // end namespace binary_log
