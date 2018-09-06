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

#ifndef BINLOG_ISTREAM_INCLUDED
#define BINLOG_ISTREAM_INCLUDED
#include "my_sys.h"
#include "sql/basic_istream.h"

/**
   It defines the error types which could happen when reading binlog files or
   deserializing binlog events. String error message of the error types are
   defined as well. It has a member variable to store an error type and
   provides a few functions to check the error type stored in the member
   variable.
 */
class Binlog_read_error {
 public:
  /**
     Possible errors which happens when reading an event.
  */
  enum Error_type {
    // No error happens
    SUCCESS = 0,
    /*
      Arrive at the end of the stream. Nothing was read. It is smaller than any
      other errors. Because READ_EOF is often not an error, and others are
      usually errors.
    */
    READ_EOF = 1,
    // malformed event
    BOGUS,
    // IO error while reading
    SYSTEM_IO,
    // Failed to allocate memory
    MEM_ALLOCATE,
    // Only a partial event could be read
    TRUNC_EVENT,
    // Only a partial Format_description_log_event could be read
    TRUNC_FD_EVENT,
    EVENT_TOO_LARGE,
    CHECKSUM_FAILURE,
    // Event's is_valid returned false
    INVALID_EVENT,
    // Cannot open the binlog file
    CANNOT_OPEN,
    // System IO error happened while reading the binlog magic
    HEADER_IO_FAILURE,
    // The binlog magic is incorrect
    BAD_BINLOG_MAGIC
  };

  Binlog_read_error() {}
  Binlog_read_error(Error_type type) : m_type(type) {}

  bool has_error() { return m_type != SUCCESS; }
  bool has_fatal_error() { return m_type > READ_EOF; }

  /**
     Return the error encounted when reading events.
   */
  Error_type get_type() const { return m_type; }

  /**
     Return error message of the error type.

     @return It will return nullptr if m_type is SUCCESS. In practice, it should
             never be called if m_type is SUCCESS. So an assertion is added in
             debug mode which predicts m_type is not SUCCESS.
  */
  const char *get_str() const;

  /**
     Set m_error to error.

     @param[in] type  The error type will be set
     @retval false If error is SUCCESS
     @retval true If error is not SUCCESS.
  */
  bool set_type(Error_type type) {
    m_type = type;
    return has_error();
  }

 private:
  Error_type m_type = SUCCESS;
};

/**
   Base class of binlog input files. It is a logical binlog file which wraps
   and hides the detail of lower layer storage implementation. Binlog reader and
   other binlog code just uses this class to control real storage.
*/
class Basic_binlog_ifile : public Basic_seekable_istream {
 public:
  /**
     @param[in] binlog_read_error Basic_binlog_ifile doesn't own an
                                  Binlog_read_error. So the caller should
                                  provide one to it. When error happens,
                                  the error type will be set into 'error'.
   */
  Basic_binlog_ifile(Binlog_read_error *binlog_read_error);
  Basic_binlog_ifile(const Basic_binlog_ifile &) = delete;
  Basic_binlog_ifile &operator=(const Basic_binlog_ifile &) = delete;
  ~Basic_binlog_ifile() override;
  /**
     Open a binlog file.

     @param[in] file_name  name of the binlog file which will be opened.
  */
  bool open(const char *file_name);
  /**
     Close the binlog file it is reading.
  */
  void close();

  ssize_t read(unsigned char *buffer, size_t length) override;
  bool seek(my_off_t position) override;

  my_off_t position() const { return m_position; }
  bool is_open() const { return m_istream != nullptr; }

  /**
     Get length of the binlog file. It is not os file length. The content maybe
     encrypted or compressed. It is the total length of BINLOG_MAGIC and all
     raw binlog events.
  */
  my_off_t length() override;

 protected:
  /**
     Open the system layer file. It is the entry of the stream pipeline.
     Implementation is delegated to sub-classes. Sub-classes opens system layer
     files in different way.

     @param[in] file_name  name of the binlog file which will be opened.
  */
  virtual Basic_seekable_istream *open_file(const char *file_name) = 0;
  /**
     close the system layer file.
  */
  virtual void close_file() = 0;

  /**
     It is convenient for caller to share a Binlog_read_error object between
     streams. So Binlog_read_error pointer is defined here. It should be
     initialized in constructor by caller.
  */
  Binlog_read_error *m_error;

 private:
  /**
     The binlog's position where it is reading. It is the position in logical
     binlog file, but not the position of system file.
  */
  my_off_t m_position = 0;
  /** It is the entry of the low level stream pipeline. */
  Basic_seekable_istream *m_istream = nullptr;

  /**
     Read binlog magic from binlog file and check if it is valid binlog magic.
     @retval false Its binlog magic is valid
     @retval true Its binlog magic is invalid
  */
  bool read_binlog_magic();
};

#ifdef MYSQL_SERVER
/**
   Binlog input file. It is responsible for opening binlog files generated by
   the server itself, but not relaylog files.
*/
class Binlog_ifile : public Basic_binlog_ifile {
 public:
  using Basic_binlog_ifile::Basic_binlog_ifile;

 protected:
  Basic_seekable_istream *open_file(const char *file_name) override;
  void close_file() override;

 private:
  IO_CACHE_istream m_ifile;
};

/**
   Relaylog input file. It is responsible for opening relay log files.
*/
class Relaylog_ifile : public Basic_binlog_ifile {
 public:
  using Basic_binlog_ifile::Basic_binlog_ifile;

 protected:
  Basic_seekable_istream *open_file(const char *file_name) override;
  void close_file() override;

 private:
  IO_CACHE_istream m_ifile;
};
#endif

#endif  // BINLOG_ISTREAM_INCLUDED
