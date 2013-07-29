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

#ifndef BINLOG_DRIVER_INCLUDED
#define	BINLOG_DRIVER_INCLUDED

#include "binlog_event.h"
#include "protocol.h"

namespace mysql {
namespace system {

class Binary_log_driver
{
public:
  template <class FilenameT>
  Binary_log_driver(const FilenameT& filename = FilenameT(),
                    unsigned int offset = 0)
    : m_binlog_file_name(filename), m_binlog_offset(offset)
  {
  }

  virtual ~Binary_log_driver() {}

  /**
   * Connect to the binary log using previously declared connection parameters
   * @return Success or error code
   * @retval 0 Success
   * @retval >0 Error code (to be specified)
   */
  virtual int connect()= 0;
  virtual int connect(const std::string &filename, ulong position)= 0;
  /**
   * Blocking attempt to get the next binlog event from the stream
   * @param event [out] Pointer to a binary log event to be fetched.
   */
  virtual unsigned int wait_for_next_event(mysql::Binary_log_event **event)= 0;

  /**
   * Set the reader position
   * @param str The file name
   * @param position The file position
   *
   * @return False on success and True if an error occurred.
   */
  virtual int set_position(const std::string &str, unsigned long position)= 0;

  /**
   * Get the read position.
   *
   * @param[out] string_ptr Pointer to location where the
     filename will be stored.
   * @param[out] position_ptr Pointer to location where
     the position will be stored.
   *
   * @retval 0 Success
   * @retval >0 Error code
   */
  virtual int get_position(std::string *filename_ptr,
                           unsigned long *position_ptr) = 0;

  virtual int disconnect()= 0;
  Binary_log_event* parse_event(std::istream &sbuff, Log_event_header *header);

protected:
  std::string m_binlog_file_name;
  /**
   * Used each time the client reconnects to the server to specify an
   * offset position.
   */
  unsigned long m_binlog_offset;
};

} // namespace mysql::system
} // namespace mysql
#endif	/* BINLOG_DRIVER_INCLUDED */
