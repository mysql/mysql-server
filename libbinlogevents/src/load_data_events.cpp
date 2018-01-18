/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "load_data_events.h"
#include <stdlib.h>
#include <string.h>

namespace binary_log
{

/*
  Execute_load_event constructors
*/
Execute_load_event::
Execute_load_event(const uint32_t file_id_arg, const char* db_arg)
: Binary_log_event(EXEC_LOAD_EVENT),
  file_id(file_id_arg), db(db_arg)
{
}

/**
  The constructor is called by MySQL slave, while applying the events.
*/
Execute_load_query_event::
Execute_load_query_event(uint32_t file_id_arg,
                         uint32_t fn_pos_start_arg,
                         uint32_t fn_pos_end_arg,
                         enum_load_dup_handling dup_arg)
: Query_event(EXECUTE_LOAD_QUERY_EVENT),
  file_id(file_id_arg), fn_pos_start(fn_pos_start_arg),
  fn_pos_end(fn_pos_end_arg), dup_handling(dup_arg)
{
}

/**
  The constructor used inorder to decode EXECUTE_LOAD_QUERY_EVENT from a
  packet. It is used on the MySQL server acting as a slave.
*/
Execute_load_query_event::
Execute_load_query_event(const char* buf,
                         unsigned int event_len,
                         const Format_description_event *description_event)
: Query_event(buf, event_len, description_event,
              EXECUTE_LOAD_QUERY_EVENT),
  file_id(0), fn_pos_start(0), fn_pos_end(0)
{
  if (!query)
    return;

  buf+= description_event->common_header_len;

  memcpy(&fn_pos_start, buf + ELQ_FN_POS_START_OFFSET, sizeof(fn_pos_start));
  fn_pos_start= le32toh(fn_pos_start);
  memcpy(&fn_pos_end, buf + ELQ_FN_POS_END_OFFSET, sizeof(fn_pos_end));
  fn_pos_end= le32toh(fn_pos_end);
  dup_handling= (enum_load_dup_handling)(*(buf + ELQ_DUP_HANDLING_OFFSET));

  if (fn_pos_start > q_len || fn_pos_end > q_len ||
      dup_handling > LOAD_DUP_REPLACE)
    return;

  memcpy(&file_id, buf + ELQ_FILE_ID_OFFSET, 4);
  file_id= le32toh(file_id);
}


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

  #ifndef DBUG_OFF
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


/**
  Delete_file_event constructor
*/
Delete_file_event::Delete_file_event(const char* buf, unsigned int len,
                                     const Format_description_event*
                                     description_event)
: Binary_log_event(&buf, description_event->binlog_version,
                    description_event->server_version), file_id(0)
{
  //buf is advanced in Binary_log_event constructor to point to
  //beginning of post-header
  unsigned char common_header_len= description_event->common_header_len;
  unsigned char delete_file_header_len=
                     description_event->post_header_len[DELETE_FILE_EVENT - 1];
  if (len < (unsigned int)(common_header_len + delete_file_header_len))
    return;
  memcpy(&file_id, buf + DF_FILE_ID_OFFSET, 4);
  file_id= le32toh(file_id);
}

/**
  Execute_load_event constructor
*/
Execute_load_event::Execute_load_event(const char* buf, unsigned int len,
                                       const Format_description_event*
                                       description_event)
  :Binary_log_event(&buf, description_event->binlog_version,
                    description_event->server_version), file_id(0)
{
  //buf is advanced in Binary_log_event constructor to point to
  //beginning of post-header
  unsigned char common_header_len= description_event->common_header_len;
  unsigned char exec_load_header_len= description_event->
                                      post_header_len[EXEC_LOAD_EVENT-1];

  if (len < (unsigned int)(common_header_len + exec_load_header_len))
    return;

  memcpy(&file_id, buf + EL_FILE_ID_OFFSET, 4);
  file_id= le32toh(file_id);
}

/**
  Constructor receives a packet from the MySQL master or the binary
  log, containing a block of data to be appended to the file being loaded via
  LOAD_DATA_INFILE query; and decodes it to create an Append_block_event.
*/
Append_block_event::Append_block_event(const char* buf, unsigned int len,
                                       const Format_description_event*
                                       description_event)
: Binary_log_event(&buf, description_event->binlog_version,
                    description_event->server_version), block(0)
{
  //buf is advanced in Binary_log_event constructor to point to
  //beginning of post-header
  unsigned char common_header_len= description_event->common_header_len;
  unsigned char append_block_header_len=
    description_event->post_header_len[APPEND_BLOCK_EVENT - 1];
  unsigned int total_header_len= common_header_len + append_block_header_len;
  if (len < total_header_len)
    return;

  memcpy(&file_id, buf + AB_FILE_ID_OFFSET, 4);
  file_id= le32toh(file_id);

  block= (unsigned char*)buf + append_block_header_len;
  block_len= len - total_header_len;
}


Begin_load_query_event::
Begin_load_query_event(const char* buf, unsigned int len,
                       const Format_description_event* desc_event)
: Append_block_event(buf, len, desc_event)
{
}
} // end namespace binary_log
