/*
   Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.

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
#include "ndb_global.h"
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

  STATIC_CONST( INVALID_PAGE_NO = 0xffffffff );
  STATIC_CONST( INVALID_PAGE_IDX = 0xffff );

  bool isNull() const { return m_page_no == RNIL; }
  void setNull() { m_page_no= RNIL; m_file_no= m_page_idx= ~0;}

  static bool isInvalid(Uint32 lk1, Uint32 lk2)
  {
    return lk1 == INVALID_PAGE_NO;
  }
  void setInvalid()
  {
    m_page_no = INVALID_PAGE_NO;
    m_page_idx = INVALID_PAGE_IDX;
  }
  bool isInvalid() const
  {
    return m_page_no == INVALID_PAGE_NO;
  }
  /**
   * Can the local key be saved in one Uint32
   */
  static bool isShort(Uint32 pageId) {
    return pageId < (1 << (32 - MAX_TUPLES_BITS));
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




