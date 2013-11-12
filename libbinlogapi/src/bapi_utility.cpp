/* Copyright (c) 2006, 2013, Oracle and/or its affiliates. All rights reserved.

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


#include "binlog_event.h"
#include "byteorder.h"
#include <algorithm>

using std::min;
using std::max;

namespace bapi_debug
{
  bool debug_checksum_test= false;
}
namespace binary_log{
/**
   Tests the checksum algorithm used for the binary log, and asserts in case if
   if the checksum algorithm is invalid
   @param   event_buf        point to the buffer containing serialized event
   @param   event_len       length of the event accounting possible
                             checksum alg
   @param   alg             checksum algorithm used for the binary log

   @return  TRUE            if test fails
            FALSE           as success
*/
bool event_checksum_test(unsigned char *event_buf, unsigned long event_len,
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
        FD event is checksummed and therefore verified w/o the binlog-in-use flag
      */
      memcpy(&flags, event_buf + FLAGS_OFFSET, sizeof(flags));
      flags= le16toh(flags);
      if (flags & LOG_EVENT_BINLOG_IN_USE_F)
        event_buf[FLAGS_OFFSET] &= ~LOG_EVENT_BINLOG_IN_USE_F;
#ifndef DBUG_OFF
      /*
         The only algorithm currently is CRC32. Zero indicates
         the binlog file is checksum-free *except* the FD-event.
      */
      assert(fd_alg == BINLOG_CHECKSUM_ALG_CRC32 || fd_alg == 0);
      assert(alg == BINLOG_CHECKSUM_ALG_CRC32);
#endif
      /*
        Complile time guard to watch over  the max number of alg
      */
      do_compile_time_assert(BINLOG_CHECKSUM_ALG_ENUM_END <= 0x80);
    }
    memcpy(&incoming, event_buf + event_len - BINLOG_CHECKSUM_LEN, sizeof(incoming));
    incoming= le32toh(incoming);
    computed= checksum_crc32(0L, NULL, 0);
    /* checksum the event content but not the checksum part itself */
    computed= binary_log::checksum_crc32(computed, (const unsigned char*) event_buf,
                          event_len - BINLOG_CHECKSUM_LEN);
    if (flags != 0)
    {
      /* restoring the orig value of flags of FD */
#ifndef DBUG_OFF
      assert(event_buf[EVENT_TYPE_OFFSET] == FORMAT_DESCRIPTION_EVENT);
#endif
      event_buf[FLAGS_OFFSET]= flags;
    }

    res= !(computed == incoming);
  }
#ifndef DBUG_OFF
  if (bapi_debug::debug_checksum_test)
    return true;
#endif
  return res;
}

} //end namespace
