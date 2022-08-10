/* 
   Copyright (c) 2007, 2022, Oracle and/or its affiliates.

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
  typedef DLFifoList<Pool> LockQueueElement_fifo;
  typedef LocalDLFifoList<Pool> Local_LockQueueElement_fifo;
  
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
  LockQueueElement_fifo::Head m_queue;
};


#undef JAM_FILE_ID

#endif
