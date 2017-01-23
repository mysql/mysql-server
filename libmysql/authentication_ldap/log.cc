/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved. */

#include <my_global.h>
#include "log.h"
#include "mysql/psi/psi_memory.h"

bool external_logging = false;

int Log_writer_error::Open(std::string file_name) {
  return 0;
}

int Log_writer_error::Close() {
  return 0;
}

void Log_writer_error::Write(std::string data) {
  std::cerr << data << "\n";
  std::cerr.flush();
}

Log_writer_file::Log_writer_file() {
  m_file_stream =  NULL;
}

Log_writer_file::~Log_writer_file() {
  if(m_file_stream) {
    m_file_stream->close();
  }
}

int Log_writer_file::Open(std::string file_name) {
  int ret_val = -1;
  m_file_name = file_name;
  if (m_file_name == "") {
    goto EXIT;
  }
  else {
   m_file_stream =  new std::ofstream();
   m_file_stream->open(m_file_name.c_str(), std::ios_base::binary|std::ios_base::out );
   if( m_file_stream->is_open() ) {
     ret_val = 0;
   }
  }
EXIT:
  return ret_val;
}

void Log_writer_file::Write(std::string data) {
  *m_file_stream << data;
}

int Log_writer_file::Close() {
  if(m_file_stream) {
    m_file_stream->close();
  }
  return 0;
}
