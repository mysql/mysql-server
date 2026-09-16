/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */
#include "sql/binlog/binlog_ofile.h"
#include "include/mysql/psi/mysql_file.h"
#include "my_dbug.h"
#include "sql/binlog_ostream.h"
#include "sql/mysqld.h"
#include "sql/rpl_log_encryption.h"

MYSQL_BIN_LOG::Binlog_ofile::~Binlog_ofile() {
  DBUG_TRACE;
  close();
  return;
}

bool MYSQL_BIN_LOG::Binlog_ofile::open(
#ifdef HAVE_PSI_INTERFACE
    PSI_file_key log_file_key,
#endif
    const char *binlog_name, myf flags, bool existing) {
  DBUG_TRACE;
  assert(m_pipeline_head == nullptr);

#ifndef NDEBUG
  {
#ifndef HAVE_PSI_INTERFACE
    PSI_file_key log_file_key = PSI_NOT_INSTRUMENTED;
#endif
    MY_STAT info;
    if (!mysql_file_stat(log_file_key, binlog_name, &info, MYF(0))) {
      assert(existing == !(my_errno() == ENOENT));
      set_my_errno(0);
    }
  }
#endif

  std::unique_ptr<IO_CACHE_ostream> file_ostream(new IO_CACHE_ostream);
  if (file_ostream->open(log_file_key, binlog_name, flags)) return true;

  m_pipeline_head = std::move(file_ostream);

  /* Setup encryption for new files if needed */
  if (!existing && rpl_encryption.is_enabled()) {
    std::unique_ptr<Binlog_encryption_ostream> encrypted_ostream(
        new Binlog_encryption_ostream());
    if (encrypted_ostream->open(std::move(m_pipeline_head))) return true;
    m_encrypted_header_size = encrypted_ostream->get_header_size();
    m_pipeline_head = std::move(encrypted_ostream);
  }

  return false;
}

std::unique_ptr<MYSQL_BIN_LOG::Binlog_ofile>
MYSQL_BIN_LOG::Binlog_ofile::open_existing(
#ifdef HAVE_PSI_INTERFACE
    PSI_file_key log_file_key,
#endif
    const char *binlog_name, myf flags) {
  DBUG_TRACE;
  std::unique_ptr<Rpl_encryption_header> header;
  unsigned char magic[BINLOG_MAGIC_SIZE];

  /* Open a simple istream to read the magic from the file */
  IO_CACHE_istream istream;
  if (istream.open(key_file_binlog, key_file_binlog_cache, binlog_name,
                   MYF(MY_WME | MY_DONT_CHECK_FILESIZE), rpl_read_size))
    return nullptr;
  if (istream.read(magic, BINLOG_MAGIC_SIZE) != BINLOG_MAGIC_SIZE)
    return nullptr;

  assert(Rpl_encryption_header::ENCRYPTION_MAGIC_SIZE == BINLOG_MAGIC_SIZE);
  /* Identify the file type by the magic to get the encryption header */
  if (memcmp(magic, Rpl_encryption_header::ENCRYPTION_MAGIC,
             BINLOG_MAGIC_SIZE) == 0) {
    header = Rpl_encryption_header::get_header(&istream);
    if (header == nullptr) return nullptr;
  } else if (memcmp(magic, BINLOG_MAGIC, BINLOG_MAGIC_SIZE) != 0) {
    return nullptr;
  }

  /* Open the binlog_ofile */
  std::unique_ptr<Binlog_ofile> ret_ofile(new Binlog_ofile);
  if (ret_ofile->open(
#ifdef HAVE_PSI_INTERFACE
          log_file_key,
#endif
          binlog_name, flags, true)) {
    return nullptr;
  }

  if (header != nullptr) {
    /* Add the encryption stream on top of IO_CACHE */
    std::unique_ptr<Binlog_encryption_ostream> encrypted_ostream(
        new Binlog_encryption_ostream);
    ret_ofile->m_encrypted_header_size = header->get_header_size();
    encrypted_ostream->open(std::move(ret_ofile->m_pipeline_head),
                            std::move(header));
    ret_ofile->m_pipeline_head = std::move(encrypted_ostream);
    ret_ofile->set_encrypted();
  }
  return ret_ofile;
}

void MYSQL_BIN_LOG::Binlog_ofile::close() {
  m_pipeline_head.reset(nullptr);
  m_position = 0;
  m_encrypted_header_size = 0;
}

bool MYSQL_BIN_LOG::Binlog_ofile::write(const unsigned char *buffer,
                                        my_off_t length) {
  assert(m_pipeline_head != nullptr);

  if (m_pipeline_head->write(buffer, length)) return true;

  m_position += length;
  return false;
}

bool MYSQL_BIN_LOG::Binlog_ofile::update(const unsigned char *buffer,
                                         my_off_t length, my_off_t offset) {
  assert(m_pipeline_head != nullptr);
  return m_pipeline_head->seek(offset) ||
         m_pipeline_head->write(buffer, length);
}

bool MYSQL_BIN_LOG::Binlog_ofile::truncate(my_off_t offset) {
  assert(m_pipeline_head != nullptr);

  if (m_pipeline_head->truncate(offset)) return true;
  m_position = offset;
  return false;
}

bool MYSQL_BIN_LOG::Binlog_ofile::flush() { return m_pipeline_head->flush(); }
bool MYSQL_BIN_LOG::Binlog_ofile::sync() { return m_pipeline_head->sync(); }
bool MYSQL_BIN_LOG::Binlog_ofile::flush_and_sync() { return flush() || sync(); }
my_off_t MYSQL_BIN_LOG::Binlog_ofile::position() { return m_position; }
bool MYSQL_BIN_LOG::Binlog_ofile::is_empty() { return position() == 0; }
bool MYSQL_BIN_LOG::Binlog_ofile::is_open() {
  return m_pipeline_head != nullptr;
}

int MYSQL_BIN_LOG::Binlog_ofile::get_encrypted_header_size() {
  return m_encrypted_header_size;
}

my_off_t MYSQL_BIN_LOG::Binlog_ofile::get_real_file_size() {
  return m_position + m_encrypted_header_size;
}

std::unique_ptr<Truncatable_ostream>
MYSQL_BIN_LOG::Binlog_ofile::get_pipeline_head() {
  return std::move(m_pipeline_head);
}

bool MYSQL_BIN_LOG::Binlog_ofile::is_encrypted() { return m_encrypted; }

void MYSQL_BIN_LOG::Binlog_ofile::set_encrypted() { m_encrypted = true; }
