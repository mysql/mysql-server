/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved. */
#include "log_client.h"

Ldap_logger::Ldap_logger() {
  m_log_level = LDAP_LOG_LEVEL_NONE;
  m_log_writer = NULL;
  m_log_writer = new Ldap_log_writer_error();
  m_log_writer->open();
}

Ldap_logger::~Ldap_logger() {
  if (m_log_writer) {
    m_log_writer->close();
    delete m_log_writer;
  }
}
void Ldap_logger::set_log_level(ldap_log_level level) {
  m_log_level = level;
}

int Ldap_log_writer_error::open() {
  return 0;
}

int Ldap_log_writer_error::close() {
  return 0;
}

void Ldap_log_writer_error::write(std::string data) {
  std::cerr << data << "\n";
  std::cerr.flush();
}

Ldap_log_writer_error::Ldap_log_writer_error() {
}

Ldap_log_writer_error::~Ldap_log_writer_error() {
}
