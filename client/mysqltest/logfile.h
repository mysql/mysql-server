#ifndef LOGFILE_INCLUDED
#define LOGFILE_INCLUDED

// Copyright (c) 2018, 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

/// @file
///
/// This file declares the Log file class.

#include "my_io.h"
#include "my_sys.h"

class Logfile {
 public:
  Logfile();
  Logfile(const Logfile &) = default;

  virtual ~Logfile() { close(); }

  /// Return file name.
  ///
  /// @retval Name of the file.
  const char *file_name() const { return m_filename; }

  /// Return number bytes written into a log file.
  ///
  /// @retval Number bytes written into a log file.
  std::size_t bytes_written() const { return m_bytes_written; }

  /// Flush any unwritten content from the stream's buffer to the
  /// associated file.
  ///
  /// @retval False if successful, true otherwise.
  bool flush();

  /// Construct a file path using directory name, file name and extension
  /// arguments and open it in write/update mode.
  ///
  /// @param dirname  Directory name
  /// @param filename File name
  /// @param ext      Extension name
  ///
  /// @retval False if successful, true otherwise.
  bool open(const char *dirname, const char *filename, const char *ext);

  /// Write the contents in data array to a file.
  ///
  /// @param data   Pointer to the array to be written
  /// @param length Length of the buffer
  ///
  /// @retval False if successful, true otherwise.
  bool write(const char *data, std::size_t length);

  /// Print the last N number of lines from a file to stderr.
  ///
  /// @param lines Number of lines
  void show_tail(unsigned int lines);

 private:
  /// Close the given file stream.
  void close();

  char m_filename[FN_REFLEN];
  FILE *m_file;
  std::size_t m_bytes_written;
};

#endif  // LOGFILE_INCLUDED
