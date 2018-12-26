/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "xplatform/my_xp_thread.h"

#ifdef _WIN32
My_xp_thread_win::My_xp_thread_win()
  :m_handle(NULL), m_thread_var(NULL),
   m_thread(static_cast<native_thread_t *>(malloc(sizeof(native_thread_t)))),
   m_thread_once(static_cast<native_thread_once_t *>(malloc(sizeof(native_thread_once_t))))
{}


My_xp_thread_win::~My_xp_thread_win()
{
  free(m_thread);
  free((void *) m_thread_once);
}


native_thread_t *My_xp_thread_win::get_native_thread()
{
  return m_thread;
}


int My_xp_thread_win::create(const native_thread_attr_t *attr,
                             native_start_routine func,
                             void *arg)
{
  struct thread_start_parameter *par;
  unsigned int  stack_size;

  par= (struct thread_start_parameter *)malloc(sizeof(*par));
  if (!par)
    goto error_return;

  par->func=  func;
  par->arg=   arg;
  stack_size= attr ? attr->dwStackSize : 0;

  m_handle= (HANDLE)_beginthreadex(NULL, stack_size, win_thread_start,
                                    par, 0, (unsigned int *)m_thread);
  if (m_handle)
    return 0;

  my_osmaperr(GetLastError());
  free(par);

error_return:
  m_thread= 0;
  m_handle= NULL;
  return 1;
};


int My_xp_thread_win::once(void(*init_routine)(void))
{
  LONG state;

  /*
    Do "dirty" read to find out if initialization is already done, to
    save an interlocked operation in common case. Memory barriers are ensured
    by Visual C++ volatile implementation.
  */
  if (*m_thread_once == MY_THREAD_ONCE_DONE)
    return 0;

  state= InterlockedCompareExchange(m_thread_once, MY_THREAD_ONCE_INPROGRESS,
                                    MY_THREAD_ONCE_INIT);

  switch (state)
  {
  case MY_THREAD_ONCE_INIT:
    /* This is the initializer thread */
    (*init_routine)();
    *m_thread_once= MY_THREAD_ONCE_DONE;
    break;

  case MY_THREAD_ONCE_INPROGRESS:
    /* init_routine in progress. Wait for its completion */
    while (*m_thread_once == MY_THREAD_ONCE_INPROGRESS)
    {
      Sleep(1);
    }
    break;
  case MY_THREAD_ONCE_DONE:
    /* Nothing to do */
    break;
  }
  return 0;
}


int My_xp_thread_win::join(void **value_ptr)
{
  int result= 0;
  DWORD ret=  WaitForSingleObject(m_handle, INFINITE);

  if (ret != WAIT_OBJECT_0)
  {
    my_osmaperr(GetLastError());
    result= 1;
  }

  if (m_handle)
    CloseHandle(m_handle);

  m_thread= 0;
  m_handle= NULL;

  return result;
}


int My_xp_thread_win::cancel()
{
  BOOL ok= FALSE;

  if (m_handle)
  {
    ok= TerminateThread(m_handle, 0);
    CloseHandle(m_handle);
  }

  if (ok)
    return 0;

  errno= EINVAL;
  return -1;
}


int My_xp_thread_win::detach()
{
  BOOL ok= FALSE;

  if (m_handle)
  {
    ok= CloseHandle(m_handle);
  }

  if (ok)
    return 0;

  errno= EINVAL;
  return -1;
}


void My_xp_thread_util::exit(void *value_ptr)
{
  _endthreadex(0);
}


int My_xp_thread_util::attr_init(native_thread_attr_t *attr)
{
  attr->dwStackSize= 0;
  return 0;
}


int My_xp_thread_util::attr_destroy(native_thread_attr_t *attr)
{
  memset(attr, 0, sizeof(*attr));
  return 0;
}


native_thread_t My_xp_thread_util::self()
{
  return GetCurrentThreadId();
}


/**
  Sets errno corresponding to GetLastError() value.
*/

void My_xp_thread_win::my_osmaperr(unsigned long oserrno)
{
  /*
    set thr_winerr so that we could return the Windows Error Code
    when it is EINVAL.
  */
  m_thread_var= (st_native_thread_var *)malloc(sizeof(*m_thread_var));
  m_thread_var->thr_winerr= oserrno;
  m_thread_var->thr_errno= get_errno_from_oserr(oserrno);
}
#else
My_xp_thread_pthread::My_xp_thread_pthread()
  :m_thread(static_cast<native_thread_t *>(malloc(sizeof(native_thread_t)))),
   m_thread_once(static_cast<native_thread_once_t *>(malloc(sizeof(native_thread_once_t))))
{}


My_xp_thread_pthread::~My_xp_thread_pthread()
{
  free(m_thread);
  free(m_thread_once);
}

/* purecov: begin deadcode */
native_thread_t *My_xp_thread_pthread::get_native_thread()
{
  return m_thread;
}
/* purecov: end */

int My_xp_thread_pthread::create(const native_thread_attr_t *attr,
                                 native_start_routine func,
                                 void *arg)
{
  return pthread_create(m_thread, attr, func, arg);
};


int My_xp_thread_pthread::once(void (*init_routine)(void))
{
  return pthread_once(m_thread_once, init_routine);
}


int My_xp_thread_pthread::join(void **value_ptr)
{
  return pthread_join(*m_thread, value_ptr);
}

/* purecov: begin deadcode */
int My_xp_thread_pthread::cancel()
{
  return pthread_cancel(*m_thread);
}
/* purecov: end */

int My_xp_thread_pthread::detach()
{
  return pthread_detach(*m_thread);
}


void My_xp_thread_util::exit(void *value_ptr)
{
  pthread_exit(value_ptr);
}

/* purecov: begin deadcode */
int My_xp_thread_util::attr_init(native_thread_attr_t *attr)
{
  return pthread_attr_init(attr);
}


int My_xp_thread_util::attr_destroy(native_thread_attr_t *attr)
{
  return pthread_attr_destroy(attr);
}


native_thread_t My_xp_thread_util::self()
{
  return pthread_self();
}
/* purecov: end */
#endif
