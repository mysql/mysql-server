/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/** @file storage/temptable/src/result.cc
TempTable Result to string convert. */

#include "storage/temptable/include/temptable/result.h"

namespace temptable {

const char* result_to_string(Result r) {
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
