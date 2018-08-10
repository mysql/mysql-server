/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef CMD_EXEC_WINDOWS_H_
#define CMD_EXEC_WINDOWS_H_

#define _CRT_SECURE_NO_WARNINGS 1
#ifdef UNICODE
# #undef UNICODE
#endif
#include <stdint.h>
#include <windows.h>

#include <string>

// Launches a process as child of current process and exposes the stdin & stdout
// of the child process (implemented thru pipelines) so the client of this class
// can read from the child's stdout and write to the child's stdin.
class Process_launcher {
 public:
  /**
   * Creates a new process and launch it.
   * Argument 'args' must have a last entry that is NULL.
   * If redirect_stderr is true, the child's stderr is redirected to the same
   * stream than child's stdout.
   */
  Process_launcher(const char *cmd_line, bool redirect_stderr = true,
                   const std::string &env = "")
      : cmd_line_(cmd_line),
        env_(env),
        is_alive_(true),
        redirect_stderr_(redirect_stderr) {
    // launch the child process.
    start();
  }

  ~Process_launcher() {
    if (is_alive_) close();
  }

  /**
   * Reads a single byte (and returns it).
   * Throws an shcore::Exception in case of error when reading.
   */
  int read_one_char();  // read from stdout of child process

  /**
   * Read up to a 'count' bytes from the stdout of the child process.
   * This method blocks until the amount of bytes is read.
   * @param buf already allocated buffer where the read data will be stored.
   * @param count the maximum amount of bytes to read.
   * @return the real number of bytes read.
   * Returns an shcore::Exception in case of error when reading.
   */
  int read(char *buf, size_t count);

  /**
   * Write into stdin of child process.
   * Returns an shcore::Exception in case of error when writing.
   */
  int write_one_char(char c);

  /**
   * Writes several bytes into stdin of child process.
   * Returns an shcore::Exception in case of error when writing.
   */
  int write(const char *buf, size_t count);

  /**
   * Kills the child process.
   */
  void kill();

  /**
   * Returns the child process handle.
   * In Linux this needs to be cast to pid_t, in Windows to cast to HANDLE.
   */
  uint64_t get_pid();

  /**
   * Waits for the child process to exit and returns its exit code.
   * If the child process is already dead, wait() returns immediately.
   */
  int wait();

  /**
   * Returns the file descriptor write handle (to write child's stdin).
   * In Linux this needs to be cast to int, in Windows to cast to HANDLE.
   */
  uint64_t get_fd_write();

  /**
   * Returns the file descriptor read handle (to read child's stdout).
   * In Linux this needs to be cast to int, in Windows to cast to HANDLE.
   */
  uint64_t get_fd_read();

 private:
  /**
   * Throws an exception with the specified message, if msg == NULL, the
   * exception's message is specific of the platform error. (errno in Linux /
   * GetLastError in Windows).
   */
  void report_error(const char *msg);
  /** Launches the child process, and makes pipes available for read/write. */
  void start();
  /** Closes child process */
  void close();

  std::string cmd_line_;
  std::string env_;
  std::string args_;
  bool is_alive_;

  HANDLE child_in_rd;
  HANDLE child_in_wr;
  HANDLE child_out_rd;
  HANDLE child_out_wr;
  PROCESS_INFORMATION pi;
  STARTUPINFO si;
  bool redirect_stderr_;
};

#endif  // CMD_EXEC_WINDOWS_H_
