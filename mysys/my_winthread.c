/* Copyright (c) 2000, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*****************************************************************************
** Simulation of posix threads calls for Windows
*****************************************************************************/
#if defined (_WIN32)
#include "mysys_priv.h"
#include <process.h>
#include <signal.h>

int pthread_attr_init(pthread_attr_t *connect_att)
{
  connect_att->dwStackSize	= 0;
  connect_att->dwCreatingFlag	= 0;
  return 0;
}


int pthread_attr_setstacksize(pthread_attr_t *connect_att, size_t stack)
{
  connect_att->dwStackSize= (DWORD)stack;
  return 0;
}


int pthread_attr_getstacksize(pthread_attr_t *connect_att, size_t *stack)
{
  *stack= (size_t)connect_att->dwStackSize;
  return 0;
}


int pthread_attr_destroy(pthread_attr_t *connect_att)
{
  memset(connect_att, 0, sizeof(*connect_att));
  return 0;
}


int pthread_dummy(int ret)
{
  return ret;
}

static void install_sigabrt_handler(void);

struct thread_start_parameter
{
  pthread_handler func;
  void *arg;
};

static unsigned int __stdcall pthread_start(void *p)
{
  struct thread_start_parameter *par= (struct thread_start_parameter *)p;
  pthread_handler func= par->func;
  void *arg= par->arg;
  free(p);
  (*func)(arg);
  return 0;
}


/**
  Create thread.
  This function provides combined implementation for
  pthread_create() and pthread_create_get_handle() functions.

  @param thread_id    reference to pthread object
  @param attr         reference to pthread attribute
  @param func         pthread handler function
  @param param        parameters to pass to newly created thread
  @param out_handle   reference to thread handle. This needs to be passed to
                      pthread_join_with_handle() function when it is joined
  @return int
    @retval 0 success
    @retval 1 failure

*/
int pthread_create_base(pthread_t *thread_id, const pthread_attr_t *attr,
                        pthread_handler func, void *param, HANDLE *out_handle)
{
  HANDLE handle= NULL;
  struct thread_start_parameter *par;
  unsigned int  stack_size;
  DBUG_ENTER("pthread_create");

  par= (struct thread_start_parameter *)malloc(sizeof(*par));
  if (!par)
   goto error_return;

  par->func= func;
  par->arg= param;
  stack_size= attr?attr->dwStackSize:0;

  handle= (HANDLE)_beginthreadex(NULL, stack_size , pthread_start,
                                 par, 0, thread_id);
  if (!handle)
  {
    my_osmaperr(GetLastError());
    free(par);
    goto error_return;
  }
  DBUG_PRINT("info", ("thread id=%u",*thread_id));

  if (!out_handle)
  {
    /* Do not need thread handle, close it */
    CloseHandle(handle);
    DBUG_RETURN(0);
  }
  else
  {
    /* Save thread handle, it will be used later during join */
    *out_handle= handle;
    DBUG_RETURN(0);
  }

error_return:
  DBUG_PRINT("error",
         ("Can't create thread to handle request (error %d)",errno));
  DBUG_RETURN(1);
}

int pthread_create(pthread_t *thread_id, const pthread_attr_t *attr,
                   pthread_handler func, void *param)
{
  return pthread_create_base(thread_id, attr, func, param, NULL);
}

/*
  Existing mysql_thread_create does not work well in windows platform
  when threads are joined because
  A)during thread creation thread handle is not stored.
  B)during thread join, thread handle is retrieved using OpenThread().
    OpenThread() does not behave properly when thread to be joined is already
    exited.

  Due to the above issue, pthread_create_get_handle() and
  pthread_join_with_handle() needs to be used
  This function returns the handle in output parameter out_handle.
  Caller is expected to store this value and use it during
  pthread_join_with_handle() function call.
*/
int pthread_create_get_handle(pthread_t *thread_id,
                              const pthread_attr_t *attr,
                              pthread_handler func, void *param,
                              HANDLE *out_handle)
{
  return pthread_create_base(thread_id, attr, func, param, out_handle);
}

/**
   Get thread HANDLE.
   @param thread      reference to pthread object
   @return int
     @retval !NULL    valid thread handle
     @retval NULL     failure

*/
HANDLE pthread_get_handle(pthread_t thread_id)
{
  HANDLE handle;

  handle= OpenThread(SYNCHRONIZE, FALSE, thread_id);
  if (!handle)
    my_osmaperr(GetLastError());
  return handle;
}

void pthread_exit(void *a)
{
  _endthreadex(0);
}

/**
  Join thread.
  This function provides combined implementation for
  pthread_join() and pthread_join_with_handle() functions.

  @param thread      reference to pthread object
  @param handle      thread handle of thread to be joined
  @return int
    @retval 0 success
    @retval 1 failure

*/
int pthread_join_base(pthread_t thread, HANDLE handle)
{
  DWORD  ret;
  if (!handle)
  {
     handle= OpenThread(SYNCHRONIZE, FALSE, thread);
     if (!handle)
     {
       my_osmaperr(GetLastError());
       goto error_return;
     }
  }
  ret= WaitForSingleObject(handle, INFINITE);
  if(ret != WAIT_OBJECT_0)
  {
    my_osmaperr(GetLastError());
    goto error_return;
  }
  CloseHandle(handle);
  return 0;

error_return:
  if(handle)
    CloseHandle(handle);
  return 1;
}

/*
  This function is unsafe in windows. Use pthread_create_get_handle()
  and pthread_join_with_handle() instead.
  During thread join, thread handle is retrieved using OpenThread().
  OpenThread() does not behave properly when thread to be joined is already
  exited.
*/
int pthread_join(pthread_t thread, void **value_ptr)
{
  return pthread_join_base(thread, NULL);
}

int pthread_join_with_handle(HANDLE handle)
{
  pthread_t dummy= 0;
  return pthread_join_base(dummy, handle);
}

int pthread_cancel(pthread_t thread)
{

  HANDLE handle= 0;
  BOOL ok= FALSE;

  handle= OpenThread(THREAD_TERMINATE, FALSE, thread);
  if (handle)
  {
     ok= TerminateThread(handle,0);
     CloseHandle(handle);
  }
  if (ok)
    return 0;

  errno= EINVAL;
  return -1;
}

/*
 One time initialization. For simplicity, we assume initializer thread
 does not exit within init_routine().
*/
int my_pthread_once(my_pthread_once_t *once_control, 
    void (*init_routine)(void))
{
  LONG state;

  /*
    Do "dirty" read to find out if initialization is already done, to
    save an interlocked operation in common case. Memory barriers are ensured by 
    Visual C++ volatile implementation.
  */
  if (*once_control == MY_PTHREAD_ONCE_DONE)
    return 0;

  state= InterlockedCompareExchange(once_control, MY_PTHREAD_ONCE_INPROGRESS,
                                        MY_PTHREAD_ONCE_INIT);

  switch(state)
  {
  case MY_PTHREAD_ONCE_INIT:
    /* This is initializer thread */
    (*init_routine)();
    *once_control= MY_PTHREAD_ONCE_DONE;
    break;

  case MY_PTHREAD_ONCE_INPROGRESS:
    /* init_routine in progress. Wait for its completion */
    while(*once_control == MY_PTHREAD_ONCE_INPROGRESS)
    {
      Sleep(1);
    }
    break;
  case MY_PTHREAD_ONCE_DONE:
    /* Nothing to do */
    break;
  }
  return 0;
}
#endif
