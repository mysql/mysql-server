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

#ifndef FILE_DRIVER_INCLUDED
#define	FILE_DRIVER_INCLUDED

#include "binlog_api.h"
#include "binlog_driver.h"
#include "protocol.h"
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAGIC_NUMBER_SIZE 4

namespace mysql {
namespace system {

class Binlog_file_driver
  : public Binary_log_driver
{
public:
  template <class TFilename>
  Binlog_file_driver(const TFilename& filename = TFilename(),
                     unsigned int offset = 0)
    : Binary_log_driver(filename, offset)
  {
  }

    int connect();
    int disconnect();
    unsigned int wait_for_next_event(mysql::Binary_log_event **event);
    int set_position(const std::string &str, unsigned long position);
    int get_position(std::string *str, unsigned long *position);
    int connect(const std::string &filename, ulong offset);
private:

    unsigned long m_binlog_file_size;

    /*
      Bytes that has been read so for from the file.
      Updated after every event is read.
    */
    unsigned long m_bytes_read;

    std::ifstream m_binlog_file;

    Log_event_header m_event_log_header;
};

} // namespace mysql::system
} // namespace mysql

#endif	/* FILE_DRIVER_INCLUDED */
