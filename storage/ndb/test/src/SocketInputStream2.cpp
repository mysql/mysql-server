/* Copyright 2008 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include <SocketInputStream2.hpp>

#include <NdbOut.hpp>

bool
SocketInputStream2::gets(BaseString& str)
{
  if (get_buffered_line(str))
    return true;

  char buf[16];
  do {
    ssize_t read_res = read_socket(buf, sizeof(buf));
    if (read_res == -1)
      return false;

    if (!add_buffer(buf, read_res))
      return false;

    if (get_buffered_line(str))
      return true;

  } while(true);

  abort(); // Should never come here
  return false;
};


bool
SocketInputStream2::has_data_to_read()
{
  const int res = ndb_poll(m_socket, true, false, false,
                           m_read_timeout * 1000);

  if (res == 1)
    return true; // Yes, there was data

  if (res == 0)
    return false; // Timeout occured

  assert(res == -1);
  return false;
}


ssize_t
SocketInputStream2::read_socket(char* buf, size_t len)
{
  if (!has_data_to_read())
    return -1;

  size_t read_res = my_recv(m_socket, buf, len, 0);
  if (read_res == 0)
    return -1; // Has data to read but only EOF received

  return read_res;
}


bool
SocketInputStream2::get_buffered_line(BaseString& str)
{
  char *start, *ptr;
  char *end = (char*)m_buffer.get_data() + m_buffer.length();
  start = ptr =(char*)m_buffer.get_data() + m_buffer_read_pos;

  while(ptr && ptr < end && *ptr)
  {
    if (*ptr == '\n')
    {
      size_t len = ptr-start;
      /* Found end of line, return this part of the buffer */
      str.assign(start, len);

      /*
         Set new read position in buffer, increase with
         one to step past '\n'
      */
      m_buffer_read_pos += (len + 1);

      return true;
    }
    ptr++;
  }
  return false;
}


bool
SocketInputStream2::add_buffer(char* buf, ssize_t len)
{
  // ndbout_c("add_buffer: '%.*s'", len, buf);
  if (m_buffer.append(buf, len) != 0)
    return false;
  return true;
}
