/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
/* Internals */

#ifndef AUTH_UTILITY_INCLUDED
#define AUTH_UTILITY_INCLUDED

#include "my_alloc.h"

/**
 Class to manage MEM_ROOT. It either accepts and initialized MEM_ROOT
 or initializes a new one and controls its lifespan.
*/
class Mem_root_base {
 public:
  explicit Mem_root_base(MEM_ROOT *mem_root);
  Mem_root_base(const Mem_root_base &) = delete;
  Mem_root_base &operator=(const Mem_root_base &) = delete;
  ~Mem_root_base();
  MEM_ROOT *get_mem_root() const;

 protected:
  MEM_ROOT *m_mem_root;
  MEM_ROOT m_internal_mem_root;

 private:
  Mem_root_base();
  bool m_inited;
};

/**
  Return MEM_ROOT handle.
*/
inline MEM_ROOT *Mem_root_base::get_mem_root() const { return m_mem_root; }

#endif /* AUTH_UTILITY_INCLUDED */
