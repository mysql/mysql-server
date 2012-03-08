/*
   Copyright (C) 2005-2008 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef __UNDO_BUFFER_HPP
#define __UNDO_BUFFER_HPP

#include <ndb_global.h>
#include <kernel_types.h>

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
  Uint32 * alloc_copy_tuple(Local_key* dst, Uint32 words);

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

#endif
