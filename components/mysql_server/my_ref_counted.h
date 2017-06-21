/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef MY_REF_COUNTED_H
#define MY_REF_COUNTED_H

#include <atomic>

#include "my_inttypes.h"

/**
  Helper class for reference counting.
*/
class my_ref_counted
{
public:
  my_ref_counted();
  my_ref_counted(my_ref_counted &other);
  virtual ~my_ref_counted();
  uint64 add_reference();
  bool release_reference(uint64* new_count= NULL);
  uint64 get_reference_count() const;

private:
  /** the reference count value */
  std::atomic<uint64> m_count;
};

#endif /* MY_REF_COUNTED_H */
