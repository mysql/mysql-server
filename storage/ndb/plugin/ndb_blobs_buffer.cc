/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifdef TEST_NDB_BLOBS_BUFFER
#include "storage/ndb/plugin/ndb_blobs_buffer.h"
#include <NdbTap.hpp>

TAPTEST(NdbBlobsBuffer) {
  {
    Ndb_blobs_buffer buf0;

    // Zero size buffer
    OK(buf0.allocate(0));
    OK(buf0.size() == 0);
    OK(buf0.get_ptr(0) == nullptr);

    // Allocate buffer with one byte
    OK(buf0.allocate(1));
    OK(buf0.size() == 1);
    OK(buf0.get_ptr(0) != nullptr);
    OK(buf0.get_ptr(1) == nullptr);

    // Allocate buffer with four bytes
    OK(buf0.allocate(4));
    OK(buf0.size() == 4);
    OK(buf0.get_ptr(0) != nullptr);
    OK(buf0.get_ptr(3) != nullptr);
    OK(buf0.get_ptr(4) == nullptr);
  }

  {
    Ndb_blobs_buffer buf1;

    // Allocate and release
    constexpr size_t BUF1_SIZE = 37892;
    OK(buf1.allocate(BUF1_SIZE));
    OK(buf1.size() == BUF1_SIZE);
    buf1.release();
    OK(buf1.size() == 0);
  }

  constexpr bool manual_test = false;
  if (manual_test) {
    // Fail allocate
    Ndb_blobs_buffer buf2;
    OK(!buf2.allocate(~0));
    OK(buf2.size() == 0);
  }

  return 1;  // OK
}
#endif
