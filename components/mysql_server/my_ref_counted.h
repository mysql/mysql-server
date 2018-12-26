/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MY_REF_COUNTED_H
#define MY_REF_COUNTED_H

#include <atomic>

#include "my_inttypes.h"

/**
  Helper class for reference counting.
*/
class my_ref_counted {
 public:
  my_ref_counted();
  my_ref_counted(my_ref_counted &other);
  virtual ~my_ref_counted();
  uint64 add_reference();
  bool release_reference(uint64 *new_count = NULL);
  uint64 get_reference_count() const;

 private:
  /** the reference count value */
  std::atomic<uint64> m_count;
};

#endif /* MY_REF_COUNTED_H */
