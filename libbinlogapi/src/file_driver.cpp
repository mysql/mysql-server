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

#include "file_driver.h"

namespace mysql { namespace system {
using namespace std;

 int Binlog_file_driver::connect()
  {
    struct stat stat_buff;

    char magic[]= {0xfe, 0x62, 0x69, 0x6e, 0};
    char magic_buf[MAGIC_NUMBER_SIZE];

    // Get the file size.
    if (stat(m_binlog_file_name.c_str(), &stat_buff) == -1)
      return ERR_FAIL;                          // Can't stat binlog file.
    m_binlog_file_size= stat_buff.st_size;

    m_binlog_file.exceptions(ifstream::failbit | ifstream::badbit |
                           ifstream::eofbit);
    try
    {
      // Check if the file can be opened for reading.
      m_binlog_file.open(m_binlog_file_name.c_str(), ios::in | ios::binary);

      // Check if a valid MySQL binlog file is provided, BINLOG_MAGIC.
      m_binlog_file.read(magic_buf, MAGIC_NUMBER_SIZE);

      if(memcmp(magic, magic_buf, MAGIC_NUMBER_SIZE))
        return ERR_FAIL;                        // Not a valid binlog file.

      // Reset the get pointer.
      //m_binlog_file.seekg(0, ios::beg );

      m_bytes_read= MAGIC_NUMBER_SIZE;

      // Read the first event to check the binlog version
      if (m_bytes_read < m_binlog_file_size && m_binlog_file.good())
      {
        Protocol_chunk<uint32_t> prot_timestamp(m_event_log_header.timestamp);
        Protocol_chunk<uint8_t> prot_type_code(m_event_log_header.type_code);

        m_binlog_file >> prot_timestamp
                      >> prot_type_code;

        if (m_event_log_header.type_code != FORMAT_DESCRIPTION_EVENT)
          return ERR_BINLOG_VERSION;
      }

    } catch (...)
    {
      return ERR_FAIL;
    }
    return ERR_OK;
  }

 int Binlog_file_driver::connect(const std::string &filename, ulong position)
  {
    struct stat stat_buff;

    char magic[]= {0xfe, 0x62, 0x69, 0x6e, 0};
    char magic_buf[MAGIC_NUMBER_SIZE];

    // Get the file size.
    if (stat(m_binlog_file_name.c_str(), &stat_buff) == -1)
      return ERR_FAIL;                          // Can't stat binlog file.
    m_binlog_file_size= stat_buff.st_size;

    m_binlog_file.exceptions(ifstream::failbit | ifstream::badbit |
                           ifstream::eofbit);
    try
    {
      // Check if the file can be opened for reading.
      m_binlog_file.open(m_binlog_file_name.c_str(), ios::in | ios::binary);

      // Check if a valid MySQL binlog file is provided, BINLOG_MAGIC.
      m_binlog_file.read(magic_buf, MAGIC_NUMBER_SIZE);

      if(memcmp(magic, magic_buf, MAGIC_NUMBER_SIZE))
        return ERR_FAIL;                        // Not a valid binlog file.

      m_bytes_read= MAGIC_NUMBER_SIZE;

      // Read the first event to check the binlog version
      if (m_bytes_read < m_binlog_file_size && m_binlog_file.good())
      {
        Protocol_chunk<uint32_t> prot_timestamp(m_event_log_header.timestamp);
        Protocol_chunk<uint8_t> prot_type_code(m_event_log_header.type_code);

        m_binlog_file >> prot_timestamp
                      >> prot_type_code;

        if (m_event_log_header.type_code != FORMAT_DESCRIPTION_EVENT)
          return ERR_BINLOG_VERSION;
      }

      m_binlog_file.seekg(position, ios::beg );

    } catch (...)
    {
      return ERR_FAIL;
    }

    m_bytes_read= position;
    return ERR_OK;
  }
  int Binlog_file_driver::disconnect()
  {
    m_binlog_file.close();
    return ERR_OK;
  }


  int Binlog_file_driver::set_position(const string &str, ulong position)
  {
    m_binlog_file.exceptions(ifstream::failbit | ifstream::badbit |
                           ifstream::eofbit);
    try
    {
      m_binlog_file.seekg(position, ios::beg );
    } catch(...)
    {
      return ERR_FAIL;
    }

    m_bytes_read= position;

    return ERR_OK;
  }


  int Binlog_file_driver::get_position(string *str, ulong *position)
  {
    m_binlog_file.exceptions(ifstream::failbit | ifstream::badbit |
                             ifstream::eofbit);
    *str= m_binlog_file_name;

   try
    {
      if(position)
      {
        std::streamoff binlog_file_pos= m_binlog_file.tellg();
        *position= (unsigned long)binlog_file_pos;
      }
    } catch(...)
    {
      return ERR_FAIL;
    }

    return ERR_OK;
  }


  unsigned int Binlog_file_driver::wait_for_next_event(mysql::Binary_log_event **event)
  {

    assert(m_binlog_file.tellg() >= 4 );
    m_binlog_file.exceptions(ifstream::failbit | ifstream::badbit |
                             ifstream::eofbit);

    try
    {
      if(m_bytes_read < m_binlog_file_size && m_binlog_file.good())
      {
        //Protocol_chunk<uint8_t> prot_marker(m_event_log_header.marker);
        Protocol_chunk<uint32_t> prot_timestamp(m_event_log_header.timestamp);
        Protocol_chunk<uint8_t> prot_type_code(m_event_log_header.type_code);
        Protocol_chunk<uint32_t> prot_server_id(m_event_log_header.server_id);
        Protocol_chunk<uint32_t>
          prot_event_length(m_event_log_header.event_length);
        Protocol_chunk<uint32_t>
          prot_next_position(m_event_log_header.next_position);
        Protocol_chunk<uint16_t> prot_flags(m_event_log_header.flags);

        m_binlog_file >> prot_timestamp
                      >> prot_type_code
                      >> prot_server_id
                      >> prot_event_length
                      >> prot_next_position
                      >> prot_flags;

        /*
        m_binlog_file.read(reinterpret_cast<char*>
                           (&m_event_log_header.timestamp),sizeof(uint32_t));
        m_binlog_file.read(reinterpret_cast<char*>
                           (&m_event_log_header.type_code),sizeof(uint8_t));
        m_binlog_file.read(reinterpret_cast<char*>
                           (&m_event_log_header.server_id),sizeof(uint32_t));
        m_binlog_file.read(reinterpret_cast<char*>
                          (&m_event_log_header.event_length),sizeof(uint32_t));
        m_binlog_file.read(reinterpret_cast<char*>(
                         (&m_event_log_header.next_position),sizeof(uint32_t));
        m_binlog_file.read(reinterpret_cast<char*>(&m_event_log_header.flags),
                           sizeof(uint16_t));
                           */

        *event= parse_event(* static_cast<std::istream*> (&m_binlog_file),
                            &m_event_log_header);
        // to get rid of gcc warnings
        std::streamoff binlog_file_pos= m_binlog_file.tellg();
        /*
          Correction.Except for the default case (above), this condition should
          always fail.
        */
        if (m_bytes_read + m_event_log_header.event_length !=
            (unsigned long)binlog_file_pos)
          m_binlog_file.seekg(m_bytes_read + m_event_log_header.event_length,
                              ios::beg);

        m_bytes_read= binlog_file_pos;

        if(*event)
        {
          if ((*event)->header()->type_code == FORMAT_DESCRIPTION_EVENT)
          {
            // Check server version and the checksum value
            int ret= check_checksum_value(event);
            return ret; // ret is the error code
          }
        }
        return ERR_OK;
      }
      else
        disconnect();
    } catch(...)
    {
      return ERR_FAIL;
    }
    return ERR_EOF;
  }

}
}

