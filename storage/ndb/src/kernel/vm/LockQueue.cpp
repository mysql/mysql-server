/* 
   Copyright (c) 2007, 2016, Oracle and/or its affiliates. All rights reserved.

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


#include "LockQueue.hpp"
#include "SimulatedBlock.hpp"

#define JAM_FILE_ID 318


Uint32
LockQueue::lock(SimulatedBlock* block,
                Pool & thePool, 
                const UtilLockReq* req, const UtilLockReq** lockOwner)
{
  const bool exclusive = ! (req->requestInfo & UtilLockReq::SharedLock);
  const bool trylock = req->requestInfo & UtilLockReq::TryLock;
  const bool notify = req->requestInfo & UtilLockReq::Notify;
  
  Local_LockQueueElement_fifo queue(thePool, m_queue);
  
  bool grant = true;
  Ptr<LockQueueElement> lockEPtr;
  if (queue.last(lockEPtr))
  {
    jamBlock(block);
    if (! (lockEPtr.p->m_req.requestInfo & UtilLockReq::SharedLock))
    {
      jamBlock(block);
      grant = false;
    }
    else if (exclusive)
    {
      jamBlock(block);
      grant = false;
    }
    else if (lockEPtr.p->m_req.requestInfo & UtilLockReq::Granted)
    {
      jamBlock(block);
      grant = true;
    }
    else
    {
      jamBlock(block);
      grant = false;
    }
  }
  
  if(trylock && grant == false)
  {
    jamBlock(block);
    if (notify && lockOwner)
    {
      jamBlock(block);
      queue.first(lockEPtr);
      * lockOwner = &lockEPtr.p->m_req;
    }
    return UtilLockRef::LockAlreadyHeld;
  }
  
  if(!thePool.seize(lockEPtr))
  {
    jamBlock(block);
    return UtilLockRef::OutOfLockRecords;
  }
  
  lockEPtr.p->m_req = *req;
  queue.addLast(lockEPtr);
  
  if(grant)
  {
    jamBlock(block);
    lockEPtr.p->m_req.requestInfo |= UtilLockReq::Granted;
    return UtilLockRef::OK;
  }
  else
  {
    jamBlock(block);
    return UtilLockRef::InLockQueue;
  }
}

Uint32
LockQueue::unlock(SimulatedBlock* block,
                  Pool & thePool, 
                  const UtilUnlockReq* req,
                  UtilLockReq* orig_req)
{
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  
  Ptr<LockQueueElement> lockEPtr;
  Local_LockQueueElement_fifo queue(thePool, m_queue);
  
  for (queue.first(lockEPtr); !lockEPtr.isNull(); queue.next(lockEPtr))
  {
    jamBlock(block);
    if (lockEPtr.p->m_req.senderData == senderData &&
        lockEPtr.p->m_req.senderRef == senderRef)
    {
      jamBlock(block);
      
      Uint32 res;
      if (lockEPtr.p->m_req.requestInfo & UtilLockReq::Granted)
      {
        jamBlock(block);
        res = UtilUnlockRef::OK;
      }
      else
      {
        jamBlock(block);
        res = UtilUnlockRef::NotLockOwner;
      }
      
      /* Copy out orig request if ptr supplied */
      if (orig_req)
        *orig_req = lockEPtr.p->m_req;
      
      queue.release(lockEPtr);
      return res;
    }
  }
  
  return UtilUnlockRef::NotInLockQueue;
}

bool
LockQueue::first(SimulatedBlock* block,
                 Pool& thePool, Iterator & iter)
{
  Local_LockQueueElement_fifo queue(thePool, m_queue);
  if (queue.first(iter.m_curr))
  {
    iter.m_block = block;
    iter.m_prev.setNull();
    iter.thePool = &thePool;
    return true;
  }
  return false;
}

bool
LockQueue::next(Iterator& iter)
{
  iter.m_prev = iter.m_curr;
  Local_LockQueueElement_fifo queue(*iter.thePool, m_queue);
  return queue.next(iter.m_curr);
}

int
LockQueue::checkLockGrant(Iterator& iter, UtilLockReq* req)
{
  SimulatedBlock* block = iter.m_block;
  Local_LockQueueElement_fifo queue(*iter.thePool, m_queue);
  if (iter.m_prev.isNull())
  {
    if (iter.m_curr.p->m_req.requestInfo & UtilLockReq::Granted)
    {
      jamBlock(block);
      return 1;
    }
    else
    {
      jamBlock(block);
      * req = iter.m_curr.p->m_req;
      iter.m_curr.p->m_req.requestInfo |= UtilLockReq::Granted;
      return 2;
    }
  }
  else
  {
    jamBlock(block);
    /**
     * Prev is granted...
     */
    assert(iter.m_prev.p->m_req.requestInfo & UtilLockReq::Granted);
    if (iter.m_prev.p->m_req.requestInfo & UtilLockReq::SharedLock)
    {
      jamBlock(block);
      if (iter.m_curr.p->m_req.requestInfo & UtilLockReq::SharedLock)
      {
        jamBlock(block);
        if (iter.m_curr.p->m_req.requestInfo & UtilLockReq::Granted)
        {
          jamBlock(block);
          return 1;
        }
        else
        {
          jamBlock(block);
          * req = iter.m_curr.p->m_req;
          iter.m_curr.p->m_req.requestInfo |= UtilLockReq::Granted;
          return 2;
        }
      }
    }
    return 0;
  }
}

void
LockQueue::clear(Pool& thePool)
{
  Local_LockQueueElement_fifo queue(thePool, m_queue);
  while (queue.releaseFirst());
}

void
LockQueue::dump_queue(Pool& thePool, SimulatedBlock* block)
{
  Ptr<LockQueueElement> ptr;
  Local_LockQueueElement_fifo queue(thePool, m_queue);

  for (queue.first(ptr); !ptr.isNull(); queue.next(ptr))
  {
    jamBlock(block);
    block->infoEvent("- sender: 0x%x data: %u %s %s extra: %u",
                     ptr.p->m_req.senderRef,
                     ptr.p->m_req.senderData,
                     (ptr.p->m_req.requestInfo & UtilLockReq::SharedLock) ? 
                     "S":"X",
                     (ptr.p->m_req.requestInfo & UtilLockReq::Granted) ? 
                     "granted" : "",
                     ptr.p->m_req.extra);
  }
}

