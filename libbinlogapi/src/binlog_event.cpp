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
#include <iostream>
typedef unsigned long ulong;


const unsigned char checksum_version_split[3]= {5, 6, 1};
const unsigned long checksum_version_product=
  (checksum_version_split[0] * 256 + checksum_version_split[1]) * 256 +
  checksum_version_split[2];

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
    ret= static_cast<enum_binlog_checksum_alg>(*(buf + len - BINLOG_CHECKSUM_LEN - BINLOG_CHECKSUM_ALG_DESC_LEN));
  assert(ret == BINLOG_CHECKSUM_ALG_OFF ||
         ret == BINLOG_CHECKSUM_ALG_UNDEF ||
         ret == BINLOG_CHECKSUM_ALG_CRC32);
  return ret;
}


/**
  Log_event_header constructor
*/
Log_event_header::Log_event_header(const char* buf,
                                   const Format_event *description_event)
{
  //TODO: replace uint4korr with le32toh() when merged with patch on rb2984
  when.tv_sec= uint4korr(buf);
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

Binary_log_event::~Binary_log_event()
{
}

Binary_log_event * create_incident_event(unsigned int type,
                                         const char *message, unsigned long pos)
{
  Incident_event *incident= new Incident_event();
  incident->header()->type_code= INCIDENT_EVENT;
  incident->header()->next_position= pos;
  incident->header()->event_length= LOG_EVENT_HEADER_SIZE + 2 + strlen(message);
  incident->type= type;
  incident->message.append(message);
  return incident;
}


void Binary_log_event::print_event_info(std::ostream& info) {}
void Binary_log_event::print_long_info(std::ostream& info) {}

void Unknown_event::print_event_info(std::ostream& info)
{
  info << "Unhandled event";
}

void Unknown_event::print_long_info(std::ostream& info)
{
  info << "Timestamp: " << this->header()->timestamp;
  this->print_event_info(info);
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
  info << "Timestamp: " << this->header()->timestamp;
  info << "\tThread id: " << (int)thread_id;
  info << "\tExec time: " << (int)exec_time;
  info << "\nDatabase: " << db_name;
  info << "\tQuery: ";
  this->print_event_info(info);
}

void Rotate_event::print_event_info(std::ostream& info)
{
  info << "Binlog Position: " << binlog_pos;
  info << ", Log name: " << binlog_file;
}

void Rotate_event::print_long_info(std::ostream& info)
{
  info << "Timestamp: " << this->header()->timestamp;
  info << "\t";
  this->print_event_info(info);
}

void Format_event::print_event_info(std::ostream& info)
{
  info << "Server ver: " << master_version;
  info << ", Binlog ver: " << binlog_version;
}

void Format_event::print_long_info(std::ostream& info)
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
  info << "Timestamp: " << this->header()->timestamp;
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
  info << "Timestamp: " << this->header()->timestamp;
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
  info << "Timestamp: " << this->header()->timestamp;
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
  info << "Timestamp: " << this->header()->timestamp;
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
  info << "Timestamp: " << this->header()->timestamp;
  info << "\t";
  this->print_event_info(info);
}

} // end namespace binary_log
