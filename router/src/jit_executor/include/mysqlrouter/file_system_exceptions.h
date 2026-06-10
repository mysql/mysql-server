/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ROUTER_SRC_JIT_EXECUTOR_INCLUDE_MYSQLROUTER_FILE_SYSTEM_EXCEPTIONS_H_
#define ROUTER_SRC_JIT_EXECUTOR_INCLUDE_MYSQLROUTER_FILE_SYSTEM_EXCEPTIONS_H_

#include <stdexcept>
#include <string>

#include "mysqlrouter/jit_executor_exceptions.h"

namespace shcore {
namespace polyglot {

class File_system_exception : public Jit_executor_exception {
 public:
  using Jit_executor_exception::Jit_executor_exception;
};

class Illegal_argument_exception : public File_system_exception {
 public:
  Illegal_argument_exception(const char *msg)
      : File_system_exception("IllegalArgumentException", msg) {}
};

class No_such_file_exception : public File_system_exception {
 public:
  No_such_file_exception(const char *msg)
      : File_system_exception("NoSuchFileException", msg) {}
};

class IO_exception : public File_system_exception {
 public:
  IO_exception(const char *msg) : File_system_exception("IOException", msg) {}
};

class Security_exception : public File_system_exception {
 public:
  Security_exception(const char *msg)
      : File_system_exception("SecurityException", msg) {}
};

class File_already_exists_exception : public File_system_exception {
 public:
  File_already_exists_exception(const char *msg)
      : File_system_exception("FileAlreadyExistsException", msg) {}
};

class Directory_not_empty_exception : public File_system_exception {
 public:
  Directory_not_empty_exception(const char *msg)
      : File_system_exception("DirectoryNotEmptyException", msg) {}
};

class Not_directory_exception : public File_system_exception {
 public:
  Not_directory_exception(const char *msg)
      : File_system_exception("NotDirectoryException", msg) {}
};

class Closed_channel_exception : public File_system_exception {
 public:
  Closed_channel_exception(const char *msg)
      : File_system_exception("ClosedChannelException", msg) {}
};

}  // namespace polyglot
}  // namespace shcore

#endif  // ROUTER_SRC_JIT_EXECUTOR_INCLUDE_MYSQLROUTER_FILE_SYSTEM_EXCEPTIONS_H_