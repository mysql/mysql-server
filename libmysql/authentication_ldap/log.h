/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved. */
#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include "mysql/psi/psi_memory.h"
#include <mysql/plugin.h>
#include <mysql/service_my_plugin_log.h>
#include <my_dbug.h>

extern MYSQL_PLUGIN g_ldap_plugin_info;

struct log_type {
  typedef enum {
    LOG_INFO, LOG_WARNING, LOG_ERROR
  } type;
};

enum log_level {
  LOG_LEVEL_NONE = 1, LOG_LEVEL_ERROR_WARNING, LOG_LEVEL_ALL
};

class Log_writer {
public:
  virtual ~Log_writer() {};
  virtual int Open(std::string file_name) = 0;
  virtual int Close() = 0;
  virtual void Write(std::string data) = 0;
};

class Log_writer_error: Log_writer {
public:
  int Open(std::string file_name);
  int Close();
  void Write(std::string data);
};

class Log_writer_file: Log_writer {
public:
  Log_writer_file();
  ~Log_writer_file();
  int Open(std::string file_name);
  int Close();
  void Write(std::string data);

private:
  std::string m_file_name;
  std::ofstream *m_file_stream;
};

template<class LOGGER_TYPE>
class Logger {
public:
  Logger(std::string file_name);
  ~Logger();
  template<log_type::type type>
  void Log(std::string msg);
  void SetLogLevel(log_level level);
private:
  Log_writer *m_log_writer;
  log_level m_log_level;
  int m_logger_initilzed;
};

template<class LOGGER_TYPE>
Logger<LOGGER_TYPE>::Logger(std::string file_name) {
  m_logger_initilzed = -1;
  m_log_level = LOG_LEVEL_ALL;
  m_log_writer = NULL;
  m_log_writer = (Log_writer*) (new LOGGER_TYPE());
  m_logger_initilzed = m_log_writer->Open(file_name);
}

template<class LOGGER_TYPE>
Logger<LOGGER_TYPE>::~Logger() {
  m_log_writer->Close();
  delete (LOGGER_TYPE*) m_log_writer;
}

template<class LOGGER_TYPE>
template<log_type::type type>
void Logger<LOGGER_TYPE>::Log(std::string msg) {
  if (!m_log_writer)
    return;
  /** Log all the messages as debug messages as well. */
  DBUG_PRINT("ldap", (": %s", msg.c_str()));
  std::stringstream header;
  header << my_getsystime() << ": ";
  int plugin_error_level = MY_INFORMATION_LEVEL;
  switch (type) {
  case log_type::LOG_INFO:
      plugin_error_level = MY_INFORMATION_LEVEL;
      if (LOG_LEVEL_ALL > m_log_level)
        return;
      header << "<INFO> ";
      break;
  case log_type::LOG_WARNING:
      plugin_error_level = MY_WARNING_LEVEL;
      if (LOG_LEVEL_ERROR_WARNING > m_log_level)
        return;
      header << "<WARNING> ";
      break;
  case log_type::LOG_ERROR:
      plugin_error_level = MY_ERROR_LEVEL;
      if (LOG_LEVEL_NONE > m_log_level)
          return;
      header << "<ERROR> ";
      break;
    }

  header << my_getsystime() << ": ";
  m_log_writer->Write(header.str());
  m_log_writer->Write(msg);
  my_plugin_log_message(&g_ldap_plugin_info,
                        (plugin_log_level) plugin_error_level, msg.c_str());
}

template<class LOGGER_TYPE>
void Logger<LOGGER_TYPE>::SetLogLevel(log_level level) {
  m_log_level = level;
}

extern Logger<Log_writer_file> g_logger;

//#define LDAP_PLUGIN_AUTH_LOG_NONE
#ifndef LDAP_PLUGIN_AUTH_LOG_NONE
#define log_info g_logger.Log< log_type::LOG_INFO >
#define log_warning g_logger.Log< log_type::LOG_WARNING >
#define log_error g_logger.Log< log_type::LOG_ERROR >
#else
#define log_info(...)
#define log_warning(...)
#define log_error(...)
#endif

#endif
