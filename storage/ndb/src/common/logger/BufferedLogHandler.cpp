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

#include "BufferedLogHandler.hpp"
#include <basestring_vsnprintf.h>

struct ThreadData
{
  BufferedLogHandler* buf_loghandler;
};

void* async_log_function(void* args)
{
  ThreadData* data = (ThreadData*)args;
  BufferedLogHandler* buf_loghandler = data->buf_loghandler;


  while (!buf_loghandler->isStopSet())
  {
    buf_loghandler->writeToDestLogHandler();
  }

  // print left over messages, if any
  while (buf_loghandler->writeToDestLogHandler());

  // print lost count in the end, if any
  buf_loghandler->writeLostMsgDestLogHandler();

  delete data;

  return NULL;
}

BufferedLogHandler::BufferedLogHandler(LogHandler* dest_loghandler)
 : LogHandler(), m_dest_loghandler(dest_loghandler),
   m_log_threadvar(NULL), m_stop_logging(false)
{
  m_logbuf = new LogBuffer(32768, new MessageStreamLostMsgHandler()); // 32kB
  ThreadData *thr_data = new ThreadData();
  thr_data->buf_loghandler = this;

  m_log_threadvar = NdbThread_Create(async_log_function,
                   (void**)thr_data,
                   0,
                   (char*)"async_local_log_thread",
                   NDB_THREAD_PRIO_MEAN);
  if (m_log_threadvar == NULL)
  {
    abort();
  }
}

BufferedLogHandler::~BufferedLogHandler()
{
  m_stop_logging = true;
  NdbThread_WaitFor(m_log_threadvar, NULL);
  NdbThread_Destroy(&m_log_threadvar);
  delete m_logbuf;
}

bool
BufferedLogHandler::open()
{
  return true;
}

bool
BufferedLogHandler::close()
{
  return true;
}

bool
BufferedLogHandler::is_open()
{
  if (m_log_threadvar == NULL)
  {
    return false;
  }
  return true;
}

//
// PROTECTED
//
void
BufferedLogHandler::writeHeader(const char* pCategory,
                                Logger::LoggerLevel level,
                                time_t now)
{
  /**
   * Add log level, timestamp, category length to m_log_fixedpart and
   * category to m_log_varpart.
   */
  m_log_fixedpart.level = level;
  m_log_fixedpart.log_timestamp = now;

  size_t pCategory_len = strlen(pCategory);
  m_log_fixedpart.varpart_length[0] = pCategory_len;
  memcpy(m_log_varpart, pCategory, pCategory_len);
}

void
BufferedLogHandler::writeMessage(const char* pMsg)
{
  // add message length to m_log_fixedpart and the message to m_log_varpart
  size_t pMsg_len = strlen(pMsg);

  m_log_fixedpart.varpart_length[1] = pMsg_len;
  memcpy(m_log_varpart + m_log_fixedpart.varpart_length[0], pMsg, pMsg_len);
}

void
BufferedLogHandler::writeFooter()
{
  // add the LogHandler::append() parameters to the log buffer
  /**
   * LogBuffer contents:
   * ([log-fixed-part] [log-var-part])*
   */
  size_t total_log_size = sizeof(LogMessageFixedPart) +
      m_log_fixedpart.varpart_length[0] +
      m_log_fixedpart.varpart_length[1];

  memcpy(m_to_append, &m_log_fixedpart, sizeof(LogMessageFixedPart));
  memcpy(m_to_append + sizeof(LogMessageFixedPart), m_log_varpart,
         m_log_fixedpart.varpart_length[0] +
         m_log_fixedpart.varpart_length[1]);

  m_logbuf->append((void*)&m_to_append, total_log_size);
}

bool
BufferedLogHandler::isStopSet()
{
  return m_stop_logging;
}

bool
BufferedLogHandler::setParam(const BaseString &param, const BaseString &value)
{
  return true;
}

bool
BufferedLogHandler::writeToDestLogHandler()
{
  char category[LogHandler::MAX_HEADER_LENGTH + 1];
  char msg[MAX_LOG_MESSAGE_SIZE + 1];
  LogMessageFixedPart log_fixed_part;

  if (m_logbuf->get((char*)&log_fixed_part, sizeof(LogMessageFixedPart)) != 0)
  {
    assert(log_fixed_part.varpart_length[0] <= LogHandler::MAX_HEADER_LENGTH);
    assert(log_fixed_part.varpart_length[1] <= MAX_LOG_MESSAGE_SIZE);
    m_logbuf->get(category, log_fixed_part.varpart_length[0]);
    m_logbuf->get(msg, log_fixed_part.varpart_length[1]);
    category[log_fixed_part.varpart_length[0]] = '\0';
    msg[log_fixed_part.varpart_length[1]] = '\0';

    m_dest_loghandler->append(category, log_fixed_part.level, msg,
                            log_fixed_part.log_timestamp);
    return true;
  }
  return false;
}

void
BufferedLogHandler::writeLostMsgDestLogHandler()
{
  size_t lost_count = m_logbuf->getLostCount();
  char category[LogHandler::MAX_HEADER_LENGTH + 1];
  char msg[MAX_LOG_MESSAGE_SIZE + 1];

  if (lost_count)
  {
    strcpy(category, "MgmtSrvr");
    Logger::LoggerLevel level = Logger::LL_INFO;
    BaseString::snprintf(msg, MAX_LOG_MESSAGE_SIZE,
                         "*** %lu MESSAGES LOST ***",
                         (unsigned long)lost_count);
    time_t now = ::time((time_t*)NULL);
    m_dest_loghandler->append(category, level, msg, now);
  }
}

size_t
MessageStreamLostMsgHandler::getSizeOfLostMsg(size_t lost_bytes, size_t lost_msgs)
{
  size_t lost_msg_len = sizeof(BufferedLogHandler::LogMessageFixedPart) +
      basestring_snprintf(NULL, 0, m_lost_msg_fmt, lost_msgs) +
      strlen(m_category);
  return lost_msg_len;
}

bool
MessageStreamLostMsgHandler::writeLostMsg(char* buf, size_t buf_size, size_t lost_bytes, size_t lost_msgs)
{
  BufferedLogHandler::LogMessageFixedPart lost_message_fixedpart;
  lost_message_fixedpart.level = Logger::LL_DEBUG;
  lost_message_fixedpart.log_timestamp = time((time_t*)NULL);

  lost_message_fixedpart.varpart_length[0] = strlen(m_category);
  lost_message_fixedpart.varpart_length[1] =
      basestring_snprintf(NULL, 0, m_lost_msg_fmt, lost_msgs);

  const size_t sz_fixedpart = sizeof(lost_message_fixedpart);
  memcpy(buf, &lost_message_fixedpart, sz_fixedpart);
  memcpy(buf + sz_fixedpart, m_category, strlen(m_category));
  basestring_snprintf(buf + sz_fixedpart + strlen(m_category),
                      lost_message_fixedpart.varpart_length[1],
                      m_lost_msg_fmt, lost_msgs);
  return true;
}
