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

/**
  @addtogroup Replication
  @{

  @file decoder.h

  @brief Contains the class representing the decoding logic of the
  Binary_log_event.
*/

#ifndef DECODER_INCLUDED
#define DECODER_INCLUDED
#include "binary_log.h"

namespace binary_log {

/**
  @class Decoder

  @anchor Table_common_footer
  The Decoder contains the following:
  <table>

  <tr>
    <th>Name</th>
    <th>Format</th>
    <th>Description</th>
  </tr>

  <tr>
    <td>fd_ev</td>
    <td>Object pointer of class Format_description_event</td>
    <td>This will be passed to the event constructor while decoding
        that event.
    </td>
  </tr>
  <tr>
    <td>force_read</td>
    <td>bool</td>
    <td>The user can pass this option if they want to read unknown/corrupted
        events also and continue reading the binary log
    </td>
  </tr>
  </table>

*/
class Decoder
{
public:
  Decoder()
  : force_read(0)
  {
  }
  explicit Decoder(bool force_read_arg)
  : force_read(force_read_arg)
  {
  }
  ~Decoder()
  {
    delete fd_ev;
  }
  /**
    decode_event() functions reads an event.
    We need the description_event to be able to parse the
    event (to know the post-header's size); in fact in decode_event
    we detect the event's type, then call the specific event's
    constructor and pass description_event as an argument.

    @param   buf:       The buf containing the event data, fetched
                        from one of the supported transport in mysql-binlog.
    @param   event_len: Length of the event buffer.
    @param   error:     It will store the error occured if any and can be used
                        by the caller to find out what went wrong
    @param   crc_check: If set true it will lead to compare the
                        incoming and outgoing checksum value for all events.

    @return  An event object pointer of type Binary_log_event
    @note
    Allocates memory;  The caller is responsible for clean-up.
  */
  Binary_log_event* decode_event(const char* buf, size_t  event_len,
                                   const char **error, bool crc_check);
  /**
    Returns the checksum_algorithm implemented at the server side
    @param:  buf         buf containing the complete event data
    @param:  event_type  Type of the event being decoded.
    @param:  event_len   Length of the event being decoded

    @return: An enum containing the checksum_algorithm
   */
  enum_binlog_checksum_alg checksum_algorithm(const  char * buf,
                                              unsigned int event_type,
                                              size_t event_len);
  void set_format_description_event(Format_description_event *);
  Format_description_event* get_format_description_event();
private:
  Format_description_event *fd_ev;
  bool force_read;
};
}
#endif
