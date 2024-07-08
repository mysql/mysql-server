/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DYNAMIC_ARRAY_INCLUDED
#define DYNAMIC_ARRAY_INCLUDED

#include "my_inttypes.h"
#include "mysql/psi/psi_memory.h"

struct DYNAMIC_ARRAY {
  uchar *buffer{nullptr};
  uint elements{0}, max_element{0};
  uint alloc_increment{0};
  uint size_of_element{0};
  PSI_memory_key m_psi_key{PSI_NOT_INSTRUMENTED};
};

// Use Prealloced_array or std::vector or something similar in C++
extern bool my_init_dynamic_array(DYNAMIC_ARRAY *array, PSI_memory_key key,
                                  uint element_size, void *init_buffer,
                                  uint init_alloc, uint alloc_increment);
#define dynamic_element(array, array_index, type) \
  ((type)((array)->buffer) + (array_index))

extern bool insert_dynamic(DYNAMIC_ARRAY *array, const void *element);
extern void *alloc_dynamic(DYNAMIC_ARRAY *array);
extern void delete_dynamic(DYNAMIC_ARRAY *array);

#endif  // DYNAMIC_ARRAY_INCLUDED
