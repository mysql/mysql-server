/* Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "binary_log_types.h"

#include "statement_events.h"

#include <algorithm>
#include <stdint.h>

const unsigned char checksum_version_split[3]= {5, 6, 1};
const unsigned long checksum_version_product=
  (checksum_version_split[0] * 256 + checksum_version_split[1]) * 256 +
  checksum_version_split[2];


namespace binary_log_debug
{
  bool debug_query_mts_corrupt_db_names= false;
  bool debug_checksum_test= false;
  bool debug_simulate_invalid_address= false;
  bool debug_pretend_version_50034_in_binlog= false;
}

namespace binary_log
{
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
enum_binlog_checksum_alg
Log_event_footer::get_checksum_alg(const char* buf, unsigned long len)
{
  enum_binlog_checksum_alg ret;
  char version[ST_SERVER_VER_LEN];
  unsigned char version_split[3];
  BAPI_ASSERT(buf[EVENT_TYPE_OFFSET] == FORMAT_DESCRIPTION_EVENT);
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
  BAPI_ASSERT(ret == BINLOG_CHECKSUM_ALG_OFF ||
              ret == BINLOG_CHECKSUM_ALG_UNDEF ||
              ret == BINLOG_CHECKSUM_ALG_CRC32);
  return ret;
}

/**
  Log_event_header constructor

  @param buf                  the buffer containing the complete information
                              including the event and the header data

  @param description_event    first constructor of Format_description_event,
                              used to extract the binlog_version
*/
Log_event_header::
Log_event_header(const char* buf, uint16_t binlog_version)
: data_written(0), log_pos(0)
{
  uint32_t tmp_sec;
  memcpy(&tmp_sec, buf, sizeof(tmp_sec));
  when.tv_sec= le32toh(tmp_sec);
  when.tv_usec= 0;
  type_code= static_cast<Log_event_type>(buf[EVENT_TYPE_OFFSET]);
  memcpy(&unmasked_server_id,
         buf + SERVER_ID_OFFSET, sizeof(unmasked_server_id));

  unmasked_server_id= le32toh(unmasked_server_id);

  /**
    @verbatim
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
    @endverbatim
   */
  memcpy(&data_written, buf + EVENT_LEN_OFFSET, 4);
  data_written= le64toh(data_written);

  memcpy(&log_pos, buf + LOG_POS_OFFSET, 4);
  log_pos= le64toh(log_pos);

  switch (binlog_version)
  {
  case 1:
    log_pos= 0;
    flags= 0;
    break;

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
    @verbatim
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
    @endverbatim
   */
    // Fall through.
  default:
    memcpy(&flags, buf + FLAGS_OFFSET, sizeof(flags));
    flags= le16toh(flags);

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
      break;
    }
  /* otherwise, go on with reading the header from buf (nothing now) */
  } //end switch (binlog_version)
  BAPI_ASSERT(type_code < ENUM_END_EVENT || flags & LOG_EVENT_IGNORABLE_F);
}


/**
  Tests the checksum algorithm used for the binary log, and asserts in case
  if the checksum algorithm is invalid.

  @param   event_buf       point to the buffer containing serialized event
  @param   event_len       length of the event accounting possible
                           checksum alg
  @param   alg             checksum algorithm used for the binary log

  @retval  true            if test fails
  @retval  false           as success
*/
bool Log_event_footer::event_checksum_test(unsigned char *event_buf,
                                           unsigned long event_len,
                                           enum_binlog_checksum_alg alg)
{
  bool res= false;
  unsigned short flags= 0; // to store in FD's buffer flags orig value

  if (alg != BINLOG_CHECKSUM_ALG_OFF && alg != BINLOG_CHECKSUM_ALG_UNDEF)
  {
    uint32_t incoming;
    uint32_t computed;

    if (event_buf[EVENT_TYPE_OFFSET] == FORMAT_DESCRIPTION_EVENT)
    {
    #ifndef DBUG_OFF
      unsigned char fd_alg= event_buf[event_len - BINLOG_CHECKSUM_LEN -
                                      BINLOG_CHECKSUM_ALG_DESC_LEN];
    #endif
      /*
        FD event is checksummed and therefore verified w/o
        the binlog-in-use flag.
      */
      memcpy(&flags, event_buf + FLAGS_OFFSET, sizeof(flags));
      flags= le16toh(flags);
      if (flags & LOG_EVENT_BINLOG_IN_USE_F)
        event_buf[FLAGS_OFFSET] &= ~LOG_EVENT_BINLOG_IN_USE_F;
      /*
         The only algorithm currently is CRC32. Zero indicates
         the binlog file is checksum-free *except* the FD-event.
      */
    #ifndef DBUG_OFF
      BAPI_ASSERT(fd_alg == BINLOG_CHECKSUM_ALG_CRC32 || fd_alg == 0);
    #endif
      BAPI_ASSERT(alg == BINLOG_CHECKSUM_ALG_CRC32);
      /*
        Complile time guard to watch over  the max number of alg
      */
      do_compile_time_assert(BINLOG_CHECKSUM_ALG_ENUM_END <= 0x80);
    }
    memcpy(&incoming,
           event_buf + event_len - BINLOG_CHECKSUM_LEN, sizeof(incoming));
    incoming= le32toh(incoming);

    computed= checksum_crc32(0L, NULL, 0);
    /* checksum the event content but not the checksum part itself */
    computed= binary_log::checksum_crc32(computed,
                                         (const unsigned char*) event_buf,
                                         event_len - BINLOG_CHECKSUM_LEN);

    if (flags != 0)
    {
      /* restoring the orig value of flags of FD */
      BAPI_ASSERT(event_buf[EVENT_TYPE_OFFSET] == FORMAT_DESCRIPTION_EVENT);
      event_buf[FLAGS_OFFSET]= static_cast<unsigned char>(flags);
    }

    res= !(computed == incoming);
  }
#ifndef DBUG_OFF
  if (binary_log_debug::debug_checksum_test)
    return true;
#endif
  return res;
}


/**
  This ctor will create a new object of Log_event_header, and initialize
  the variable m_header, which in turn will be used to initialize Log_event's
  member common_header.
  It will also advance the buffer after decoding the header(it is done through
  the constructor of Log_event_header) and
  will be pointing to the start of event data
*/
Binary_log_event::Binary_log_event(const char **buf, uint16_t binlog_version,
                                   const char *server_version)
: m_header(*buf, binlog_version)
{
  m_footer= Log_event_footer();
  //buf is advanced in Binary_log_event constructor to point to beginning of
  //post-header
  (*buf)+= LOG_EVENT_HEADER_LEN;
}

/*
  The destructor is pure virtual to prevent instantiation of the class.
*/
Binary_log_event::~Binary_log_event()
{
}

  /**
    This event type should never occur. It is never written to a binary log.
    If an event is read from a binary log that cannot be recognized as something
    else, it is treated as Unknown_event.

    @param buf                Contains the serialized event.
    @param description_event  An FDE event, used to get the following information
                              -binlog_version
                              -server_version
                              -post_header_len
                              -common_header_len
                              The content of this object
                              depends on the binlog-version currently in use.
  */
Unknown_event::Unknown_event(const char* buf,
                const Format_description_event *description_event)
  : Binary_log_event(&buf,
                     description_event->binlog_version,
                     description_event->server_version)
  {
  }
#ifndef HAVE_MYSYS
void Binary_log_event::print_event_info(std::ostream& info) {}
void Binary_log_event::print_long_info(std::ostream& info) {}
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
  info << "Timestamp: " << header()->when.tv_sec;
  this->print_event_info(info);
}

void Unknown_event::print_event_info(std::ostream& info)
{
  info << "Unhandled event";
}

void Unknown_event::print_long_info(std::ostream& info)
{
  info << "Timestamp: " << header()->when.tv_sec;
  this->print_event_info(info);
}

#endif
} // end namespace binary_log
