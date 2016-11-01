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

#ifndef MY_XP_THREAD_INCLUDED
#define MY_XP_THREAD_INCLUDED

#include <xplatform/my_xp_cond.h>

#ifndef ETIME
#define ETIME ETIMEDOUT                         /* For FreeBSD */
#endif

#ifndef ETIMEDOUT
#define ETIMEDOUT 145                           /* Win32 doesn't have this */
#endif

typedef unsigned int uint32;
typedef uint32 native_thread_id;

#ifdef _WIN32

typedef volatile LONG    native_thread_once_t;
typedef DWORD            native_thread_t;
typedef struct thread_attr
{
  DWORD dwStackSize;
} native_thread_attr_t;
typedef void *(__cdecl *native_start_routine)(void *);
#define MY_THREAD_ONCE_INIT       0
#define MY_THREAD_ONCE_INPROGRESS 1
#define MY_THREAD_ONCE_DONE       2

#include <process.h>
#include <signal.h>

struct thread_start_parameter
{
  native_start_routine func;
  void *arg;
};


static unsigned int __stdcall win_thread_start(void *p)
{
  struct thread_start_parameter *par= (struct thread_start_parameter *)p;
  native_start_routine func=          par->func;
  void *arg=                          par->arg;

  free(p);
  (*func)(arg);
  return 0;
}


/* All thread specific variables are in the following struct */
struct st_native_thread_var
{
  int thr_errno;
  /*
    thr_winerr is used for returning the original OS error-code in Windows,
    my_osmaperr() returns EINVAL for all unknown Windows errors, hence we
    preserve the original Windows Error code in thr_winerr.
  */
  int thr_winerr;
  native_cond_t suspend;
  native_thread_id id;
  int volatile abort;
  struct st_native_thread_var *next, **prev;
  void *opt_info;
#ifndef DBUG_OFF
  void *dbug;
#endif
};


int set_mysys_thread_var(struct st_native_thread_var *mysys_var);

#ifndef DBUG_OFF
/**
  Returns pointer to DBUG for holding current state.
*/
void **my_thread_var_dbug();
#endif

#define my_errno mysys_thread_var()->thr_errno

struct errentry
{
  unsigned long oscode;                         /* OS return value */
  int sysv_errno;                               /* System V error code */
};

static struct errentry errtable[]= {
  {  ERROR_INVALID_FUNCTION,       EINVAL    },  /* 1 */
  {  ERROR_FILE_NOT_FOUND,         ENOENT    },  /* 2 */
  {  ERROR_PATH_NOT_FOUND,         ENOENT    },  /* 3 */
  {  ERROR_TOO_MANY_OPEN_FILES,    EMFILE    },  /* 4 */
  {  ERROR_ACCESS_DENIED,          EACCES    },  /* 5 */
  {  ERROR_INVALID_HANDLE,         EBADF     },  /* 6 */
  {  ERROR_ARENA_TRASHED,          ENOMEM    },  /* 7 */
  {  ERROR_NOT_ENOUGH_MEMORY,      ENOMEM    },  /* 8 */
  {  ERROR_INVALID_BLOCK,          ENOMEM    },  /* 9 */
  {  ERROR_BAD_ENVIRONMENT,        E2BIG     },  /* 10 */
  {  ERROR_BAD_FORMAT,             ENOEXEC   },  /* 11 */
  {  ERROR_INVALID_ACCESS,         EINVAL    },  /* 12 */
  {  ERROR_INVALID_DATA,           EINVAL    },  /* 13 */
  {  ERROR_INVALID_DRIVE,          ENOENT    },  /* 15 */
  {  ERROR_CURRENT_DIRECTORY,      EACCES    },  /* 16 */
  {  ERROR_NOT_SAME_DEVICE,        EXDEV     },  /* 17 */
  {  ERROR_NO_MORE_FILES,          ENOENT    },  /* 18 */
  {  ERROR_LOCK_VIOLATION,         EACCES    },  /* 33 */
  {  ERROR_BAD_NETPATH,            ENOENT    },  /* 53 */
  {  ERROR_NETWORK_ACCESS_DENIED,  EACCES    },  /* 65 */
  {  ERROR_BAD_NET_NAME,           ENOENT    },  /* 67 */
  {  ERROR_FILE_EXISTS,            EEXIST    },  /* 80 */
  {  ERROR_CANNOT_MAKE,            EACCES    },  /* 82 */
  {  ERROR_FAIL_I24,               EACCES    },  /* 83 */
  {  ERROR_INVALID_PARAMETER,      EINVAL    },  /* 87 */
  {  ERROR_NO_PROC_SLOTS,          EAGAIN    },  /* 89 */
  {  ERROR_DRIVE_LOCKED,           EACCES    },  /* 108 */
  {  ERROR_BROKEN_PIPE,            EPIPE     },  /* 109 */
  {  ERROR_DISK_FULL,              ENOSPC    },  /* 112 */
  {  ERROR_INVALID_TARGET_HANDLE,  EBADF     },  /* 114 */
  {  ERROR_INVALID_NAME,           ENOENT    },  /* 123 */
  {  ERROR_INVALID_HANDLE,         EINVAL    },  /* 124 */
  {  ERROR_WAIT_NO_CHILDREN,       ECHILD    },  /* 128 */
  {  ERROR_CHILD_NOT_COMPLETE,     ECHILD    },  /* 129 */
  {  ERROR_DIRECT_ACCESS_HANDLE,   EBADF     },  /* 130 */
  {  ERROR_NEGATIVE_SEEK,          EINVAL    },  /* 131 */
  {  ERROR_SEEK_ON_DEVICE,         EACCES    },  /* 132 */
  {  ERROR_DIR_NOT_EMPTY,          ENOTEMPTY },  /* 145 */
  {  ERROR_NOT_LOCKED,             EACCES    },  /* 158 */
  {  ERROR_BAD_PATHNAME,           ENOENT    },  /* 161 */
  {  ERROR_MAX_THRDS_REACHED,      EAGAIN    },  /* 164 */
  {  ERROR_LOCK_FAILED,            EACCES    },  /* 167 */
  {  ERROR_ALREADY_EXISTS,         EEXIST    },  /* 183 */
  {  ERROR_FILENAME_EXCED_RANGE,   ENOENT    },  /* 206 */
  {  ERROR_NESTING_NOT_ALLOWED,    EAGAIN    },  /* 215 */
  {  ERROR_NOT_ENOUGH_QUOTA,       ENOMEM    }    /* 1816 */
};

/* size of the table */
#define ERRTABLESIZE (sizeof(errtable)/sizeof(errtable[0]))

/*
  The following two constants must be the minimum and maximum
  values in the (contiguous) range of Exec Failure errors.
*/
#define MIN_EXEC_ERROR ERROR_INVALID_STARTING_CODESEG
#define MAX_EXEC_ERROR ERROR_INFLOOP_IN_RELOC_CHAIN

/*
  These are the low and high value in the range of errors that are access
  violations.
*/
#define MIN_EACCES_RANGE ERROR_WRITE_PROTECT
#define MAX_EACCES_RANGE ERROR_SHARING_BUFFER_EXCEEDED


static int get_errno_from_oserr(unsigned long oserrno)
{
  int i;

  /* check the table for the OS error code */
  for (i= 0; i < ERRTABLESIZE; ++i)
  {
    if (oserrno == errtable[i].oscode)
    {
      return  errtable[i].sysv_errno;
    }
  }

  /*
    The error code wasn't in the table.  We check for a range of
    EACCES errors or exec failure errors (ENOEXEC).  Otherwise
    EINVAL is returned.
  */

  if (oserrno >= MIN_EACCES_RANGE && oserrno <= MAX_EACCES_RANGE)
    return EACCES;
  else if (oserrno >= MIN_EXEC_ERROR && oserrno <= MAX_EXEC_ERROR)
    return ENOEXEC;
  else
    return EINVAL;
}

#else
#include <pthread.h>

typedef pthread_once_t   native_thread_once_t;
typedef pthread_t        native_thread_t;
typedef pthread_attr_t   native_thread_attr_t;
typedef void *(*native_start_routine)(void *);
#define MY_THREAD_ONCE_INIT       PTHREAD_ONCE_INIT
#endif

/**
  @class My_xp_thread

  Abstract class used to wrap mutex for various platforms.

  A typical use case is:

  @code{.cpp}

  My_xp_thread thread= My_xp_thread::get_thread();
  thread->create(NULL, &function, &args);

  void *result;
  thread->join(&result);

  @endcode
*/
class My_xp_thread
{
public:
  /**
    Creates thread.

    @param thread attributes
    @param routine function
    @param function parameters
    @return success status
  */

  virtual int create(const native_thread_attr_t *attr,
                     native_start_routine func, void *arg)= 0;


  /**
    One time initialization.

    @param init routine to invoke
    @return success status
  */

  virtual int once(void (*init_routine)(void))= 0;


  /**
    Suspend invoking thread until this thread terminates.

    @param pointer for a placeholder for the terminating thread status
    @return success status
  */

  virtual int join(void **value_ptr)= 0;


  /**
    Cancel this thread.

    @return success status
  */

  virtual int cancel()= 0;


  /**
    Detach this thread, i.e. its resources can be reclaimed when it terminates.

    @return success status
  */

  virtual int detach()= 0;


  /**
    Retrieves native thread reference

    @return native thread pointer
  */

  virtual native_thread_t *get_native_thread()= 0;

  virtual ~My_xp_thread() {}
};

#ifdef _WIN32
class My_xp_thread_win : public My_xp_thread
{
private:
  HANDLE m_handle;
  void my_osmaperr( unsigned long oserrno);
  st_native_thread_var *m_thread_var;
  /*
    Disabling the copy constructor and assignment operator.
  */
  My_xp_thread_win(My_xp_thread_win const&);
  My_xp_thread_win& operator=(My_xp_thread_win const&);
public:
  explicit My_xp_thread_win();
  virtual ~My_xp_thread_win();
#else
class My_xp_thread_pthread : public My_xp_thread
{
private:
  /*
    Disabling the copy constructor and assignment operator.
  */
  My_xp_thread_pthread(My_xp_thread_pthread const&);
  My_xp_thread_pthread& operator=(My_xp_thread_pthread const&);
public:
  explicit My_xp_thread_pthread();
  virtual ~My_xp_thread_pthread();
#endif
  int create(const native_thread_attr_t *attr, native_start_routine func,
             void *arg);
  int once(void (*init_routine)(void));
  int join(void **value_ptr);
  int cancel();
  int detach();
  native_thread_t *get_native_thread();

protected:
  native_thread_t *m_thread;
  native_thread_once_t *m_thread_once;
};

#ifdef _WIN32
class My_xp_thread_impl : public My_xp_thread_win
#else
class My_xp_thread_impl : public My_xp_thread_pthread
#endif
{
public:
  explicit My_xp_thread_impl() {}
  ~My_xp_thread_impl() {}
};

class My_xp_thread_util
{
public:
  /**
    Terminate invoking thread.

    @param thread exit value pointer
  */

  static void exit(void *value_ptr);


  /**
    Initialize thread attributes object.

    @param thread attributes
    @return success status
  */

  static int attr_init(native_thread_attr_t *attr);


  /**
    Destroy thread attributes object.

    @param thread attributes
    @return success status
  */

  static int attr_destroy(native_thread_attr_t *attr);


  /**
    Retrieve current thread id.

    @return current thread id
  */

  static native_thread_t self();
};

#endif // MY_XP_THREAD_INCLUDED
