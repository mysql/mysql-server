/* 
   Copyright (C) 2007, 2008 MySQL AB
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

#ifndef BLOCK_MUTEX_IMPL_HPP
#define BLOCK_MUTEX_IMPL_HPP

#include "ArrayPool.hpp"
#include "DLFifoList.hpp"
#include "KeyTable.hpp"
#include <signaldata/UtilLock.hpp>

class LockQueue
{
public:
  LockQueue() {}

  /**
   * A lock queue element
   */
  struct LockQueueElement 
  {
    LockQueueElement() {}

    UtilLockReq m_req;
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
  };

  typedef ArrayPool<LockQueueElement> Pool;
  
  Uint32 lock(Pool&, const UtilLockReq * req, const UtilLockReq** lockOwner= 0);
  Uint32 unlock(Pool&, const UtilUnlockReq* req, UtilLockReq* orig_req= 0);
  
  /**
   * After unlock
   */
  struct Iterator 
  {
    Pool * thePool;
    Ptr<LockQueueElement> m_prev;
    Ptr<LockQueueElement> m_curr;
  };
  
  bool first(Pool& pool, Iterator&);
  bool next(Iterator&);
  
  /**
   * 0 - done
   * 1 - already granted
   * 2 - needs conf
   */
  int checkLockGrant(Iterator &, UtilLockReq * req);

  /**
   * Clear lock queue
   */
  void clear (Pool&);

  /**
   * Dump
   */
  void dump_queue(Pool&, SimulatedBlock* block);
  
private:
  /**
   * The actual lock queue
   */
  DLFifoList<LockQueueElement>::Head m_queue;
};

#endif
