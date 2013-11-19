/*
Copyright (c) 2003, 2011, 2013, Oracle and/or its affiliates. All rights
reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of
the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/

#include "binlog_event.h"
#include "transitional_methods.h"
#include <stdlib.h>
#include <string.h>
#include <iostream>

const unsigned char checksum_version_split[3]= {5, 6, 1};
const unsigned long checksum_version_product=
  (checksum_version_split[0] * 256 + checksum_version_split[1]) * 256 +
  checksum_version_split[2];

#ifdef _my_sys_h
PSI_memory_key key_memory_log_event;
PSI_memory_key key_memory_Incident_log_event_message;
PSI_memory_key key_memory_Rows_query_log_event_rows_query;
#endif

namespace binary_log_debug
{
  bool debug_query_mts_corrupt_db_names= false;
}

namespace binary_log
{
const char *get_event_type_str(Log_event_type type)
{
  switch(type) {
  case START_EVENT_V3:  return "Start_v3";
  case STOP_EVENT:   return "Stop";
  case QUERY_EVENT:  return "Query";
  case ROTATE_EVENT: return "Rotate";
  case INTVAR_EVENT: return "Intvar";
  case LOAD_EVENT:   return "Load";
  case NEW_LOAD_EVENT:   return "New_load";
  case SLAVE_EVENT:  return "Slave";
  case CREATE_FILE_EVENT: return "Create_file";
  case APPEND_BLOCK_EVENT: return "Append_block";
  case DELETE_FILE_EVENT: return "Delete_file";
  case EXEC_LOAD_EVENT: return "Exec_load";
  case RAND_EVENT: return "RAND";
  case XID_EVENT: return "Xid";
  case USER_VAR_EVENT: return "User var";
  case FORMAT_DESCRIPTION_EVENT: return "Format_desc";
  case TABLE_MAP_EVENT: return "Table_map";
  case PRE_GA_WRITE_ROWS_EVENT: return "Write_rows_event_old";
  case PRE_GA_UPDATE_ROWS_EVENT: return "Update_rows_event_old";
  case PRE_GA_DELETE_ROWS_EVENT: return "Delete_rows_event_old";
  case WRITE_ROWS_EVENT_V1: return "Write_rows_v1";
  case UPDATE_ROWS_EVENT_V1: return "Update_rows_v1";
  case DELETE_ROWS_EVENT_V1: return "Delete_rows_v1";
  case BEGIN_LOAD_QUERY_EVENT: return "Begin_load_query";
  case EXECUTE_LOAD_QUERY_EVENT: return "Execute_load_query";
  case INCIDENT_EVENT: return "Incident";
  case IGNORABLE_LOG_EVENT: return "Ignorable";
  case ROWS_QUERY_LOG_EVENT: return "Rows_query";
  case WRITE_ROWS_EVENT: return "Write_rows";
  case UPDATE_ROWS_EVENT: return "Update_rows";
  case DELETE_ROWS_EVENT: return "Delete_rows";
  case GTID_LOG_EVENT: return "Gtid";
  case ANONYMOUS_GTID_LOG_EVENT: return "Anonymous_Gtid";
  case PREVIOUS_GTIDS_LOG_EVENT: return "Previous_gtids";
  case HEARTBEAT_LOG_EVENT: return "Heartbeat";
  default: return "Unknown";
  }
  return "No Error";
}

/*this method was previously defined in log_event.cc */
/**
   The method returns the checksum algorithm used to checksum the binary log.
   For MySQL server versions < 5.6, the algorithm is undefined. For the higher
   versions, the type is decoded from the FORMAT_DESCRIPTION_EVENT.

   @param buf buffer holding serialized FD event
   @param len netto (possible checksum is stripped off) length of the event buf

   @return  the version-safe checksum alg descriptor where zero
            designates no checksum, 255 - the orginator is
            checksum-unaware (effectively no checksum) and the actuall
            [1-254] range alg descriptor.
*/
enum_binlog_checksum_alg get_checksum_alg(const char* buf, unsigned long len)
{
  enum_binlog_checksum_alg ret;
  char version[ST_SERVER_VER_LEN];
  unsigned char version_split[3];
#ifndef DBUG_OFF
  assert(buf[EVENT_TYPE_OFFSET] == FORMAT_DESCRIPTION_EVENT);
#endif
  memcpy(version, buf +
         buf[LOG_EVENT_MINIMAL_HEADER_LEN + ST_COMMON_HEADER_LEN_OFFSET]
         + ST_SERVER_VER_OFFSET, ST_SERVER_VER_LEN);
  version[ST_SERVER_VER_LEN - 1]= 0;

  do_server_version_split(version, version_split);
  if (version_product(version_split) < checksum_version_product)
    ret=  BINLOG_CHECKSUM_ALG_UNDEF;
  else
    ret= static_cast<enum_binlog_checksum_alg>(*(buf + len -
                                                 BINLOG_CHECKSUM_LEN -
                                                 BINLOG_CHECKSUM_ALG_DESC_LEN));
  assert(ret == BINLOG_CHECKSUM_ALG_OFF ||
         ret == BINLOG_CHECKSUM_ALG_UNDEF ||
         ret == BINLOG_CHECKSUM_ALG_CRC32);
  return ret;
}

/*
  This method copies the string pointed to by src (including
  the terminating null byte ('\0')) to the array pointed to by dest.
  The strings may not overlap, and the destination string dest must be
  large enough to receive the copy.

  @param src  the source string
  @param dest the desctination string

  @return     pointer to the end of the string dest
*/
char *bapi_stpcpy(char *dst, const char *src)
{
  strcpy(dst, src);
  return dst + strlen(dst);
}
/**
  Log_event_header constructor
*/
Log_event_header::Log_event_header(const char* buf,
                                   const Format_description_event *description_event)
{
  uint32_t tmp_sec;
  memcpy(&tmp_sec, buf, sizeof(tmp_sec));
  when.tv_sec= le32toh(tmp_sec);
  when.tv_usec= 0;
  //TODO:Modify server_id in Log_event based on unmasked_server_id defined here
  unmasked_server_id= uint4korr(buf + SERVER_ID_OFFSET);

  /**
    The first 13 bytes in the header is as follows:
      +============================================+
      | member_variable               offset : len |
      +============================================+
      | when.tv_sec                        0 : 4   |
      +--------------------------------------------+
      | type_code       EVENT_TYPE_OFFSET(4) : 1   |
      +--------------------------------------------+
      | server_id       SERVER_ID_OFFSET(5)  : 4   |
      +--------------------------------------------+
      | data_written    EVENT_LEN_OFFSET(9)  : 4   |
      +============================================+
   */
  data_written= uint4korr(buf + EVENT_LEN_OFFSET);
  log_pos= uint4korr(buf + LOG_POS_OFFSET);

  switch (description_event->binlog_version)
  {
  case 1:
    log_pos= 0;
    flags= 0;
    return;

  case 3:
    /*
      If the log is 4.0 (so here it can only be a 4.0 relay log read by
      the SQL thread or a 4.0 master binlog read by the I/O thread),
      log_pos is the beginning of the event: we transform it into the end
      of the event, which is more useful.
      But how do you know that the log is 4.0: you know it if
      description_event is version 3 *and* you are not reading a
      Format_desc (remember that mysqlbinlog starts by assuming that 5.0
      logs are in 4.0 format, until it finds a Format_desc).
    */
    if (buf[EVENT_TYPE_OFFSET] < FORMAT_DESCRIPTION_EVENT && log_pos)
    {
      /*
        If log_pos=0, don't change it. log_pos==0 is a marker to mean
        "don't change rli->group_master_log_pos" (see
        inc_group_relay_log_pos()). As it is unreal log_pos, adding the
        event len's is not correct. For example, a fake Rotate event should
        not have its log_pos (which is 0) changed or it will modify
        Exec_master_log_pos in SHOW SLAVE STATUS, displaying a wrong
        value of (a non-zero offset which does not exist in the master's
        binlog, so which will cause problems if the user uses this value
        in CHANGE MASTER).
      */
      log_pos+= data_written; /* purecov: inspected */
    }

  /* 4.0 or newer */
  /**
    Additional header fields include:
      +=============================================+
      | member_variable               offset : len  |
      +=============================================+
      | log_pos           LOG_POS_OFFSET(13) : 4    |
      +---------------------------------------------+
      | flags               FLAGS_OFFSET(17) : 1    |
      +---------------------------------------------+
      | extra_headers                     19 : x-19 |
      +=============================================+
     extra_headers are not used in the current version.
   */

  default:
    if (description_event->binlog_version != 3)
      log_pos= uint4korr(buf + LOG_POS_OFFSET);

    flags= uint2korr(buf + FLAGS_OFFSET);

     if ((buf[EVENT_TYPE_OFFSET] == FORMAT_DESCRIPTION_EVENT) ||
         (buf[EVENT_TYPE_OFFSET] == ROTATE_EVENT))
     {
       /*
         These events always have a header which stops here (i.e. their
         header is FROZEN).
       */
       /*
         Initialization to zero of all other Log_event members as they're
         not specified. Currently there are no such members; in the future
         there will be an event UID (but Format_description and Rotate
         don't need this UID, as they are not propagated through
         --log-slave-updates (remember the UID is used to not play a query
         twice when you have two masters which are slaves of a 3rd master).
         Then we are done with decoding the header.
      */
      return;
    }
  /* otherwise, go on with reading the header from buf (nothing now) */
  } //end switch (description_event->binlog_version)
}


/**
  This ctor will create a new object of Log_event_header, and initialize
  the variable m_header, which in turn will be used to initialize Log_event's
  member common_header.
  It will also advance the buffer after reading the common_header_len
*/
Binary_log_event::Binary_log_event(const char **buf, uint16_t binlog_version)
{
  Format_description_event *des= new Format_description_event(binlog_version);
  m_header= new Log_event_header(*buf, des);
  // remove the comments when all the events are moved to libbinlogapi
  // (*buf)+= des->common_header_len;
  delete des;
  des= NULL;
}
Binary_log_event::~Binary_log_event()
{
  //This comment should be removed in the independent version
  // now the memory is deallocated in Log_event's desctructor
  //if(m_header)
    //delete m_header;
}

Binary_log_event * create_incident_event(unsigned int type,
                                         const char *message, unsigned long pos)
{
  Incident_event *incident= new Incident_event();
  incident->header()->type_code= INCIDENT_EVENT;
  //incident->header()->next_position= pos;
  //incident->header()->event_length= LOG_EVENT_HEADER_SIZE + 2 + strlen(message);
  incident->type= type;
  incident->message.append(message);
  return incident;
}

/*
 *TODO FDE constructor is not tested in this patch, but it will
 *tested in future patches
*/
/**
  Format_description_log_event 1st constructor.

    This constructor can be used to create the event to write to the binary log
    (when the server starts or when FLUSH LOGS), or to create artificial events
    to parse binlogs from MySQL 3.23 or 4.x.
    When in a client, only the 2nd use is possible.

  @param binlog_ver             the binlog version for which we want to build
                                an event. Can be 1 (=MySQL 3.23), 3 (=4.0.x
                                x>=2 and 4.1) or 4 (MySQL 5.0). Note that the
                                old 4.0 (binlog version 2) is not supported;
                                it should not be used for replication with
                                5.0.
  @param server_ver             a string containing the server version.
*/
Format_description_event::Format_description_event(uint16_t binlog_ver, const char* server_ver)
{
  binlog_version= binlog_ver;
  switch (binlog_ver) {
  case 4: /* MySQL 5.0 and above*/
    memcpy(server_version, ::server_version, ST_SERVER_VER_LEN);
    DBUG_EXECUTE_IF("pretend_version_5_0_34_in_binlog",
                    bapi_stpcpy(server_version, "5.0.34"););
    common_header_len= LOG_EVENT_HEADER_LEN;
    number_of_event_types= LOG_EVENT_TYPES;
    /* we'll catch malloc() error in is_valid() */
    post_header_len=(uint8_t*) malloc(number_of_event_types * sizeof(uint8_t)
                                       + BINLOG_CHECKSUM_ALG_DESC_LEN);
    /*
      This long list of assignments is not beautiful, but I see no way to
      make it nicer, as the right members are enum, not array members, so
      it's impossible to write a loop.
    */
    if (post_header_len)
    {
#ifndef DBUG_OFF
      // Allows us to sanity-check that all events initialized their
      // events (see the end of this 'if' block).
      memset(post_header_len, 255, number_of_event_types * sizeof(uint8_t));
#endif

      /* Note: all event types must explicitly fill in their lengths here. */
      post_header_len[START_EVENT_V3-1]= START_V3_HEADER_LEN;
      post_header_len[QUERY_EVENT-1]= QUERY_HEADER_LEN;
      post_header_len[STOP_EVENT-1]= STOP_HEADER_LEN;
      post_header_len[ROTATE_EVENT-1]= ROTATE_HEADER_LEN;
      post_header_len[INTVAR_EVENT-1]= INTVAR_HEADER_LEN;
      post_header_len[LOAD_EVENT-1]= LOAD_HEADER_LEN;
      post_header_len[SLAVE_EVENT-1]= 0;   /* Unused because the code for Slave log event was removed. (15th Oct. 2010) */
      post_header_len[CREATE_FILE_EVENT-1]= CREATE_FILE_HEADER_LEN;
      post_header_len[APPEND_BLOCK_EVENT-1]= APPEND_BLOCK_HEADER_LEN;
      post_header_len[EXEC_LOAD_EVENT-1]= EXEC_LOAD_HEADER_LEN;
      post_header_len[DELETE_FILE_EVENT-1]= DELETE_FILE_HEADER_LEN;
      post_header_len[NEW_LOAD_EVENT-1]= NEW_LOAD_HEADER_LEN;
      post_header_len[RAND_EVENT-1]= RAND_HEADER_LEN;
      post_header_len[USER_VAR_EVENT-1]= USER_VAR_HEADER_LEN;
      post_header_len[FORMAT_DESCRIPTION_EVENT-1]= FORMAT_DESCRIPTION_HEADER_LEN;
      post_header_len[XID_EVENT-1]= XID_HEADER_LEN;
      post_header_len[BEGIN_LOAD_QUERY_EVENT-1]= BEGIN_LOAD_QUERY_HEADER_LEN;
      post_header_len[EXECUTE_LOAD_QUERY_EVENT-1]= EXECUTE_LOAD_QUERY_HEADER_LEN;
      /*
        The PRE_GA events are never written to any binlog, but
        their lengths are included in the Format_description_log_event.
        Hence, we need to assign some value here, to avoid reading
        uninitialized memory when the array is written to disk.
      */
      post_header_len[PRE_GA_WRITE_ROWS_EVENT-1] = 0;
      post_header_len[PRE_GA_UPDATE_ROWS_EVENT-1] = 0;
      post_header_len[PRE_GA_DELETE_ROWS_EVENT-1] = 0;

      post_header_len[TABLE_MAP_EVENT-1]=       TABLE_MAP_HEADER_LEN;
      post_header_len[WRITE_ROWS_EVENT_V1-1]=   ROWS_HEADER_LEN_V1;
      post_header_len[UPDATE_ROWS_EVENT_V1-1]=  ROWS_HEADER_LEN_V1;
      post_header_len[DELETE_ROWS_EVENT_V1-1]=  ROWS_HEADER_LEN_V1;
      /*
        We here have the possibility to simulate a master before we changed
        the table map id to be stored in 6 bytes: when it was stored in 4
        bytes (=> post_header_len was 6). This is used to test backward
        compatibility.
        This code can be removed after a few months (today is Dec 21st 2005),
        when we know that the 4-byte masters are not deployed anymore (check
        with Tomas Ulin first!), and the accompanying test (rpl_row_4_bytes)
        too.
      */
      DBUG_EXECUTE_IF("old_row_based_repl_4_byte_map_id_master",
                      post_header_len[TABLE_MAP_EVENT-1]=
                      post_header_len[WRITE_ROWS_EVENT_V1-1]=
                      post_header_len[UPDATE_ROWS_EVENT_V1-1]=
                      post_header_len[DELETE_ROWS_EVENT_V1-1]= 6;);
      post_header_len[INCIDENT_EVENT-1]= INCIDENT_HEADER_LEN;
      post_header_len[HEARTBEAT_LOG_EVENT-1]= 0;
      post_header_len[IGNORABLE_LOG_EVENT-1]= IGNORABLE_HEADER_LEN;
      post_header_len[ROWS_QUERY_LOG_EVENT-1]= IGNORABLE_HEADER_LEN;
      post_header_len[WRITE_ROWS_EVENT-1]=  ROWS_HEADER_LEN_V2;
      post_header_len[UPDATE_ROWS_EVENT-1]= ROWS_HEADER_LEN_V2;
      post_header_len[DELETE_ROWS_EVENT-1]= ROWS_HEADER_LEN_V2;
       post_header_len[GTID_LOG_EVENT-1]=
        post_header_len[ANONYMOUS_GTID_LOG_EVENT-1]= 25;
      //TODO  25 will be replaced by Gtid_log_event::POST_HEADER_LENGTH;
      post_header_len[PREVIOUS_GTIDS_LOG_EVENT-1]= IGNORABLE_HEADER_LEN;
      // Sanity-check that all post header lengths are initialized.
      int i;
      for (i= 0; i < number_of_event_types; i++)
        DBUG_ASSERT(post_header_len[i] != 255);
    }
    break;

  case 1: /* 3.23 */
    bapi_stpcpy(server_version, server_ver ? server_ver : "3.23");
  case 3: /* 4.0.x x>=2 */
    /*
      We build an artificial (i.e. not sent by the master) event, which
      describes what those old master versions send.
    */
    bapi_stpcpy(server_version, server_ver ? server_ver : "4.0");
    common_header_len= binlog_ver==1 ? OLD_HEADER_LEN :
      LOG_EVENT_MINIMAL_HEADER_LEN;
    /*
      The first new event in binlog version 4 is Format_desc. So any event type
      after that does not exist in older versions. We use the events known by
      version 3, even if version 1 had only a subset of them (this is not a
      problem: it uses a few bytes for nothing but unifies code; it does not
      make the slave detect less corruptions).
    */
    number_of_event_types= FORMAT_DESCRIPTION_EVENT - 1;
    post_header_len= (uint8_t*) malloc(number_of_event_types * sizeof(uint8_t));
    if (post_header_len)
    {
      post_header_len[START_EVENT_V3-1]= START_V3_HEADER_LEN;
      post_header_len[QUERY_EVENT-1]= QUERY_HEADER_MINIMAL_LEN;
      post_header_len[STOP_EVENT-1]= 0;
      post_header_len[ROTATE_EVENT-1]= (binlog_ver==1) ? 0 : ROTATE_HEADER_LEN;
      post_header_len[INTVAR_EVENT-1]= 0;
      post_header_len[LOAD_EVENT-1]= LOAD_HEADER_LEN;
      post_header_len[SLAVE_EVENT-1]= 0;  /* Unused because the code for Slave log event was removed. (15th Oct. 2010) */
      post_header_len[CREATE_FILE_EVENT-1]= CREATE_FILE_HEADER_LEN;
      post_header_len[APPEND_BLOCK_EVENT-1]= APPEND_BLOCK_HEADER_LEN;
      post_header_len[EXEC_LOAD_EVENT-1]= EXEC_LOAD_HEADER_LEN;
      post_header_len[DELETE_FILE_EVENT-1]= DELETE_FILE_HEADER_LEN;
      post_header_len[NEW_LOAD_EVENT-1]= post_header_len[LOAD_EVENT-1];
      post_header_len[RAND_EVENT-1]= 0;
      post_header_len[USER_VAR_EVENT-1]= 0;
    }
    break;
  default: /* Includes binlog version 2 i.e. 4.0.x x<=1 */
    post_header_len= 0; /* will make is_valid() fail */
    break;
  }
  //calc_server_version_split();
  checksum_alg= binary_log::BINLOG_CHECKSUM_ALG_UNDEF;
}

Format_description_event::~Format_description_event()
{
  if(post_header_len)
    free(post_header_len);
}
//void Binary_log_event::print_event_info(std::ostream& info) {}
//void Binary_log_event::print_long_info(std::ostream& info) {}

/********************************************************************
           Rotate_event methods
*********************************************************************/
/**
  The variable part of the Rotate event contains the name of the next binary
  log file,  and the position of the first event in the next binary log file.

  The buffer layout is as follows:
  +-----------------------------------------------------------------------+
  | common_header | post_header | position og the first event | file name |
  +-----------------------------------------------------------------------+

  @param buf Buffer contain event data in the layout specified above
  @param event_len The length of the event written in the log file
  @param description_event FDE used to extract the post header length, which
                           depends on the binlog version
  @param head Header information of the event
*/
Rotate_event::Rotate_event(const char* buf, unsigned int event_len,
                           const Format_description_event *description_event)
: Binary_log_event(&buf, description_event->binlog_version), new_log_ident(0),
  flags(DUP_NAME)
{
  // This will ensure that the event_len is what we have at EVENT_LEN_OFFSET
  size_t header_size= description_event->common_header_len;
  size_t post_header_len= description_event->post_header_len[ROTATE_EVENT - 1];
  unsigned int ident_offset;

  if (event_len < header_size)
    return;

  buf += header_size;

  /**
    By default, an event start immediately after the magic bytes in the binary
    log, which is at offset 4. In case if the slave has to rotate to a
    different event instead of the first one, the binary log offset for that
    event is specified in the post header. Else, the position is set to 4.
  */
  if (post_header_len)
  {
    memcpy(&pos, buf + R_POS_OFFSET, 8);
    pos= le64toh(pos);
  }
  else
    pos= 4;

  ident_len= event_len - (header_size + post_header_len);
  ident_offset= post_header_len;
  set_if_smaller(ident_len,FN_REFLEN-1);

  new_log_ident= bapi_strndup(buf + ident_offset, ident_len);
}

/**
  This method is used by the binlog_browser to print short and long
  information about the event. Since the body of Stop_event is empty
  the relevant information contains only the timestamp.
  Please note this is different from the print_event_info methods
  used by mysqlbinlog.cc.

  @param std output stream to which the event data is appended.
*/
void Stop_event::print_long_info(std::ostream& info)
{
  info << "Timestamp: " << this->header()->when.tv_sec;
  this->print_event_info(info);
}

void Unknown_event::print_event_info(std::ostream& info)
{
  info << "Unhandled event";
}

void Unknown_event::print_long_info(std::ostream& info)
{
  info << "Timestamp: " << this->header()->when.tv_sec;
  this->print_event_info(info);
}

/**
  This method initializes the members of strcuture variable sql_ex_data_info,
  defined in a Load_event. The structure stores the data about processing
  the file loaded into tables using LOAD_DATA_INFILE, which is optionally
  specified in the LOAD_DATA_INFILE query.

  @param buf Buffer contained in the following format
      +-----------------------------------------------------------------------+
      |field_term_len|field_term|enclosed_len|enclosed|line_term_len|line_term|
      +-----------------------------------------------------------------------+
      +------------------------------------------------------------------+
      |line_start_len|line_start|escaped_len|escaped|opt_flags|empty_flag|
      +------------------------------------------------------------------+
  @param buf_end pointer after the empty flag bitfield
  @param ue_new_format flag indicating whther the new format is to be forced
  @return  the pointer to the first byte after the sql_ex structure, which is
            the start of field lengths array.
*/
const char *binary_log::sql_ex_data_info::init(const char *buf,
                                               const char *buf_end,
                                               bool use_new_format)
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
Load_event::Load_event(const char *buf, uint event_len,
                       const Format_description_event *description_event)
  :Binary_log_event(&buf, description_event->binlog_version), num_fields(0),
   fields(0), field_lens(0),field_block_len(0),
   table_name(0), db(0), fname(0), local_fname(FALSE),
   /**
     Load_log_event which comes from the binary log does not contain
     information about the type of insert which was used on the master.
     Assume that it was an ordinary, non-concurrent LOAD DATA.
    */
   is_concurrent(FALSE)
{
  if (event_len)
    copy_load_event(buf, event_len,
                   ((buf[EVENT_TYPE_OFFSET] == LOAD_EVENT) ?
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

  @param buf Event common header+data for Load_event
  @param event_len Length of fixed + variable part of even data
  @param body_offset Length indicating starting of variable data part in buf
  @param description_event FDE read from the same binary log file

  @return 0 on success
          1 on failure
*/
int Load_event::copy_load_event(const char *buf, unsigned long event_len,
                                int body_offset, const Format_description_event
                                *description_event)
{
  /**
    Fixed data part
  +---------------------------------------------------------------------------+
  |thread_id|exec_time|no. of lines to skip|tb_name_len |db_name_len|col_count|
  +---------------------------------------------------------------------------+
  */

  unsigned int data_len;
  char* buf_end = (char*)buf + event_len;
  /* this is the beginning of the post-header */
  const char* data_head = buf + description_event->common_header_len;

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
    Variable data part
  +---------------------------------------------------------------------------+
  |sql_ex_data struct|len of col names to load|col_names|tb_name|db_name|fname|
  +---------------------------------------------------------------------------+
  */
  if ((int) event_len < body_offset)
    return 1;
  /*
    Sql_ex_data.init() on success returns the pointer to the first byte after
    the sql_ex structure, which is the start of field lengths array.
  */
  if (!(field_lens= (unsigned char*)sql_ex_data.init((char*)buf + body_offset,
                                        buf_end,
                                        buf[EVENT_TYPE_OFFSET] != LOAD_EVENT)))
    return 1;

  data_len = event_len - body_offset;
  if (num_fields > data_len) // simple sanity check against corruption
    return 1;
  for (unsigned int i= 0; i < num_fields; i++)
    field_block_len+= (unsigned int)field_lens[i] + 1;

  fields= (char*)field_lens + num_fields;
  table_name= fields + field_block_len;
  db= table_name + table_name_len + 1;
  fname= db + db_len + 1;
  fname_len= strlen(fname);
  // null termination is accomplished by the caller doing buf[event_len]= 0

  return 0;
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
  :Load_event(buf, 0, description_event),
   fake_base(0), block(0), inited_from_old(0)
{
  unsigned int block_offset;
  unsigned int header_len= description_event->common_header_len;
  unsigned char load_header_len=
                description_event->post_header_len[LOAD_EVENT-1];
  unsigned char create_file_header_len=
                description_event->post_header_len[CREATE_FILE_EVENT-1];
#ifdef _my_sys_h
  if (!(event_buf= (char*)my_memdup(key_memory_log_event,
                                     buf, len, MYF(MY_WME))) ||
      copy_load_event(event_buf,len,
                     ((buf[EVENT_TYPE_OFFSET] == LOAD_EVENT) ?
                      load_header_len + header_len :
                      (fake_base ? (header_len + load_header_len) :
                                   (header_len + load_header_len) +
                                   create_file_header_len)),
                     description_event))
    return;
#else
  event_buf= (const char*)malloc(len);
  if(!event_buf)
    return;
  memcpy(&event_buf, &buf, len);
  if (copy_load_event(event_buf,len,
                     ((buf[EVENT_TYPE_OFFSET] == LOAD_EVENT) ?
                      load_header_len + header_len :
                      (fake_base ? (header_len+load_header_len) :
                                   (header_len+load_header_len) +
                                   create_file_header_len)),
                     description_event,
                     header))
    return;
#endif
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
           sizeof(file_id));
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

Create_file_event::Create_file_event(unsigned char* block_arg,
                                     unsigned int  block_len_arg,
                                     unsigned int file_id_arg)
: Load_event(), fake_base(0), block(block_arg), event_buf(0),
  block_len(block_len_arg), file_id(file_id_arg)
{
  sql_ex_data.force_new_format();
}

/**
  Delete_file_event constructor
*/
Delete_file_event::Delete_file_event(const char* buf, unsigned int len,
                                     const Format_description_event*
                                     description_event)
  :Binary_log_event(&buf, description_event->binlog_version), file_id(0)
{
  unsigned char common_header_len= description_event->common_header_len;
  unsigned char delete_file_header_len=
                     description_event->post_header_len[DELETE_FILE_EVENT - 1];
  if (len < (unsigned int)(common_header_len + delete_file_header_len))
    return;
  memcpy(&file_id, buf + common_header_len + DF_FILE_ID_OFFSET,
         sizeof(file_id));
  file_id= le32toh(file_id);
}

/**
  Execute_load_event constructor
*/

Execute_load_event::Execute_load_event(const char* buf, unsigned int len,
                                       const Format_description_event*
                                       description_event)
  :Binary_log_event(&buf, description_event->binlog_version), file_id(0)
{
  unsigned char common_header_len= description_event->common_header_len;
  unsigned char exec_load_header_len= description_event->
                                      post_header_len[EXEC_LOAD_EVENT-1];

  if (len < (unsigned int)(common_header_len + exec_load_header_len))
    return;

  memcpy(&file_id, buf + common_header_len + EL_FILE_ID_OFFSET,
         sizeof(file_id));
  file_id= le32toh(file_id);
}

/**
  Append_block_event constructor
*/

Append_block_event::Append_block_event(const char* buf, unsigned int len,
                                       const Format_description_event*
                                       description_event)
  :Binary_log_event(&buf, description_event->binlog_version), block(0)
{
  unsigned char common_header_len= description_event->common_header_len;
  unsigned char append_block_header_len=
    description_event->post_header_len[APPEND_BLOCK_EVENT-1];
  unsigned int total_header_len= common_header_len+append_block_header_len;
  if (len < total_header_len)
    return;

  memcpy(&file_id, buf + common_header_len + AB_FILE_ID_OFFSET,
         sizeof(file_id));
  file_id= le32toh(file_id);

  block= (unsigned char*)buf + total_header_len;
  block_len= len - total_header_len;
}


Begin_load_query_event::
Begin_load_query_event(const char* buf, unsigned int len,
                       const Format_description_event* desc_event)
  :Append_block_event(buf, len, desc_event)
{
}

void Query_event::print_event_info(std::ostream& info)
{
  if (strcmp(query.c_str(), "BEGIN") != 0 &&
      strcmp(query.c_str(), "COMMIT") != 0)
  {
    info << "use `" << db_name << "`; ";
  }
  info << query;
}

void Query_event::print_long_info(std::ostream& info)
{
  info << "Timestamp: " << this->header()->when.tv_sec;
  info << "\tThread id: " << (int)thread_id;
  info << "\tExec time: " << (int)exec_time;
  info << "\nDatabase: " << db_name;
  info << "\tQuery: ";
  this->print_event_info(info);
}

void Rotate_event::print_event_info(std::ostream& info)
{
  info << "Binlog Position: " << pos;
  info << ", Log name: " << new_log_ident;
}

void Rotate_event::print_long_info(std::ostream& info)
{
  info << "Timestamp: " << this->header()->when.tv_sec;
  info << "\t";
  this->print_event_info(info);
}

void Format_description_event::print_event_info(std::ostream& info)
{
  info << "Server ver: " << master_version;
  info << ", Binlog ver: " << binlog_version;
}

void Format_description_event::print_long_info(std::ostream& info)
{
  this->print_event_info(info);
  info << "\nCreated timestamp: " << created_ts;
  info << "\tCommon Header Length: " << (int)log_header_len;
  info << "\nPost header length for events: \n";
}

void User_var_event::print_event_info(std::ostream& info)
{
  info << "@`" << name << "`=";
  if(type == STRING_TYPE)
    info  << value;
  else
    info << "<Binary encoded value>";
  //TODO: value is binary encoded, requires decoding
}

void User_var_event::print_long_info(std::ostream& info)
{
  info << "Timestamp: " << this->header()->when.tv_sec;
  info << "\tType: "
       << get_value_type_string(static_cast<Value_type>(type));
  info << "\n";
  this->print_event_info(info);
}

void Table_map_event::print_event_info(std::ostream& info)
{
  info << "table id: " << table_id << " ("
       << db_name << "."
       << table_name << ")";
}

void Table_map_event::print_long_info(std::ostream& info)
{
  info << "Timestamp: " << this->header()->when.tv_sec;
  info << "\tFlags: " << flags;
  info << "\tColumn Type: ";
  /**
    TODO: Column types are stored as integers. To be
    replaced by string representation of types.
  */
  std::vector<uint8_t>::iterator it;
  for (it= columns.begin(); it != columns.end(); ++it)
  {
    info << "\t" << (int)*it;
  }
  info << "\n";
  this->print_event_info(info);
}

void Row_event::print_event_info(std::ostream& info)
{
  info << "table id: " << table_id << " flags: ";
  info << get_flag_string(static_cast<enum_flag>(flags));
}

void Row_event::print_long_info(std::ostream& info)
{
  info << "Timestamp: " << this->header()->when.tv_sec;
  info << "\n";
  this->print_event_info(info);

  //TODO: Extract table names and column data.
  if (this->get_event_type() == PRE_GA_WRITE_ROWS_EVENT ||
      this->get_event_type() == WRITE_ROWS_EVENT_V1 ||
      this->get_event_type() == WRITE_ROWS_EVENT)
    info << "\nType: Insert" ;

  if (this->get_event_type() == PRE_GA_DELETE_ROWS_EVENT ||
      this->get_event_type() == DELETE_ROWS_EVENT_V1 ||
      this->get_event_type() == DELETE_ROWS_EVENT)
    info << "\nType: Delete" ;

  if (this->get_event_type() == PRE_GA_UPDATE_ROWS_EVENT ||
      this->get_event_type() == UPDATE_ROWS_EVENT_V1 ||
      this->get_event_type() == UPDATE_ROWS_EVENT)
    info << "\nType: Update" ;
}

void Int_var_event::print_event_info(std::ostream& info)
{
  info << get_type_string(static_cast<Int_event_type>(type));
  info << "\tValue: " << value;
}

void Int_var_event::print_long_info(std::ostream& info)
{
  info << "Timestamp: " << this->header()->when.tv_sec;
  info << "\t";
  this->print_event_info(info);
}

void Incident_event::print_event_info(std::ostream& info)
{
  info << message;
}

void Incident_event::print_long_info(std::ostream& info)
{
  this->print_event_info(info);
}

void Xid::print_event_info(std::ostream& info)
{
  //TODO: Write process_event function for Xid events
  info << "Xid ID=" << xid_id;
}

void Xid::print_long_info(std::ostream& info)
{
  info << "Timestamp: " << this->header()->when.tv_sec;
  info << "\t";
  this->print_event_info(info);
}

} // end namespace binary_log
