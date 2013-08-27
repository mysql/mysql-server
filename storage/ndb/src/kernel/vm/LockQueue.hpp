/* 
   Copyright (c) 2007, 2013, Oracle and/or its affiliates. All rights reserved.

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
#include "IntrusiveList.hpp"
#include "KeyTable.hpp"
#include <signaldata/UtilLock.hpp>

#define JAM_FILE_ID 283


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
  
  Uint32 lock(SimulatedBlock*, 
              Pool&, const UtilLockReq*, const UtilLockReq** = 0);
  Uint32 unlock(SimulatedBlock*,
                Pool&, const UtilUnlockReq* req,
                UtilLockReq* orig_req= 0);
  
  /**
   * After unlock
   */
  struct Iterator 
  {
    SimulatedBlock* m_block;
    Pool * thePool;
    Ptr<LockQueueElement> m_prev;
    Ptr<LockQueueElement> m_curr;
  };
  
  bool first(SimulatedBlock*, Pool& pool, Iterator&);
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


#undef JAM_FILE_ID

#endif
