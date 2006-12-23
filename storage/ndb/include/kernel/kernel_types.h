/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NDB_KERNEL_TYPES_H
#define NDB_KERNEL_TYPES_H

#include <my_config.h>
#include <ndb_types.h>
#include "ndb_limits.h"

typedef Uint16 NodeId; 
typedef Uint16 BlockNumber;
typedef Uint32 BlockReference;
typedef Uint16 GlobalSignalNumber;

enum Operation_t {
  ZREAD    = 0
  ,ZUPDATE  = 1
  ,ZINSERT  = 2
  ,ZDELETE  = 3
  ,ZWRITE   = 4
  ,ZREAD_EX = 5
#if 0
  ,ZREAD_CONSISTENT = 6
#endif
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

  Uint32 ref() const { return (m_page_no << MAX_TUPLES_BITS) | m_page_idx ;}
  
  Local_key& assref (Uint32 ref) { 
    m_page_no =ref >> MAX_TUPLES_BITS;
    m_page_idx = ref & MAX_TUPLES_PER_PAGE;
    return *this;
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




