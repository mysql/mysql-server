/*
   Copyright (C) 2003, 2005, 2006, 2008 MySQL AB, 2010 Sun Microsystems, Inc.
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

#ifndef NDB_KERNEL_TYPES_H
#define NDB_KERNEL_TYPES_H

#include <my_global.h>
#include <ndb_types.h>
#include "ndb_limits.h"

typedef Uint16 NodeId; 
typedef Uint16 BlockNumber;
typedef Uint16 BlockInstance;
typedef Uint32 BlockReference;
typedef Uint16 GlobalSignalNumber;

enum Operation_t {
  ZREAD    = 0
  ,ZUPDATE  = 1
  ,ZINSERT  = 2
  ,ZDELETE  = 3
  ,ZWRITE   = 4
  ,ZREAD_EX = 5
  ,ZREFRESH = 6
  ,ZUNLOCK  = 7
};

/**
 * 32k page
 */
struct GlobalPage {
  union {
    Uint32 data[GLOBAL_PAGE_SIZE/sizeof(Uint32)];
    Uint32 nextPool;
  };
};

struct Local_key 
{
  Uint32 m_page_no;
  Uint16 m_page_idx;
  Uint16 m_file_no;     

  bool isNull() const { return m_page_no == RNIL; }
  void setNull() { m_page_no= RNIL; m_file_no= m_page_idx= ~0;}

  Uint32 ref() const { return ref(m_page_no,m_page_idx) ;}
  
  Local_key& assref (Uint32 ref) {
    m_page_no = ref2page_id(ref);
    m_page_idx = ref2page_idx(ref);
    return *this;
  }

  static Uint32 ref(Uint32 lk1, Uint32 lk2) {
    return (lk1 << MAX_TUPLES_BITS) | lk2;
  }

  static Uint32 ref2page_id(Uint32 ref) { return ref >> MAX_TUPLES_BITS; }
  static Uint32 ref2page_idx(Uint32 ref) { return ref & MAX_TUPLES_PER_PAGE; }

  static bool isInvalid(Uint32 lk1, Uint32 lk2) {
    return ref(lk1, lk2) == ~Uint32(0);
  }
};

class NdbOut&
operator<<(class NdbOut&, const struct Local_key&);

inline
Uint32 
table_version_major(Uint32 ver)
{
  return ver & 0x00FFFFFF;
}

#endif




