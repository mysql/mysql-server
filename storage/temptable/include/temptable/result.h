/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

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
  TOO_BIG_ROW = HA_ERR_TOO_BIG_ROW,
  UNSUPPORTED = HA_ERR_UNSUPPORTED,
  WRONG_COMMAND = HA_ERR_WRONG_COMMAND,
  WRONG_INDEX = HA_ERR_WRONG_INDEX,
};

const char *result_to_string(Result r);

} /* namespace temptable */

#endif /* TEMPTABLE_RESULT_H */
