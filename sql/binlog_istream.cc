/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "sql/binlog_istream.h"
#include "sql/log_event.h"
#include "sql/mysqld.h"
#ifdef MYSQL_SERVER
#include "mysql/components/services/log_builtins.h"
#include "sql/binlog.h"
#include "sql/mysqld.h"
#endif

const char *Binlog_read_error::get_str() const {
  switch (m_type) {
    case READ_EOF:
      return "arrived the end of the file";
    case BOGUS:
      return "corrupted data in log event";
    case SYSTEM_IO:
      return "I/O error reading log event";
    case EVENT_TOO_LARGE:
      return "Event too big";
    case MEM_ALLOCATE:
      return "memory allocation failed reading log event";
    case TRUNC_EVENT:
      return "binlog truncated in the middle of event; consider out of disk "
             "space";
    case TRUNC_FD_EVENT:
      return "Found invalid Format description event in binary log";
    case CHECKSUM_FAILURE:
      return "Event crc check failed! Most likely there is event corruption.";
    case INVALID_EVENT:
      return "Found invalid event in binary log";
    case CANNOT_OPEN:
      return "Could not open log file";
    case HEADER_IO_FAILURE:
      return "I/O error reading the header from the binary log";
    case BAD_BINLOG_MAGIC:
      return "Binlog has bad magic number;  It's not a binary log file "
             "that can be used by this version of MySQL";
    default:
      /* There must be something wrong in the code if it reaches this branch. */
      DBUG_ASSERT(0);
      return nullptr;
  }
}

bool Basic_binlog_ifile::read_binlog_magic() {
  unsigned char magic[BINLOG_MAGIC_SIZE];

  if (m_istream->read(magic, BINLOG_MAGIC_SIZE) != BINLOG_MAGIC_SIZE) {
    return m_error->set_type(Binlog_read_error::BAD_BINLOG_MAGIC);
  }
  if (memcmp(magic, BINLOG_MAGIC, BINLOG_MAGIC_SIZE))
    return m_error->set_type(Binlog_read_error::BAD_BINLOG_MAGIC);
  m_position = BINLOG_MAGIC_SIZE;
  return m_error->set_type(Binlog_read_error::SUCCESS);
}

Basic_binlog_ifile::Basic_binlog_ifile(Binlog_read_error *binlog_read_error)
    : m_error(binlog_read_error) {}
Basic_binlog_ifile::~Basic_binlog_ifile() {}

bool Basic_binlog_ifile::open(const char *file_name) {
  m_istream = open_file(file_name);
  if (m_istream == nullptr)
    return m_error->set_type(Binlog_read_error::CANNOT_OPEN);
  return read_binlog_magic();
}

void Basic_binlog_ifile::close() {
  m_position = 0;
  m_istream = nullptr;
  close_file();
}

ssize_t Basic_binlog_ifile::read(unsigned char *buffer, size_t length) {
  longlong ret = m_istream->read(buffer, length);
  if (ret > 0) m_position += ret;
  return ret;
}

bool Basic_binlog_ifile::seek(my_off_t position) {
  if (m_istream->seek(position)) {
    m_error->set_type(Binlog_read_error::SYSTEM_IO);
    return true;
  }
  m_position = position;
  return false;
}

my_off_t Basic_binlog_ifile::length() { return m_istream->length(); }

#ifdef MYSQL_SERVER
Basic_seekable_istream *Binlog_ifile::open_file(const char *file_name) {
  if (m_ifile.open(key_file_binlog, key_file_binlog_cache, file_name,
                   MYF(MY_WME | MY_DONT_CHECK_FILESIZE), rpl_read_size))
    return nullptr;
  return &m_ifile;
}

void Binlog_ifile::close_file() { m_ifile.close(); }

Basic_seekable_istream *Relaylog_ifile::open_file(const char *file_name) {
  if (m_ifile.open(key_file_relaylog, key_file_relaylog_cache, file_name,
                   MYF(MY_WME | MY_DONT_CHECK_FILESIZE), rpl_read_size))
    return nullptr;
  return &m_ifile;
}

void Relaylog_ifile::close_file() { m_ifile.close(); }

#endif  // ifdef MYSQL_SERVER
