/*
   Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#ifndef STORAGE_NDB_INCLUDE_LOGGER_BUFFEREDLOGHANDLER_HPP_
#define STORAGE_NDB_INCLUDE_LOGGER_BUFFEREDLOGHANDLER_HPP_

#include <time.h>

#include "LogHandler.hpp"
#include <LogBuffer.hpp>
#include <NdbThread.h>

/**
 * 1) Creates a thread
 * 2) Logs messages to a LogBuffer object
 * 3) Reads them out in the thread and passes them to the destination LogHandler
 */
class BufferedLogHandler : public LogHandler
{
public:
  /**
   * Constructor
   * @param dest_loghandler Pointer to the destination log handler i.e the log handler
   * to which the log messages taken from the log buffer are passed.
   */
  BufferedLogHandler(LogHandler* dest_loghandler);
  /**
   * Destructor.
   */
  ~BufferedLogHandler() override;

  bool open() override;
  bool close() override;

  bool is_open() override;

  bool setParam(const BaseString &param, const BaseString &value) override;
  /**
   * Check if logging needs to be stopped.
   * @return true if logging has to be stopped, false otherwise.
   */
  virtual bool isStopSet();
  bool writeToDestLogHandler();
  void writeLostMsgDestLogHandler();

  struct LogMessageFixedPart
  {
    Logger::LoggerLevel level;
    time_t log_timestamp;
    size_t varpart_length[2]; // 0: length of category, 1: length of message
  };
  static constexpr Uint32 MAX_VARPART_SIZE = MAX_HEADER_LENGTH + MAX_LOG_MESSAGE_SIZE;

protected:
  void writeHeader(const char* pCategory, Logger::LoggerLevel level,
                   time_t now) override;
  void writeMessage(const char* pMsg) override;
  void writeFooter() override;

private:
  /** Prohibit*/
  BufferedLogHandler(const BufferedLogHandler&);
  BufferedLogHandler operator = (const BufferedLogHandler&);
  bool operator == (const BufferedLogHandler&);

  static void* async_log_func(void* args);

  LogBuffer* m_logbuf;
  // destination log handler
  LogHandler* m_dest_loghandler;

  LogMessageFixedPart m_log_fixedpart;

  // holds category and the log message
  char m_log_varpart[MAX_VARPART_SIZE];
  char m_to_append[sizeof(LogMessageFixedPart) +
                   (sizeof(char) * MAX_VARPART_SIZE)];
  NdbThread* m_log_threadvar;
  bool m_stop_logging;
};

/**
 * Custom LostMsgHandler for mgmd lost log messages, the "lost message"
 * is written in the same format as a regular log message in the log buffer.
 * E.g if five log messages are lost, the following is printed in the
 * cluster log:
 *
 * 2018-05-09 15:56:15 [MgmtSrvr] INFO     -- *** 5 MESSAGES LOST ***
 *
 */
class MessageStreamLostMsgHandler : public LostMsgHandler
{
private:
  const char* m_category;

public:
 MessageStreamLostMsgHandler() : m_category("MgmtSrvr") {}
 /* Return size in bytes which must be appended to describe the lost messages */
 size_t getSizeOfLostMsg(size_t lost_bytes, size_t lost_msgs) override;

 /* Write lost message summary into the buffer for the lost message summary */
 bool writeLostMsg(char* buf,
                   size_t buf_size,
                   size_t lost_bytes,
                   size_t lost_msgs) override;

 ~MessageStreamLostMsgHandler() override {}
};

#endif /* STORAGE_NDB_INCLUDE_LOGGER_BUFFEREDLOGHANDLER_HPP_ */
