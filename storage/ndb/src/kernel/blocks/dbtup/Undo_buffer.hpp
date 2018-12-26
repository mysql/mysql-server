/*
   Copyright (c) 2005, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef __UNDO_BUFFER_HPP
#define __UNDO_BUFFER_HPP

#include <ndb_global.h>
#include <kernel_types.h>

#define JAM_FILE_ID 404


struct Undo_buffer 
{
  Undo_buffer(class Ndbd_mem_manager*);
  
  /**
   * Alloc space for a copy tuple of size <em>words</em>
   *   store address to copy in dst
   *   supply pointer to original in curr
   *
   * @return 0 if unable to alloc space
   */
  Uint32 * alloc_copy_tuple(Local_key* dst, Uint32 words, bool locked = false);

  /**
   * Shrink size of copy tuple
   *   note: Only shrink latest allocated tuple
   */
  void shrink_copy_tuple(Local_key* dst, Uint32 words);
  
  /**
   * Free space for copy tuple at key
   */
  void free_copy_tuple(Local_key* key);
  
  /**
   * Get pointer to copy tuple
   */
  Uint32 * get_ptr(const Local_key* key);
  
private:
  class Ndbd_mem_manager* m_mm;
  Uint32 m_first_free;
};


#undef JAM_FILE_ID

#endif
