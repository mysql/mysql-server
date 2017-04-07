/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved. */
#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>
#include <iostream>
#include <sstream>
#include <my_dbug.h>
#include "my_systime.h"

struct ldap_log_type {
  typedef enum {
    LDAP_LOG_DBG, LDAP_LOG_INFO, LDAP_LOG_WARNING, LDAP_LOG_ERROR
  } ldap_type;
};

enum ldap_log_level {
  LDAP_LOG_LEVEL_NONE = 1, LDAP_LOG_LEVEL_ERROR, LDAP_LOG_LEVEL_ERROR_WARNING, LDAP_LOG_LEVEL_ERROR_WARNING_INFO, LDAP_LOG_LEVEL_ALL
};

class Ldap_log_writer_error {
public:
  Ldap_log_writer_error();
  ~Ldap_log_writer_error();
  int open();
  int close();
  void write(std::string data);
};

class Ldap_logger {
public:
  Ldap_logger();
  ~Ldap_logger();
  template<ldap_log_type::ldap_type type>
  void log(std::string msg);
  void set_log_level(ldap_log_level level);
private:
  Ldap_log_writer_error *m_log_writer;
  ldap_log_level m_log_level;
};

template<ldap_log_type::ldap_type type>
void Ldap_logger::log(std::string msg) {
  std::stringstream header;
  switch (type) {
  case ldap_log_type::LDAP_LOG_DBG:
    if (LDAP_LOG_LEVEL_ALL > m_log_level) {
      goto WRITE_DBG;
    }
    header << "[DBG] ";
    break;
  case ldap_log_type::LDAP_LOG_INFO:
    if (LDAP_LOG_LEVEL_ERROR_WARNING_INFO > m_log_level) {
      goto WRITE_DBG;
    }
    header << "[Note] ";
    break;
  case ldap_log_type::LDAP_LOG_WARNING:
    if (LDAP_LOG_LEVEL_ERROR_WARNING > m_log_level) {
      goto WRITE_DBG;
    }
    header << "[Warning] ";
    break;
  case ldap_log_type::LDAP_LOG_ERROR:
    if (LDAP_LOG_LEVEL_NONE >= m_log_level) {
      goto WRITE_DBG;
    }
    header << "[Error] ";
    break;
  };

  /** We can write debug messages also in error log file if logging level is set to debug.
      For MySQL client this will come from environment variable */
  if (m_log_writer){
    header << my_getsystime() << ": ";
    m_log_writer->write(header.str());
    m_log_writer->write(msg);
  }
WRITE_DBG:
  /** Log all the messages as debug messages as well. */
  DBUG_PRINT("ldap/sasl auth client plug-in: ", (": %s", msg.c_str()));
}

extern Ldap_logger *g_logger_client;

#define log_dbg g_logger_client->log< ldap_log_type::LDAP_LOG_DBG >
#define log_info g_logger_client->log< ldap_log_type::LDAP_LOG_INFO >
#define log_warning g_logger->log< ldap_log_type::LDAP_LOG_WARNING >
#define log_error g_logger_client->log< ldap_log_type::LDAP_LOG_ERROR >

#endif
