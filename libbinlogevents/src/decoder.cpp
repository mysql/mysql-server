/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "decoder.h"
#include <cassert>
#include <iostream>

namespace binary_log
{
enum_binlog_checksum_alg Decoder::checksum_algorithm(const char *buf,
                                                     unsigned int event_type,
                                                     size_t event_len)
{
  enum_binlog_checksum_alg alg;
  if (event_type != FORMAT_DESCRIPTION_EVENT)
    alg= fd_ev->footer()->checksum_alg;
  else
    alg= Log_event_footer::get_checksum_alg(buf, event_len);

  return alg;
}

void Decoder::set_format_description_event(Format_description_event *fd_ev_arg)
{
  fd_ev= fd_ev_arg;
}

Format_description_event * Decoder::get_format_description_event()
{
  return fd_ev;
}
Binary_log_event* Decoder::decode_event(const char* buf, size_t event_len,
                                        const char **error,
                                        bool crc_check)
{
  Binary_log_event* ev;
  enum_binlog_checksum_alg alg;
  assert(fd_ev != 0);
  //DBUG_DUMP("data", (unsigned char*) buf, event_len);

  /* Check the integrity */
  uint32_t temp_event_len;
  memcpy(&temp_event_len, buf + EVENT_LEN_OFFSET, 4);
  temp_event_len= le32toh(temp_event_len);
  if (event_len < EVENT_LEN_OFFSET ||
      buf[EVENT_TYPE_OFFSET] >= ENUM_END_EVENT ||
      (unsigned int) event_len != temp_event_len)
  {
    *error= "Sanity check failed";
    return NULL; // general sanity check - will fail on a partial read
  }

  unsigned int event_type= buf[EVENT_TYPE_OFFSET];
  /*
    If event is FD the checksum descriptor is in it.
  */
  alg= checksum_algorithm(buf, event_type, event_len);
  // Emulate the corruption during reading an event
 /* DBUG_EXECUTE_IF("corrupt_read_log_event_char",
    if (event_type != FORMAT_DESCRIPTION_EVENT)
    {
      char *debug_event_buf_c = (char *)buf;
      int debug_cor_pos = rand() % (event_len - BINLOG_CHECKSUM_LEN);
      debug_event_buf_c[debug_cor_pos] =~ debug_event_buf_c[debug_cor_pos];
    }
  );*/

  if (crc_check &&
      Log_event_footer::event_checksum_test((unsigned char *) buf, event_len, alg))
  {
    *error= "Event crc check failed! Most likely there is event corruption.";
    if (force_read)
    {
      // The user can skip this event, and move to next event.
      ev= new Unknown_event(buf, fd_ev);
      return ev;
    }
    return NULL;
  }

  if (event_type > fd_ev->number_of_event_types &&
      event_type != FORMAT_DESCRIPTION_EVENT)
  {
    /*
      It is unsafe to use the description_event if its post_header_len
      array does not include the event type.
    */
    ev= NULL;
  }
  else
  {
    /*
      In some previuos versions (see comment in
      Format_description_event::Format_description_event(char*,...)),
      event types were assigned different id numbers than in the
      present version. In order to replicate from such versions to the
      present version, we must map those event type id's to our event
      type id's.  The mapping is done with the event_type_permutation
      array, which was set up when the Format_description_event
      was read.
    */
    if (fd_ev->event_type_permutation)
    {
      int new_event_type= fd_ev->event_type_permutation[event_type];
      event_type= new_event_type;
    }

    if (alg != BINLOG_CHECKSUM_ALG_UNDEF &&
        (event_type == FORMAT_DESCRIPTION_EVENT ||
         alg != BINLOG_CHECKSUM_ALG_OFF))
      event_len= event_len - BINLOG_CHECKSUM_LEN;

    switch(event_type) {
    case QUERY_EVENT:
      ev  = new Query_event(buf, event_len, fd_ev, QUERY_EVENT);
      break;
    case LOAD_EVENT:
    case NEW_LOAD_EVENT:
      ev= new Load_event(buf, event_len, fd_ev);
      break;
    case ROTATE_EVENT:
      ev = new Rotate_event(buf, event_len, fd_ev);
      break;
    case CREATE_FILE_EVENT:
      ev = new Create_file_event(buf, event_len, fd_ev);
      break;
    case APPEND_BLOCK_EVENT:
      ev = new Append_block_event(buf, event_len, fd_ev);
      break;
    case DELETE_FILE_EVENT:
      ev = new Delete_file_event(buf, event_len, fd_ev);
      break;
    case EXEC_LOAD_EVENT:
      ev = new Execute_load_event(buf, event_len, fd_ev);
      break;
    case START_EVENT_V3: /* this is sent only by MySQL <=4.x */
      ev = new Start_event_v3(buf, fd_ev);
      break;
    case STOP_EVENT:
      ev = new Stop_event(buf, fd_ev);
      break;
    case INTVAR_EVENT:
      ev = new Intvar_event(buf, fd_ev);
      break;
    case XID_EVENT:
      ev = new Xid_event(buf, fd_ev);
      break;
    case RAND_EVENT:
      ev = new Rand_event(buf, fd_ev);
      break;
    case USER_VAR_EVENT:
      ev = new User_var_event(buf, event_len, fd_ev);
      break;
    case FORMAT_DESCRIPTION_EVENT:
      ev = new Format_description_event(buf, event_len, fd_ev);
      break;
    case WRITE_ROWS_EVENT_V1:
      ev = new Write_rows_event(buf, event_len, fd_ev);
      break;
    case UPDATE_ROWS_EVENT_V1:
      ev = new Update_rows_event(buf, event_len, fd_ev);
      break;
    case DELETE_ROWS_EVENT_V1:
      ev = new Delete_rows_event(buf, event_len, fd_ev);
      break;
    case TABLE_MAP_EVENT:
      ev = new Table_map_event(buf, event_len, fd_ev);
      break;
    case BEGIN_LOAD_QUERY_EVENT:
      ev = new Begin_load_query_event(buf, event_len, fd_ev);
      break;
    case EXECUTE_LOAD_QUERY_EVENT:
      ev= new Execute_load_query_event(buf, event_len, fd_ev);
      break;
    case INCIDENT_EVENT:
      ev = new Incident_event(buf, event_len, fd_ev);
      break;
    case ROWS_QUERY_LOG_EVENT:
      ev= new Rows_query_event(buf, event_len, fd_ev);
      break;
    case GTID_LOG_EVENT:
    case ANONYMOUS_GTID_LOG_EVENT:
      ev= new Gtid_event(buf, event_len, fd_ev);
      break;
    case PREVIOUS_GTIDS_LOG_EVENT:
      ev= new Previous_gtids_event(buf, event_len, fd_ev);
      break;
    case WRITE_ROWS_EVENT:
      ev = new Write_rows_event(buf, event_len, fd_ev);
      break;
    case UPDATE_ROWS_EVENT:
      ev = new Update_rows_event(buf, event_len, fd_ev);
      break;
    case DELETE_ROWS_EVENT:
      ev = new Delete_rows_event(buf, event_len, fd_ev);
      break;
    default:
      {
        /*
          Create an object of Ignorable_log_event for unrecognized sub-class.
          So that SLAVE SQL THREAD will only update the position and continue.
        */
        int16_t flag_temp;
        memcpy(&flag_temp, buf + FLAGS_OFFSET, 2);
        if ( le16toh(flag_temp) & LOG_EVENT_IGNORABLE_F)
        {
          ev= new Ignorable_event(buf, fd_ev);
        }
        else
        {
          ev= NULL;
        }
        break;
      }
    }
  }
  if (ev)
  {
    if ((ev)->header()->type_code == FORMAT_DESCRIPTION_EVENT)
       {
          Format_description_event *temp= fd_ev;
          fd_ev= new Format_description_event(buf, event_len, temp);
          delete temp;
       }
  }
  if (ev)
  {
    ev->footer()->checksum_alg= alg;
    if (ev->footer()->checksum_alg != BINLOG_CHECKSUM_ALG_OFF &&
        ev->footer()->checksum_alg != BINLOG_CHECKSUM_ALG_UNDEF)
    {
      uint32_t temp_crc;
      memcpy(&temp_crc, buf + event_len, 4);
      //ev->crc= le32toh(temp_crc);
    }
  }

  /*
    is_valid is small event-specific sanity tests which are
    important; for example there are some my_malloc() in constructors
    (e.g. Query_log_event::Query_log_event(char*...)); when these
    my_malloc() fail we can't return an *error out of the constructor
    (because constructor is "void") ; so instead we leave the pointer we
    wanted to allocate (e.g. 'query') to 0 and we test it and set the
    value of is_valid to true or false based on the test.
    Same for Format_description_log_event, member 'post_header_len'.

    SLAVE_EVENT is never used, so it should not be read ever.
  */
  if (!ev  || (event_type == SLAVE_EVENT))
  {
    delete ev;
    if (!force_read) /* then program dies */
    {
      *error= "Found invalid event in binary log";
      return 0;
    }
    // the user can skip this event, and move to next event
    ev= new Unknown_event(buf, fd_ev);
  }

  return ev;
}
} // end namespace binary_log
