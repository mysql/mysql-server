/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.
    Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "ndb_global.h"

#include "InputStream.hpp"
#include "util/cstrbuf.h"

FileInputStream Stdin(stdin);

FileInputStream::FileInputStream(FILE *file) : f(file) {}

char *FileInputStream::gets(char *buf, int bufLen) {
  if (!feof(f)) {
    return fgets(buf, bufLen, f);
  }
  return nullptr;
}

SocketInputStream::SocketInputStream(const NdbSocket &socket,
                                     unsigned read_timeout_ms)
    : m_socket(socket) {
  m_startover = true;
  m_timeout_remain = m_timeout_ms = read_timeout_ms;

  m_timedout = false;
}

char *SocketInputStream::gets(char *buf, int bufLen) {
  if (timedout()) return nullptr;
  assert(bufLen >= 2);
  int offset = 0;
  if (m_startover) {
    buf[0] = '\0';
    m_startover = false;
  } else
    offset = (int)strlen(buf);

  int time = 0;
  int res = m_socket.readln(m_timeout_remain, &time, buf + offset,
                            bufLen - offset, m_mutex);

  if (res >= 0) m_timeout_remain -= time;
  if (res == 0 || m_timeout_remain <= 0) {
    m_timedout = true;
    buf[0] = 0;
    return buf;
  }

  m_startover = true;

  if (res == -1) {
    return nullptr;
  }

  return buf;
}

char *RewindInputStream::gets(char *buf, int bufLen) {
  if (m_first) {
    m_first = false;
    cstrbuf buffer({buf, buf + bufLen});
    buffer.append(m_buf);
    buffer.append("\n");
    require(!buffer.is_truncated());
    return buf;
  }

  return m_stream.gets(buf, bufLen);
}
