/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#include <ndb_global.h>

#include <BaseString.hpp>
#include <LogBuffer.hpp>
#include <OutputStream.hpp>

BufferedOutputStream::BufferedOutputStream(LogBuffer *plogBuf) {
  logBuf = plogBuf;
  assert(logBuf != nullptr);
}

int BufferedOutputStream::print(const char *fmt, ...) {
  char buf[1];
  va_list ap;
  int len = 0;
  int ret = 0;

  va_start(ap, fmt);
  len = BaseString::vsnprintf(buf, sizeof(buf), fmt, ap);
  assert(len >= 0);
  va_end(ap);

  va_start(ap, fmt);
  ret = logBuf->append(fmt, ap, (size_t)len);
  va_end(ap);
  return ret;
}

int BufferedOutputStream::println(const char *fmt, ...) {
  char buf[1];
  va_list ap;
  int len = 0;
  int ret = 0;

  va_start(ap, fmt);
  len = BaseString::vsnprintf(buf, sizeof(buf), fmt, ap);
  assert(len >= 0);
  va_end(ap);

  va_start(ap, fmt);
  ret = logBuf->append(fmt, ap, (size_t)len, true);
  va_end(ap);
  return ret;
}

int BufferedOutputStream::write(const void *buf, size_t len) {
  return (int)(logBuf->append(buf, len));
}

FileOutputStream::FileOutputStream(FILE *file) { f = file; }

int FileOutputStream::print(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  const int ret = vfprintf(f, fmt, ap);
  va_end(ap);
  return ret;
}

int FileOutputStream::println(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  const int ret = vfprintf(f, fmt, ap);
  va_end(ap);
  return ret + fprintf(f, "\n");
}

int FileOutputStream::write(const void *buf, size_t len) {
  return (int)fwrite(buf, len, 1, f);
}

SocketOutputStream::SocketOutputStream(const NdbSocket &socket,
                                       unsigned write_timeout_ms)
    : m_socket(socket),
      m_timeout_ms(write_timeout_ms),
      m_timedout(false),
      m_timeout_remain(write_timeout_ms) {}

int SocketOutputStream::print(const char *fmt, ...) {
  va_list ap;
  char buf[1000];
  char *buf2 = buf;
  size_t size;

  if (fmt != nullptr && fmt[0] != 0) {
    va_start(ap, fmt);
    size = BaseString::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    /* Check if the output was truncated */
    if (size > sizeof(buf)) {
      buf2 = (char *)malloc(size);
      if (buf2 == nullptr) {
        return -1;
      }

      va_start(ap, fmt);
      BaseString::vsnprintf(buf2, size, fmt, ap);
      va_end(ap);
    }
  } else
    return 0;

  const int ret = write(buf2, size);
  if (buf2 != buf) {
    free(buf2);
  }
  return ret;
}

int SocketOutputStream::println(const char *fmt, ...) {
  va_list ap;
  char buf[1000];
  char *buf2 = buf;
  size_t size;

  if (fmt != nullptr && fmt[0] != 0) {
    va_start(ap, fmt);
    size = BaseString::vsnprintf(buf, sizeof(buf), fmt, ap) +
           1;  // extra byte for '/n'
    va_end(ap);

    /* Check if the output was truncated */
    if (size > sizeof(buf)) {
      buf2 = (char *)malloc(size);
      if (buf2 == nullptr) return -1;
      va_start(ap, fmt);
      BaseString::vsnprintf(buf2, size, fmt, ap);
      va_end(ap);
    }
  } else {
    size = 1;
  }
  buf2[size - 1] = '\n';

  int ret = write(buf2, size);
  if (buf2 != buf) free(buf2);
  return ret;
}

int SocketOutputStream::write(const void *buf, size_t len) {
  if (timedout()) return -1;

  int time = 0;
  int ret = m_socket.write(m_timeout_ms, &time, (const char *)buf, (int)len);
  if (ret >= 0) {
    m_timeout_remain -= time;
  }

  if ((ret < 0 && errno == SOCKET_ETIMEDOUT) || m_timeout_remain <= 0) {
    m_timedout = true;
    ret = -1;
  }
  return ret;
}

#include <UtilBuffer.hpp>

BufferSocketOutputStream::BufferSocketOutputStream(const NdbSocket &socket,
                                                   unsigned write_timeout_ms)
    : SocketOutputStream(socket, write_timeout_ms), m_buffer(*new UtilBuffer) {}

BufferSocketOutputStream::~BufferSocketOutputStream() { delete &m_buffer; }

int BufferSocketOutputStream::print(const char *fmt, ...) {
  char buf[1];
  va_list ap;
  int len;
  char *pos;

  // Find out length of string
  va_start(ap, fmt);
  len = BaseString::vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  // Allocate a temp buffer for the string
  UtilBuffer tmp;
  if (tmp.append(len + 1) == nullptr) return -1;

  // Print to temp buffer
  va_start(ap, fmt);
  len = BaseString::vsnprintf((char *)tmp.get_data(), len + 1, fmt, ap);
  va_end(ap);

  // Grow real buffer so it can hold the string
  if ((pos = (char *)m_buffer.append(len)) == nullptr) return -1;

  // Move everything except ending 0 to real buffer
  memcpy(pos, tmp.get_data(), tmp.length() - 1);

  return 0;
}

int BufferSocketOutputStream::println(const char *fmt, ...) {
  char buf[1];
  va_list ap;
  int len;
  char *pos;

  // Find out length of string
  va_start(ap, fmt);
  len = BaseString::vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  // Grow buffer so it can hold the string and the new line
  if ((pos = (char *)m_buffer.append(len + 1)) == nullptr) return -1;

  // Print string to buffer
  va_start(ap, fmt);
  len = BaseString::vsnprintf((char *)pos, len + 1, fmt, ap);
  va_end(ap);

  // Add newline
  pos += len;
  *pos = '\n';

  return 0;
}

int BufferSocketOutputStream::write(const void *buf, size_t len) {
  return m_buffer.append(buf, len);
}

void BufferSocketOutputStream::flush() {
  if (m_buffer.length() == 0) return;
  int elapsed = 0;
  if (m_socket.write(m_timeout_ms, &elapsed, (const char *)m_buffer.get_data(),
                     m_buffer.length()) != 0) {
    fprintf(stderr, "Failed to flush buffer to socket, errno: %d\n", errno);
  }

  m_buffer.clear();
}

StaticBuffOutputStream::StaticBuffOutputStream(char *buff, size_t size)
    : m_buff(buff), m_size(size), m_offset(0) {
  reset();
}

StaticBuffOutputStream::~StaticBuffOutputStream() {}

int StaticBuffOutputStream::print(const char *fmt, ...) {
  va_list ap;
  size_t remain = m_size - m_offset;
  assert(m_offset < m_size);

  // Print to buffer
  va_start(ap, fmt);
  int idealLen = BaseString::vsnprintf(m_buff + m_offset, remain, fmt, ap);
  va_end(ap);

  if (idealLen >= 0) {
    m_offset = MIN(m_offset + idealLen, m_size - 1);
    assert(m_buff[m_offset] == '\0');
    return 0;
  }
  assert(m_buff[m_offset] == '\0');
  return -1;
}

int StaticBuffOutputStream::println(const char *fmt, ...) {
  va_list ap;
  size_t remain = m_size - m_offset;
  assert(m_offset < m_size);

  // Print to buffer
  va_start(ap, fmt);
  int idealLen = BaseString::vsnprintf(m_buff + m_offset, remain, fmt, ap);
  va_end(ap);

  if (idealLen >= 0) {
    m_offset = MIN(m_offset + idealLen, m_size - 1);

    return print("\n");
  }
  assert(m_buff[m_offset] == '\0');
  return -1;
}

int StaticBuffOutputStream::write(const void *buf, size_t len) {
  /* Will write as much as we can, ensuring space for
   * terminating null
   */
  assert(m_offset < m_size);
  size_t remain = m_size - m_offset;

  if (remain > 1) {
    size_t copySz = MIN(len, remain - 1);
    memcpy(m_buff, buf, copySz);
    m_offset += copySz;
    m_buff[m_offset] = '\0';
    return copySz;
  }

  assert(m_buff[m_offset] == '\0');
  return 0;
}
