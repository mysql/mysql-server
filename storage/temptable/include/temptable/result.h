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

/** @file storage/temptable/include/temptable/result.h
TempTable auxiliary Result enum. */

#ifndef TEMPTABLE_RESULT_H
#define TEMPTABLE_RESULT_H

#include "my_base.h" /* HA_ERR_* */

namespace temptable {

enum class Result {
  END_OF_FILE = HA_ERR_END_OF_FILE,
  FOUND_DUPP_KEY = HA_ERR_FOUND_DUPP_KEY,
  KEY_NOT_FOUND = HA_ERR_KEY_NOT_FOUND,
  NO_SUCH_TABLE = HA_ERR_NO_SUCH_TABLE,
  OK = 0,
  OUT_OF_MEM = HA_ERR_OUT_OF_MEM,
  RECORD_FILE_FULL = HA_ERR_RECORD_FILE_FULL,
  TABLE_CORRUPT = HA_ERR_TABLE_CORRUPT,
  TABLE_EXIST = HA_ERR_TABLE_EXIST,
  UNSUPPORTED = HA_ERR_UNSUPPORTED,
  WRONG_COMMAND = HA_ERR_WRONG_COMMAND,
  WRONG_INDEX = HA_ERR_WRONG_INDEX,
};

const char* result_to_string(Result r);

} /* namespace temptable */

#endif /* TEMPTABLE_RESULT_H */
