/*
   Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef LOG_BUFFER_H
#define LOG_BUFFER_H

#include <ndb_global.h>

#include <NdbMutex.h>

class LostMsgHandler
{
public:
  /* Return size in bytes which must be appended to describe the lost messages */
  virtual size_t getSizeOfLostMsg(size_t lost_bytes, size_t lost_msgs) = 0;

  /* Write lost message summary into the buffer for the lost message summary */
  virtual bool writeLostMsg(char* buf, size_t buf_size, size_t lost_bytes, size_t lost_msgs) = 0;

  virtual ~LostMsgHandler() {};
};

class ByteStreamLostMsgHandler : public LostMsgHandler
{
private:
  const char* m_lost_msg_fmt;

public:
  ByteStreamLostMsgHandler(): m_lost_msg_fmt("\n*** %u BYTES LOST ***\n")
  {
  }
  /* Return size in bytes which must be appended to describe the lost messages */
  size_t getSizeOfLostMsg(size_t lost_bytes, size_t lost_msgs);

  /* Write lost message summary into the buffer for the lost message summary */
  bool writeLostMsg(char* buf, size_t buf_size, size_t lost_bytes, size_t lost_msgs);

  ~ByteStreamLostMsgHandler() {}
};

/**
 * Suitable for multiple producers and multiple consumers that are
 * non-blocking, i.e we don't want either of the threads to hang indefinitely.
 *
 * The producer calls append() to put data into the log buffer, while the
 * consumer calls get() to remove data from the log buffer.
 * Appending of both binary data and strings are supported.
 * Appending data of size greater that the log buffer is not possible- the only
 * workarounds are to increase the size of the log buffer or to trim the data
 * to be appended.
 *
 * 1. Appending data to the log buffer is done without hanging(non-blocking),
 *    but there is a possibility of data loss.
 *    Data is appended to the buffer if it *all* fits, otherwise nothing is
 *    appended.
 * 2. Lost data is calculated, and when space for some new entry becomes
 *    available, a 'lost bytes' message is added to the buffer before the new
 *    entry.
 *
 * get() can either be blocking or non-blocking depending on what the user wants.
 * get() blocks for at most 'timeout_ms' when 'timeout_ms' is non-zero
 * and there's no data in the log buffer.
 * get() is non-blocking when 'timeout_ms' is zero.
 */

class LogBuffer
{

public:

  /**
   * @param size Size of log buffer in bytes
   * @param lost_msg_handler Delegate to handle lost messages
   */
  explicit LogBuffer(size_t size= 32768,
                     LostMsgHandler* lost_msg_handler=
                         new ByteStreamLostMsgHandler());

  ~LogBuffer();

  /**
   * Append a c-string to the buffer.
   * Thread safe.
   *
   * @param fmt Format string that follows the same specifications
   * as format in printf
   * @param ap Reference to list holding variable number of
   * arguments that follows the same specifications as ap
   * in vprintf
   * @param len Length of the string
   * @param append_ln Set to true if a new line must be appended at the
   * end of the string.
   * @return Number of characters appended on success and
   * 0 if there's insufficient space in the log buffer.
   */
  int append(const char* fmt, va_list ap, size_t len, bool append_ln=false)
    ATTRIBUTE_FORMAT(printf, 2, 0);
  /**
   * Append data to the buffer.
   * Thread safe.
   *
   * @param buf Pointer to data to be appended
   * @param size Number of bytes starting from buf to be appended
   * @return Number of characters appended on success and
   * 0 if there's insufficient space in the log buffer.
   */
  size_t append(void* buf, size_t size);

  /**
   * Remove data from the log buffer and copy to "buf".
   * Thread safe.
   *
   * @param buf Pointer to buffer to which the data retrieved from
   * the log buffer needs to be copied into.
   * @param size Maximum number of bytes that can be copied to buf
   * @param timeout_ms Number of milliseconds the calling thread is
   * suspended if the log buffer is empty. Default value is 5000 ms.
   * @return Number of bytes got from the log buffer on success,
   * 0 if no data was retrieved, i.e log buffer was empty even after
   * waiting for timeout_ms
   */
  size_t get(char* buf, size_t size, uint timeout_ms = 5000);

  /**
   * @return Data in bytes the log buffer holds currently, not thread safe
   */
  size_t getSize() const;

  /**
   * @return Number of bytes of data lost since the previous loss.
   * @note Not synchronized
   */
  size_t getLostCount() const;


private:
  char* m_log_buf; // pointer to the start of log buffer memory
  size_t m_max_size; // max. number of bytes that can fit
  char* m_read_ptr; // pointer to a byte in the log buffer to read from

  /**
   * Candidate for pointer to memory to which the next write could
   * happen.
   */
  char* m_write_ptr;

  /**
   * Logical end of buffer while reading.
   * It's the last valid byte that can be read.
   */
  char* m_buf_end;

  /**
   * Pointer to last byte of the buffer. Data is never written to
   * this location. It is only an extra unused byte to indicate the
   * end of the log buffer in memory. */
  char* m_top;
  size_t m_size; // number of bytes used

  // number of bytes of data lost since the previous loss
  size_t m_lost_bytes;

  // the number of unsuccessful append() calls
  size_t m_lost_messages;

  LostMsgHandler* m_lost_msg_handler;

  NdbMutex *m_mutex;
  struct NdbCondition* m_cond;

  /**
   * Given a number of bytes to write to the log buffer,
   * return a pointer to memory in the log buffer into which
   * the bytes can be written into.
   * Not thread safe
   *
   * @param bytes The number of bytes to be written to the buffer.
   * @return Valid pointer on success, NULL if there's not enough space.
   */
  char* getWritePtr(size_t bytes) const;

  /**
   * Used to update the write pointer(m_write_ptr).
   * Not thread safe
   * @param bytes Number of bytes written to write_ptr.
   */
  void updateWritePtr(size_t bytes);

  /**
   * Wraps the write pointer to the beginning of log buffer
   * Not thread safe
   */
  void wrapWritePtr();

  /**
   * First step to check if there's enough space in the log buffer
   * and append "lost message" to the log buffer if required.
   * Called internally in both append() functions before actually
   * appending data to the log buffer.
   * Not synchronized
   * @param size Number of bytes occupied by the data to be appended.
   * Equal to the size parameter in append(void* buf, size_t size);
   * @return true on success, fail on failure.
   */
  bool checkForBufferSpace(size_t size);

  /**
   * Function to check internal consistency of log buffer.
   * @return true on success, false on failure.
   */
  bool checkInvariants() const;

};

#endif
