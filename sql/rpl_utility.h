/* Copyright 2006 MySQL AB. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef RPL_UTILITY_H
#define RPL_UTILITY_H

#ifndef __cplusplus
#error "Don't include this C++ header file from a non-C++ file!"
#endif

#include "mysql_priv.h"

uint32
field_length_from_packed(enum_field_types const field_type, 
                         byte const *const data);

/*
  A table definition from the master.

  RESPONSIBILITIES

  - Extract table definition data from the table map event
  - Check if table definition in table map is compatible with table
    definition on slave
 */

class table_def
{
public:
  typedef unsigned char field_type;

  table_def(field_type *t, my_size_t s)
    : m_type(t), m_size(s)
  {
  }

  my_size_t size() const { return m_size; }
  field_type type(my_ptrdiff_t i) const { return m_type[i]; }

  int compatible_with(RELAY_LOG_INFO *rli, TABLE *table) const;

private:
  my_size_t m_size;
  field_type *m_type;
};

#endif /* RPL_UTILITY_H */
