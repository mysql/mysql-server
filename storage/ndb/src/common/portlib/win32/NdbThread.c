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

#include <ndb_global.h>
#include "NdbThread.h"
#include <process.h>

#define MAX_THREAD_NAME 16

typedef unsigned (WINAPI* NDB_WIN32_THREAD_FUNC)(void*);


struct NdbThread 
{ 
    HANDLE hThread;
    unsigned nThreadId;
    char thread_name[MAX_THREAD_NAME];
};


struct NdbThread* NdbThread_Create(NDB_THREAD_FUNC *p_thread_func,
                                   NDB_THREAD_ARG *p_thread_arg,
                                   const NDB_THREAD_STACKSIZE thread_stack_size,
                                   const char* p_thread_name,
                                   NDB_THREAD_PRIO thread_prio)
{
    struct NdbThread* tmpThread;
    unsigned initflag;
    int nPriority = 0;

    if(!p_thread_func)
        return 0;
    
    tmpThread = (struct NdbThread*)malloc(sizeof(struct NdbThread));
    if(!tmpThread)
        return 0;
    
    strncpy((char*)&tmpThread->thread_name, p_thread_name, MAX_THREAD_NAME);
    
    switch(thread_prio)
    {
    case NDB_THREAD_PRIO_HIGHEST: nPriority=THREAD_PRIORITY_HIGHEST; break;
    case NDB_THREAD_PRIO_HIGH: nPriority=THREAD_PRIORITY_ABOVE_NORMAL; break;
    case NDB_THREAD_PRIO_MEAN: nPriority=THREAD_PRIORITY_NORMAL; break;
    case NDB_THREAD_PRIO_LOW: nPriority=THREAD_PRIORITY_BELOW_NORMAL; break;
    case NDB_THREAD_PRIO_LOWEST: nPriority=THREAD_PRIORITY_LOWEST; break;
    }
    initflag = (nPriority ? CREATE_SUSPENDED : 0);
    
    tmpThread->hThread = (HANDLE)_beginthreadex(0, thread_stack_size,
        (NDB_WIN32_THREAD_FUNC)p_thread_func, p_thread_arg, 
        initflag, &tmpThread->nThreadId);
    
    if(nPriority && tmpThread->hThread)
    {
        SetThreadPriority(tmpThread->hThread, nPriority);
        ResumeThread (tmpThread->hThread);
    }
    
    assert(tmpThread->hThread);
    return tmpThread;
}


void NdbThread_Destroy(struct NdbThread** p_thread)
{
    CloseHandle((*p_thread)->hThread);
    (*p_thread)->hThread = 0;
    free(*p_thread); 
    *p_thread = 0;
}


int NdbThread_WaitFor(struct NdbThread* p_wait_thread, void** status)
{
    void *local_status = 0;
    if (status == 0)
        status = &local_status;
    
    if(WaitForSingleObject(p_wait_thread->hThread, INFINITE) == WAIT_OBJECT_0
        && GetExitCodeThread(p_wait_thread->hThread, (LPDWORD)status))
    {
        CloseHandle(p_wait_thread->hThread);
        p_wait_thread->hThread = 0;
        return 0;
    }
    return -1; 
}


void NdbThread_Exit(int status)
{
    _endthreadex((DWORD) status);
}


int NdbThread_SetConcurrencyLevel(int level)
{
    return 0;
}

