/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved. */
#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <my_dbug.h>

struct log_type {
  typedef enum {
    LOG_DBG, LOG_INFO, LOG_WARNING, LOG_ERROR
  } type;
};

enum log_level {
  LOG_LEVEL_NONE = 1, LOG_LEVEL_ERROR, LOG_LEVEL_ERROR_WARNING, LOG_LEVEL_ERROR_WARNING_INFO, LOG_LEVEL_ALL
};

class Log_writer_error {
public:
  Log_writer_error();
  ~Log_writer_error();
  int open();
  int close();
  void write(std::string data);
};

class Logger {
public:
  Logger();
  ~Logger();
  template<log_type::type type>
  void log(std::string msg);
  void set_log_level(log_level level);
private:
  Log_writer_error *m_log_writer;
  log_level m_log_level;
};

template<log_type::type type>
void Logger::log(std::string msg) {
  std::stringstream header;
  switch (type) {
  case log_type::LOG_DBG:
    if (LOG_LEVEL_ALL > m_log_level) {
      goto WRITE_DBG;
    }
    header << "[DBG] ";
    break;
  case log_type::LOG_INFO:
    if (LOG_LEVEL_ERROR_WARNING_INFO > m_log_level) {
      goto WRITE_DBG;
    }
    header << "[Note] ";
    break;
  case log_type::LOG_WARNING:
    if (LOG_LEVEL_ERROR_WARNING > m_log_level) {
      goto WRITE_DBG;
    }
    header << "[Warning] ";
    break;
  case log_type::LOG_ERROR:
    if (LOG_LEVEL_NONE >= m_log_level) {
      goto WRITE_DBG;
    }
    header << "[Error] ";
    break;
  };

  /** We can write debug messages also in error log file if logging level is set to debug.
      For MySQL server this will be set using option.
      For MySQL client this will come from environment variable */
  if (m_log_writer){
    header << my_getsystime() << ": ";
    m_log_writer->write(header.str());
    m_log_writer->write(msg);
  }
WRITE_DBG:
  /** Log all the messages as debug messages as well. */
  DBUG_PRINT("ldap/sasl auth plugin: ", (": %s", msg.c_str()));
}

extern Logger *g_logger;

#define log_dbg g_logger->log< log_type::LOG_DBG >
#define log_info g_logger->log< log_type::LOG_INFO >
#define log_warning g_logger->log< log_type::LOG_WARNING >
#define log_error g_logger->log< log_type::LOG_ERROR >

#endif
