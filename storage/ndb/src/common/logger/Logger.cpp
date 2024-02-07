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

#include "Logger.hpp"

#include <time.h>

#include <ConsoleLogHandler.hpp>
#include <FileLogHandler.hpp>
#include <LogHandler.hpp>
#include "LogHandlerList.hpp"

#ifdef _WIN32
#include "EventLogHandler.hpp"
#else
#include <SysLogHandler.hpp>
#endif

#include <BufferedLogHandler.hpp>

#include <portlib/ndb_localtime.h>

const char *Logger::LoggerLevelNames[] = {"ON      ", "DEBUG   ", "INFO    ",
                                          "WARNING ", "ERROR   ", "CRITICAL",
                                          "ALERT   ", "ALL     "};

/**
 * LogHandler that passes log events to every log handler
 * in a list.
 */
class InternalLogListHandler : public LogHandler {
 public:
  InternalLogListHandler()
      : m_pHandlerList(new LogHandlerList()), m_listMutex(NdbMutex_Create()) {}

  ~InternalLogListHandler() override {
    delete m_pHandlerList;
    NdbMutex_Destroy(m_listMutex);
  }

  bool open() override { return true; }

  bool close() override { return true; }

  bool is_open() override { return true; }

  bool setParam(const BaseString & /*param*/,
                const BaseString & /*value*/) override {
    return true;
  }

  /**
   * Here we pass the message to all bound in LogHandlers
   */
  void append(const char *pCategory, Logger::LoggerLevel level,
              const char *pMsg, time_t now) override {
    Guard g(m_listMutex);

    LogHandler *pHandler = nullptr;
    while ((pHandler = m_pHandlerList->next()) != nullptr) {
      pHandler->append(pCategory, level, pMsg, now);
    }
  }

  bool addHandler(LogHandler *pHandler) {
    Guard g(m_listMutex);
    assert(pHandler != nullptr);

    if (!pHandler->is_open() && !pHandler->open()) {
      // Failed to open
      return false;
    }

    if (!m_pHandlerList->add(pHandler)) return false;

    return true;
  }

  bool removeHandler(LogHandler *pHandler) {
    Guard g(m_listMutex);
    return m_pHandlerList->remove(pHandler);
  }

  void removeAllHandlers() {
    Guard g(m_listMutex);
    m_pHandlerList->removeAll();
  }

  void setRepeatFrequency(unsigned val) override {
    Guard g(m_listMutex);
    LogHandler *pHandler;
    while ((pHandler = m_pHandlerList->next()) != nullptr) {
      pHandler->setRepeatFrequency(val);
    }
  }

 protected:
  void writeHeader(const char * /*pCategory*/, Logger::LoggerLevel /*level*/,
                   time_t /*now*/) override {}
  void writeMessage(const char * /*pMsg*/) override {}
  void writeFooter() override {}

 private:
  LogHandlerList *m_pHandlerList;
  /* Mutex to protect concurrent list modification / iteration */
  NdbMutex *m_listMutex;

  /** Prohibit*/
  InternalLogListHandler(const InternalLogListHandler &);
  InternalLogListHandler operator=(const InternalLogListHandler &);
  bool operator==(const InternalLogListHandler &);
};

Logger::Logger()
    : m_log_mutex(NdbMutex_Create()),
      m_pCategory("Logger"),
      m_handler_creation_mutex(NdbMutex_Create()),
      m_internalLogListHandler(new InternalLogListHandler()),
      m_internalBufferedHandler(nullptr),
      m_pConsoleHandler(nullptr),
      m_pFileHandler(nullptr),
      m_pSyslogHandler(nullptr) {
  m_logHandler = m_internalLogListHandler;
  disable(LL_ALL);
  enable(LL_ON);
  enable(LL_INFO);
}
Logger::~Logger() {
  stopAsync();
  removeAllHandlers();
  delete m_internalLogListHandler;
  NdbMutex_Destroy(m_handler_creation_mutex);
  NdbMutex_Destroy(m_log_mutex);
}

void Logger::setCategory(const char *pCategory) {
  Guard g(m_log_mutex);
  m_pCategory = pCategory;
}

bool Logger::createConsoleHandler(NdbOut &out) {
  Guard g(m_handler_creation_mutex);

  if (m_pConsoleHandler) return true;  // Ok, already exist

  LogHandler *log_handler = new ConsoleLogHandler(out);
  if (!log_handler) return false;

  if (!addHandler(log_handler)) {
    delete log_handler;
    return false;
  }

  m_pConsoleHandler = log_handler;
  return true;
}

void Logger::removeConsoleHandler() { removeHandler(m_pConsoleHandler); }

#ifdef _WIN32
bool Logger::createEventLogHandler(const char *source_name) {
  Guard g(m_handler_creation_mutex);

  LogHandler *log_handler = new EventLogHandler(source_name);
  if (!log_handler) return false;

  if (!addHandler(log_handler)) {
    delete log_handler;
    return false;
  }

  return true;
}
#endif

bool Logger::createFileHandler(char *filename) {
  Guard g(m_handler_creation_mutex);

  if (m_pFileHandler) return true;  // Ok, already exist

  LogHandler *log_handler = new FileLogHandler(filename);
  if (!log_handler) return false;

  if (!addHandler(log_handler)) {
    delete log_handler;
    return false;
  }

  m_pFileHandler = log_handler;
  return true;
}

void Logger::removeFileHandler() { removeHandler(m_pFileHandler); }

bool Logger::createSyslogHandler() {
#ifdef _WIN32
  return false;
#else
  Guard g(m_handler_creation_mutex);

  if (m_pSyslogHandler) return true;  // Ok, already exist

  LogHandler *log_handler = new SysLogHandler();
  if (!log_handler) return false;

  if (!addHandler(log_handler)) {
    delete log_handler;
    return false;
  }

  m_pSyslogHandler = log_handler;
  return true;
#endif
}

void Logger::removeSyslogHandler() { removeHandler(m_pSyslogHandler); }

void Logger::startAsync(unsigned buffer_kb) {
  Guard g(m_log_mutex);

  if (m_internalBufferedHandler == nullptr) {
    BufferedLogHandler *blh =
        new BufferedLogHandler(m_internalLogListHandler,
                               false, /* m_internalLogListHandler not owned */
                               m_pCategory, buffer_kb);
    if (blh == nullptr) {
      abort();
    }

    /* No repeat filtering in the Buffered Handler */
    blh->setRepeatFrequency(0);

    m_internalBufferedHandler = blh;
    m_logHandler = m_internalBufferedHandler;
  }
}

void Logger::stopAsync() {
  Guard g(m_log_mutex);
  if (m_internalBufferedHandler != nullptr) {
    delete m_internalBufferedHandler;
    m_internalBufferedHandler = nullptr;
    m_logHandler = m_internalLogListHandler;
  }
}

bool Logger::addHandler(LogHandler *pHandler) {
  return m_internalLogListHandler->addHandler(pHandler);
}

bool Logger::removeHandler(LogHandler *pHandler) {
  Guard g(m_handler_creation_mutex);
  int rc = false;
  if (pHandler != nullptr) {
    if (pHandler == m_pConsoleHandler) m_pConsoleHandler = nullptr;
    if (pHandler == m_pFileHandler) m_pFileHandler = nullptr;
    if (pHandler == m_pSyslogHandler) m_pSyslogHandler = nullptr;

    rc = m_internalLogListHandler->removeHandler(pHandler);
  }

  return rc;
}

void Logger::removeAllHandlers() {
  Guard g(m_handler_creation_mutex);
  m_internalLogListHandler->removeAllHandlers();

  m_pConsoleHandler = nullptr;
  m_pFileHandler = nullptr;
  m_pSyslogHandler = nullptr;
}

bool Logger::isEnable(LoggerLevel logLevel) const {
  Guard g(m_log_mutex);
  if (logLevel == LL_ALL) {
    for (unsigned i = 1; i < MAX_LOG_LEVELS; i++)
      if (!m_logLevels[i]) return false;
    return true;
  }
  return m_logLevels[logLevel];
}

void Logger::enable(LoggerLevel logLevel) {
  Guard g(m_log_mutex);
  if (logLevel == LL_ALL) {
    for (unsigned i = 0; i < MAX_LOG_LEVELS; i++) {
      m_logLevels[i] = true;
    }
  } else {
    m_logLevels[logLevel] = true;
  }
}

void Logger::enable(LoggerLevel fromLogLevel, LoggerLevel toLogLevel) {
  Guard g(m_log_mutex);
  if (fromLogLevel > toLogLevel) {
    LoggerLevel tmp = toLogLevel;
    toLogLevel = fromLogLevel;
    fromLogLevel = tmp;
  }

  for (int i = fromLogLevel; i <= toLogLevel; i++) {
    m_logLevels[i] = true;
  }
}

void Logger::disable(LoggerLevel logLevel) {
  Guard g(m_log_mutex);
  if (logLevel == LL_ALL) {
    for (unsigned i = 0; i < MAX_LOG_LEVELS; i++) {
      m_logLevels[i] = false;
    }
  } else {
    m_logLevels[logLevel] = false;
  }
}

void Logger::alert(const char *pMsg, ...) const {
  va_list ap;
  va_start(ap, pMsg);
  log(LL_ALERT, pMsg, ap);
  va_end(ap);
}

void Logger::critical(const char *pMsg, ...) const {
  va_list ap;
  va_start(ap, pMsg);
  log(LL_CRITICAL, pMsg, ap);
  va_end(ap);
}
void Logger::error(const char *pMsg, ...) const {
  va_list ap;
  va_start(ap, pMsg);
  log(LL_ERROR, pMsg, ap);
  va_end(ap);
}
void Logger::warning(const char *pMsg, ...) const {
  va_list ap;
  va_start(ap, pMsg);
  log(LL_WARNING, pMsg, ap);
  va_end(ap);
}

void Logger::info(const char *pMsg, ...) const {
  va_list ap;
  va_start(ap, pMsg);
  log(LL_INFO, pMsg, ap);
  va_end(ap);
}

void Logger::debug(const char *pMsg, ...) const {
  va_list ap;
  va_start(ap, pMsg);
  log(LL_DEBUG, pMsg, ap);
  va_end(ap);
}

void Logger::log(LoggerLevel logLevel, const char *pMsg, va_list ap) const {
  Guard g(m_log_mutex);
  if (m_logLevels[LL_ON] && m_logLevels[logLevel]) {
    char buf[MAX_LOG_MESSAGE_SIZE];
    BaseString::vsnprintf(buf, sizeof(buf), pMsg, ap);
    time_t now = ::time((time_t *)nullptr);
    m_logHandler->append(m_pCategory, logLevel, buf, now);
  }
}

void Logger::setRepeatFrequency(unsigned val) {
  /* Set repeat frequency on the list of handlers */
  m_internalLogListHandler->setRepeatFrequency(val);
}

Logger::Timestamp::Timestamp() {
  Logger::format_timestamp(time(nullptr), buff, TsLen);
}

void Logger::format_timestamp(const time_t epoch, char *str, size_t len) {
  assert(len > 0);  // Assume buffer has size

  // convert to local timezone
  tm tm_buf;
  if (ndb_localtime_r(&epoch, &tm_buf) == nullptr) {
    // Failed to convert to local timezone.
    // Fill with bogus time stamp value in order
    // to ensure buffer can be safely printed
    strncpy(str, "2001-01-01 00:00:00", len);
    str[len - 1] = 0;
    return;
  }

  // Print the broken down time in timestamp format
  // to the string buffer
  BaseString::snprintf(
      str, len, "%d-%.2d-%.2d %.2d:%.2d:%.2d", tm_buf.tm_year + 1900,
      tm_buf.tm_mon + 1,  // month is [0,11]. +1 -> [1,12]
      tm_buf.tm_mday, tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);
  str[len - 1] = 0;
  return;
}

#ifdef TEST_LOGGER

#include <NdbSleep.h>
#include <EventLogger.hpp>
#include <File.hpp>
#include <NdbApi.hpp>
#include <NdbTap.hpp>
#include <fstream>
#include <ndb_cluster_connection.hpp>
#include <sstream>

class Ndb_log_consumer;
class NdbApiLogConsumer;

class Ndb_log_consumer : public NdbApiLogConsumer {
  const char *filename;

 public:
  explicit Ndb_log_consumer(const char *out_filename) {
    filename = out_filename;
  }
  ~Ndb_log_consumer() override = default;

  void log(LogLevel level, const char *category, const char *message) override {
    char timestamp[64];
    std::ofstream ofs(filename);
    Logger::format_timestamp(time(nullptr), timestamp, sizeof(timestamp));
    ofs << timestamp << " [" << category << "]"
        << " [" << level << "] " << message << std::endl;
    ofs.close();
  }
};

TAPTEST(logger) {
  ndb_init();
  {
    auto clearFile = [&](const std::string &fileName) {
      std::ofstream ofs;
      ofs.open(fileName, std::ofstream::out | std::ofstream::trunc);
      ofs.close();
    };

    auto getLastLogMsg = [&](const std::string &fileName, bool clear = true) {
      std::ifstream ifs(fileName);
      std::stringstream ss;
      // Remove date + Category
      ifs.seekg(34);
      ss << ifs.rdbuf();
      ifs.close();
      if (clear) {
        clearFile(fileName);
      }
      std::string res = ss.str();
      // Remove final '\n'
      if (!res.empty()) {
        res.pop_back();
      }
      return res;
    };

    static const std::string file = "file.log";
#ifdef _WIN32
    static constexpr int async_wait_time_ms = 150;
#else
    static constexpr int async_wait_time_ms = 50;
#endif
    clearFile(file);

    /**
     * Check that messages fed into eventLogger are correctly written to
     * the log.
     *
     * Two handlers in use, ConsoleHandler only for visualization propose
     * during the test and FileHandler to check that log messages are logged
     * as expected.
     */
    g_eventLogger->createConsoleHandler();
    g_eventLogger->createFileHandler(const_cast<char *>(file.c_str()));

    // No Async
    for (int i = 0; i < 100; ++i) {
      g_eventLogger->info("[SYNC] message #%d", i);
      const std::string expected_msg =
          "INFO     -- [SYNC] message #" + std::to_string(i);
      OK(getLastLogMsg(file) == expected_msg);
    }

    // Async
    g_eventLogger->startAsync();
    for (int i = 0; i < 100; ++i) {
      g_eventLogger->info("[ASYNC] message #%d", i);
      const std::string expected_msg =
          "INFO     -- [ASYNC] message #" + std::to_string(i);
      // give time to BufferedLogHandler log thread to dump logs
      NdbSleep_MilliSleep(async_wait_time_ms);
      OK(getLastLogMsg(file) == expected_msg);
    }

    // Mix Sync and Async
    bool async = false;
    g_eventLogger->stopAsync();
    std::string expected_msg;
    for (int i = 0; i < 100; ++i) {
      if (async) {
        g_eventLogger->info("[ASYNC] message #%d", i);
        expected_msg = "INFO     -- [ASYNC] message #" + std::to_string(i);
        // give time to BufferedLogHandler log thread to dump logs
        NdbSleep_MilliSleep(async_wait_time_ms);
      } else {
        g_eventLogger->info("[SYNC] message #%d", i);
        expected_msg = "INFO     -- [SYNC] message #" + std::to_string(i);
      }
      OK(getLastLogMsg(file) == expected_msg);
      if (i % 5 == 0) {
        if (async) {
          g_eventLogger->stopAsync();
        } else {
          g_eventLogger->startAsync();
        }
        async = !async;
      }
    }

    // Stress test on BufferedLogHandler
    {
      g_eventLogger->startAsync();
      int i;
      // Adds large amount of messages to buffer, no overflow expected.
      for (i = 0; i < 1000; ++i) {
        g_eventLogger->info("[ASYNC] message #%d", i);
      }

      /**
       * Some messages may be lost.
       * Give time to BufferedLogHandler log thread to consume messages from
       * buffer and get rid of warnings.
       */
      for (int j = 0; j < 20; j++) {
        g_eventLogger->info("[ASYNC] dump warnings");
        const std::string expected_msg = "INFO     -- [ASYNC] dump warnings";
        NdbSleep_MilliSleep(1000);
        if (getLastLogMsg(file) == expected_msg) break;
      }
      NdbSleep_MilliSleep(1000);
      g_eventLogger->info("[ASYNC] message #%d", i);
      expected_msg = "INFO     -- [ASYNC] message #" + std::to_string(i);
      // give time to BufferedLogHandler log thread to dump last log message
      NdbSleep_MilliSleep(async_wait_time_ms);
      OK(getLastLogMsg(file) == expected_msg);

      g_eventLogger->stopAsync();
    }

    /**
     * Check the use of the user-supplied logging function.
     * When user-supplied logging functions is not set test uses a new created
     * FileLogHandler as 'Default Handler' instead of the original
     * ConsoleHandler. Class LoggerTest is used to set/unset the
     * 'Default Handler'.
     *
     * For visualization purpose a ConsoleHandler is also used.
     *
     * Default Handler writes logs to fileA.log, g_ndb_log_consumer1 writes
     * logs to fileB.log and g_ndb_log_consumer2 writes logs to fileC.log
     *
     */
    const std::string fileA = "fileA.log";
    const std::string fileB = "fileB.log";
    const std::string fileC = "fileC.log";

    Ndb_log_consumer *log_consumer1 = new Ndb_log_consumer(fileB.c_str());
    Ndb_log_consumer *log_consumer2 = new Ndb_log_consumer(fileC.c_str());

    g_eventLogger->close();
    clearFile(fileA);
    clearFile(fileB);
    clearFile(fileC);
    g_eventLogger->createConsoleHandler();
    std::unique_ptr<LogHandler> lh =
        std::make_unique<FileLogHandler>(fileA.c_str());
    lh->open();

    // g_ndb_log_consumer1 in use, logs expected to be written to fileB
    Ndb_cluster_connection::set_log_consumer(log_consumer1);
    for (int i = 0; i < 10; ++i) {
      g_eventLogger->info("[CONSUMER 1] message #%d", i);
      const std::string expected_msg =
          "[2] [CONSUMER 1] message #" + std::to_string(i);
      OK(getLastLogMsg(fileA) != expected_msg);
      OK(getLastLogMsg(fileB) == expected_msg);
      OK(getLastLogMsg(fileC) != expected_msg);
    }

    // Default Handler re-activated, logs expected to be written to fileA
    Ndb_cluster_connection::set_log_consumer(nullptr);
    Logger::LoggerTest::setHandler(*g_eventLogger, lh.get());

    for (int i = 0; i < 10; ++i) {
      g_eventLogger->info("[DEFAULT] message #%d", i);
      const std::string expected_msg =
          "INFO     -- [DEFAULT] message #" + std::to_string(i);
      NdbSleep_MilliSleep(async_wait_time_ms);

      OK(getLastLogMsg(fileA) == expected_msg);
      OK(getLastLogMsg(fileB) != expected_msg);
      OK(getLastLogMsg(fileC) != expected_msg);
    }

    // g_ndb_log_consumer2 in use, logs expected to be written to fileC
    Ndb_cluster_connection::set_log_consumer(log_consumer2);
    for (int i = 0; i < 10; ++i) {
      g_eventLogger->info("[CONSUMER 2] message #%d", i);
      const std::string expected_msg =
          "[2] [CONSUMER 2] message #" + std::to_string(i);
      OK(getLastLogMsg(fileA) != expected_msg);
      OK(getLastLogMsg(fileB) != expected_msg);
      OK(getLastLogMsg(fileC) == expected_msg);
    }

    // Default Handler re-activated and set to original ConsoleHandler
    Ndb_cluster_connection::set_log_consumer(nullptr);
    Logger::LoggerTest::unset(*g_eventLogger);
    // Message expected to be writen to console (2x).
    g_eventLogger->info("DONE");

    delete log_consumer1;
    delete log_consumer2;

    File_class::remove(file.c_str());
    File_class::remove(fileA.c_str());
    File_class::remove(fileB.c_str());
    File_class::remove(fileC.c_str());
  }

  ndb_end(0);
  return 1;
}

#endif
