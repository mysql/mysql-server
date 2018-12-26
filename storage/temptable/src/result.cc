/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/src/result.cc
TempTable Result to string convert. */

#include "storage/temptable/include/temptable/result.h"

namespace temptable {

const char *result_to_string(Result r) {
  switch (r) {
    case Result::END_OF_FILE:
      return "END_OF_FILE";
    case Result::FOUND_DUPP_KEY:
      return "FOUND_DUPP_KEY";
    case Result::KEY_NOT_FOUND:
      return "KEY_NOT_FOUND";
    case Result::NO_SUCH_TABLE:
      return "NO_SUCH_TABLE";
    case Result::OK:
      return "OK";
    case Result::OUT_OF_MEM:
      return "OUT_OF_MEM";
    case Result::RECORD_FILE_FULL:
      return "RECORD_FILE_FULL";
    case Result::TABLE_CORRUPT:
      return "TABLE_CORRUPT";
    case Result::TABLE_EXIST:
      return "TABLE_EXIST";
    case Result::UNSUPPORTED:
      return "UNSUPPORTED";
    case Result::WRONG_COMMAND:
      return "WRONG_COMMAND";
    case Result::WRONG_INDEX:
      return "WRONG_INDEX";
  }
  return "UNKNOWN";
}

} /* namespace temptable */
