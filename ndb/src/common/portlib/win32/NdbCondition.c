/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <assert.h>
#include <sys/types.h>

#include "NdbCondition.h"
#include <NdbMutex.h>


struct NdbCondition
{
    long nWaiters;
    NdbMutex* pNdbMutexWaitersLock;
    HANDLE hSemaphore;
    HANDLE hEventWaitersDone;
    int bWasBroadcast;
};


struct NdbCondition* 
NdbCondition_Create(void)
{
    int result = 0;
    struct NdbCondition* pNdbCondition = (struct NdbCondition*)malloc(sizeof(struct NdbCondition));
    if(!pNdbCondition)
        return 0;
    
    pNdbCondition->nWaiters = 0;
    pNdbCondition->bWasBroadcast = 0;
    if(!(pNdbCondition->hSemaphore = CreateSemaphore(0, 0, MAXLONG, 0)))
        result = -1;
    else if(!(pNdbCondition->pNdbMutexWaitersLock = NdbMutex_Create()))
        result = -1;
    else if(!(pNdbCondition->hEventWaitersDone = CreateEvent(0, 0, 0, 0)))
        result = -1;

    assert(!result);
    return pNdbCondition;
}


int 
NdbCondition_Wait(struct NdbCondition* p_cond,
                  NdbMutex* p_mutex)
{
    int result;
    int bLastWaiter;
    if(!p_cond || !p_mutex)
        return 1;
    
    NdbMutex_Lock(p_cond->pNdbMutexWaitersLock);
    p_cond->nWaiters++;
    NdbMutex_Unlock(p_cond->pNdbMutexWaitersLock);
    
    if(NdbMutex_Unlock(p_mutex))
        return -1;
    result = WaitForSingleObject (p_cond->hSemaphore, INFINITE);

    NdbMutex_Lock(p_cond->pNdbMutexWaitersLock);
    p_cond->nWaiters--;
    bLastWaiter = (p_cond->bWasBroadcast && p_cond->nWaiters==0);
    NdbMutex_Unlock(p_cond->pNdbMutexWaitersLock);
    
    if(result==WAIT_OBJECT_0 && bLastWaiter)
        SetEvent(p_cond->hEventWaitersDone);
    
    NdbMutex_Lock(p_mutex);
    return result;
}


int 
NdbCondition_WaitTimeout(struct NdbCondition* p_cond,
                         NdbMutex* p_mutex,
                         int msecs)
{
    int result;
    int bLastWaiter;
    if (!p_cond || !p_mutex)
        return 1;
    
    NdbMutex_Lock(p_cond->pNdbMutexWaitersLock);
    p_cond->nWaiters++;
    NdbMutex_Unlock(p_cond->pNdbMutexWaitersLock);
    if(msecs<0)
        msecs = 0;

    if(NdbMutex_Unlock(p_mutex))
        return -1;
    result = WaitForSingleObject(p_cond->hSemaphore, msecs);

    NdbMutex_Lock(p_cond->pNdbMutexWaitersLock);
    p_cond->nWaiters--;
    bLastWaiter = (p_cond->bWasBroadcast && p_cond->nWaiters==0);
    NdbMutex_Unlock(p_cond->pNdbMutexWaitersLock);

    if(result!=WAIT_OBJECT_0)
        result = -1;

    if(bLastWaiter)
        SetEvent(p_cond->hEventWaitersDone); 

    NdbMutex_Lock(p_mutex);
    return result;
}


int 
NdbCondition_Signal(struct NdbCondition* p_cond)
{
    int bHaveWaiters;
    if(!p_cond)
        return 1;
    
    NdbMutex_Lock(p_cond->pNdbMutexWaitersLock);
    bHaveWaiters = (p_cond->nWaiters > 0);
    NdbMutex_Unlock(p_cond->pNdbMutexWaitersLock);
    
    if(bHaveWaiters)
        return (ReleaseSemaphore(p_cond->hSemaphore, 1, 0) ? 0 : -1);
    else
        return 0;
}


int NdbCondition_Broadcast(struct NdbCondition* p_cond)
{
    int bHaveWaiters;
    int result = 0;
    if(!p_cond)
        return 1;
    
    NdbMutex_Lock(p_cond->pNdbMutexWaitersLock);
    bHaveWaiters = 0;
    if(p_cond->nWaiters > 0)
    {
        p_cond->bWasBroadcast = !0;
        bHaveWaiters = 1;
    }
    NdbMutex_Unlock(p_cond->pNdbMutexWaitersLock);
    if(bHaveWaiters)
    {
        if(!ReleaseSemaphore(p_cond->hSemaphore, p_cond->nWaiters, 0))
            result = -1;
        else if(WaitForSingleObject (p_cond->hEventWaitersDone, INFINITE) != WAIT_OBJECT_0)
            result = -1;
        p_cond->bWasBroadcast = 0;
    }
    return result;
}


int NdbCondition_Destroy(struct NdbCondition* p_cond)
{
    int result;
    if(!p_cond)
        return 1;
    
    CloseHandle(p_cond->hEventWaitersDone);
    NdbMutex_Destroy(p_cond->pNdbMutexWaitersLock);
    result = (CloseHandle(p_cond->hSemaphore) ? 0 : -1); 

    free(p_cond);
    return 0;
}

