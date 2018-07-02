/*
   Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef STORAGE_NDB_INCLUDE_LOGGER_BUFFEREDLOGHANDLER_HPP_
#define STORAGE_NDB_INCLUDE_LOGGER_BUFFEREDLOGHANDLER_HPP_

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
   * @param logbuf Pointer to the log buffer where log messages should be written into.
   * @param dest_loghandler Pointer to the destination log handler i.e the log handler
   * to which the log messages taken from the log buffer are passed.
   */
  BufferedLogHandler(LogHandler* dest_loghandler);
  /**
   * Destructor.
   */
  virtual ~BufferedLogHandler();

  virtual bool open();
  virtual bool close();

  virtual bool is_open();

  virtual bool setParam(const BaseString &param, const BaseString &value);
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
  STATIC_CONST( MAX_VARPART_SIZE = MAX_HEADER_LENGTH + MAX_LOG_MESSAGE_SIZE );

protected:
  virtual void writeHeader(const char* pCategory, Logger::LoggerLevel level,
                           time_t now);
  virtual void writeMessage(const char* pMsg);
  virtual void writeFooter();

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
  const char* m_lost_msg_fmt;
  const char* m_category;

public:
  MessageStreamLostMsgHandler(): m_lost_msg_fmt("*** %u MESSAGES LOST ***"),
  m_category("MgmtSrvr")
  {
  }
  /* Return size in bytes which must be appended to describe the lost messages */
  size_t getSizeOfLostMsg(size_t lost_bytes, size_t lost_msgs);

  /* Write lost message summary into the buffer for the lost message summary */
  bool writeLostMsg(char* buf, size_t buf_size, size_t lost_bytes, size_t lost_msgs);

  ~MessageStreamLostMsgHandler() {}
};

#endif /* STORAGE_NDB_INCLUDE_LOGGER_BUFFEREDLOGHANDLER_HPP_ */
