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
#ifndef BINLOG_OFILE_H_INCLUDED
#define BINLOG_OFILE_H_INCLUDED

#include "my_inttypes.h"
#include "mysql/components/services/bits/psi_file_bits.h"
#include "sql/basic_ostream.h"
#include "sql/binlog.h"

/**
   Logical binlog file which wraps and hides the detail of lower layer storage
   implementation. Binlog code just use this class to control real storage
 */
class MYSQL_BIN_LOG::Binlog_ofile : public Basic_ostream {
 public:
  ~Binlog_ofile() override;

  /**
     Opens the binlog file. It opens the lower layer storage.

     @param[in] log_file_key  The PSI_file_key for this stream
     @param[in] binlog_name  The file to be opened
     @param[in] flags  The flags used by IO_CACHE.
     @param[in] existing True if opening the file, false if creating a new one.

     @retval false  Success
     @retval true  Error
  */
  [[nodiscard]] bool open(
#ifdef HAVE_PSI_INTERFACE
      PSI_file_key log_file_key,
#endif
      const char *binlog_name, myf flags, bool existing = false);

  /**
    Opens an existing binlog file. It opens the lower layer storage reusing the
    existing file password if needed.

    @param[in] log_file_key The PSI_file_key for this stream
    @param[in] binlog_name The file to be opened
    @param[in] flags The flags used by IO_CACHE.

    @retval std::unique_ptr A Binlog_ofile object pointer.
    @retval nullptr Error.
  */
  [[nodiscard]] static std::unique_ptr<MYSQL_BIN_LOG::Binlog_ofile>
  open_existing(
#ifdef HAVE_PSI_INTERFACE
      PSI_file_key log_file_key,
#endif
      const char *binlog_name, myf flags);

  virtual void close();

  /**
     Writes data into storage and maintains binlog position.

     @param[in] buffer  the data will be written
     @param[in] length  the length of the data

     @retval false  Success
     @retval true  Error
  */
  [[nodiscard]] virtual bool write(const unsigned char *buffer,
                                   my_off_t length) override;

  /**
     Updates some bytes in the binlog file. If is only used for clearing
     LOG_EVENT_BINLOG_IN_USE_F.

     @param[in] buffer  the data will be written
     @param[in] length  the length of the data
     @param[in] offset  the offset of the bytes will be updated

     @retval false  Success
     @retval true  Error
  */
  [[nodiscard]] virtual bool update(const unsigned char *buffer,
                                    my_off_t length, my_off_t offset);

  /**
     Truncates some data at the end of the binlog file.

     @param[in] offset  where the binlog file will be truncated to.

     @retval false  Success
     @retval true  Error
  */
  [[nodiscard]] virtual bool truncate(my_off_t offset);

  [[nodiscard]] virtual bool flush();
  [[nodiscard]] virtual bool sync();
  [[nodiscard]] virtual bool flush_and_sync();
  [[nodiscard]] virtual my_off_t position();
  [[nodiscard]] virtual bool is_empty();
  [[nodiscard]] virtual bool is_open();

  /**
    Returns the encrypted header size of the binary log file.

    @retval 0 The file is not encrypted.
    @retval >0 The encryption header size.
  */
  [[nodiscard]] virtual int get_encrypted_header_size();

  /**
    Returns the real file size.

    While position() returns the "file size" from the plain binary log events
    stream point of view, this function considers the encryption header when it
    exists.

    @return The real file size considering the encryption header.
  */
  [[nodiscard]] virtual my_off_t get_real_file_size();

  /**
    Get the pipeline head.

    @retval  Returns the pipeline head or nullptr.
  */
  [[nodiscard]] virtual std::unique_ptr<Truncatable_ostream>
  get_pipeline_head();

  /**
    Check if the log file is encrypted.

    @retval  True if the log file is encrypted.
    @retval  False if the log file is not encrypted.
  */
  [[nodiscard]] virtual bool is_encrypted();

  /**
    Set that the log file is encrypted.
  */
  virtual void set_encrypted();

 private:
  my_off_t m_position = 0;
  int m_encrypted_header_size = 0;
  std::unique_ptr<Truncatable_ostream> m_pipeline_head;
  bool m_encrypted = false;
};

#endif /* BINLOG_OFILE_H_INCLUDED */
