/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved. */

#include <my_global.h>
#include "log.h"
#include "mysql/psi/psi_memory.h"

bool external_logging = false;

Logger::Logger() {
  m_logger_initilzed = -1;
  m_log_level = LOG_LEVEL_NONE;
  m_log_writer = NULL;
  m_log_writer = new Log_writer_error();
  m_logger_initilzed = m_log_writer->open();
}

Logger::~Logger() {
  if (m_log_writer) {
    m_log_writer->close();
    delete m_log_writer;
  }
}
void Logger::set_log_level(log_level level) {
  m_log_level = level;
}

int Log_writer_error::open() {
  return 0;
}

int Log_writer_error::close() {
  return 0;
}

void Log_writer_error::write(std::string data) {
  std::cerr << data << "\n";
  std::cerr.flush();
}

Log_writer_error::Log_writer_error() {
}

Log_writer_error::~Log_writer_error() {
}
