/* Copyright (C) 2012 Monty Program Ab

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

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif

#define _WIN32_WINNT 0x0601

#include <my_global.h>
#include <violite.h>
#include <sql_priv.h>
#include <sql_class.h>
#include <my_pthread.h>
#include <scheduler.h>
#include <sql_connect.h>
#include <mysqld.h>
#include <debug_sync.h>
#include <threadpool.h>
#include <windows.h>


/*
  Threadpool API is not available on XP. We still want to compile a single 
  version on Windows, but use the latest functionality if available.
  We cannot use threadpool functionality directly, since executable won't 
  start on XP and loader will complain about missing symbols.

  We solve using the usual way it is done on Windows, i.e with dynamic loading.
  We'll need to load a lot of function, and make this less painful with the
  WEAK_SYMBOL macro below
*/

/*
 WEAK_SYMBOL(return_type, function_name, argument_type1,..,argument_typeN)

 Declare and load function pointer from kernel32. The name of the static 
 variable that holds the function pointer is my_<original function name>
 This should be combined with 
 #define <original function name> my_<original function name>
 so that one could use Widows APIs transparently, without worrying whether
 they are present in a particular version or not.

 Of course, prior to use of any function there should be a check for correct
 Windows version, or check whether function pointer is not NULL.
*/
#define WEAK_SYMBOL(return_type, function, ...) \
  typedef return_type (WINAPI *pFN_##function)(__VA_ARGS__); \
  static pFN_##function my_##function = (pFN_##function) \
    (GetProcAddress(GetModuleHandle("kernel32"),#function))

WEAK_SYMBOL(VOID, CancelThreadpoolIo, PTP_IO);
#define CancelThreadpoolIo my_CancelThreadpoolIo

WEAK_SYMBOL(VOID, CloseThreadpool, PTP_POOL);
#define CloseThreadpool my_CloseThreadpool

WEAK_SYMBOL(VOID, CloseThreadpoolIo, PTP_IO);
#define CloseThreadpoolIo my_CloseThreadpoolIo

WEAK_SYMBOL(VOID, CloseThreadpoolTimer,PTP_TIMER);
#define CloseThreadpoolTimer my_CloseThreadpoolTimer

WEAK_SYMBOL(VOID, CloseThreadpoolWait,PTP_WAIT);
#define CloseThreadpoolWait my_CloseThreadpoolWait

WEAK_SYMBOL(PTP_POOL, CreateThreadpool,PVOID);
#define CreateThreadpool my_CreateThreadpool

WEAK_SYMBOL(PTP_IO, CreateThreadpoolIo, HANDLE, PTP_WIN32_IO_CALLBACK, PVOID ,
  PTP_CALLBACK_ENVIRON);
#define CreateThreadpoolIo my_CreateThreadpoolIo

WEAK_SYMBOL(PTP_TIMER, CreateThreadpoolTimer, PTP_TIMER_CALLBACK ,
 PVOID pv, PTP_CALLBACK_ENVIRON pcbe);
#define CreateThreadpoolTimer my_CreateThreadpoolTimer

WEAK_SYMBOL(PTP_WAIT, CreateThreadpoolWait, PTP_WAIT_CALLBACK, PVOID, 
  PTP_CALLBACK_ENVIRON);
#define CreateThreadpoolWait my_CreateThreadpoolWait

WEAK_SYMBOL(VOID, DisassociateCurrentThreadFromCallback, PTP_CALLBACK_INSTANCE);
#define DisassociateCurrentThreadFromCallback my_DisassociateCurrentThreadFromCallback

WEAK_SYMBOL(DWORD, FlsAlloc, PFLS_CALLBACK_FUNCTION);
#define FlsAlloc my_FlsAlloc

WEAK_SYMBOL(PVOID, FlsGetValue, DWORD);
#define FlsGetValue my_FlsGetValue

WEAK_SYMBOL(BOOL, FlsSetValue, DWORD, PVOID);
#define FlsSetValue my_FlsSetValue

WEAK_SYMBOL(VOID, SetThreadpoolThreadMaximum, PTP_POOL, DWORD);
#define SetThreadpoolThreadMaximum my_SetThreadpoolThreadMaximum

WEAK_SYMBOL(BOOL, SetThreadpoolThreadMinimum, PTP_POOL, DWORD);
#define SetThreadpoolThreadMinimum my_SetThreadpoolThreadMinimum

WEAK_SYMBOL(VOID, SetThreadpoolTimer, PTP_TIMER, PFILETIME,DWORD,DWORD);
#define SetThreadpoolTimer my_SetThreadpoolTimer

WEAK_SYMBOL(VOID, SetThreadpoolWait, PTP_WAIT,HANDLE,PFILETIME);
#define SetThreadpoolWait my_SetThreadpoolWait

WEAK_SYMBOL(VOID, StartThreadpoolIo, PTP_IO);
#define StartThreadpoolIo my_StartThreadpoolIo

WEAK_SYMBOL(VOID, WaitForThreadpoolIoCallbacks,PTP_IO, BOOL);
#define WaitForThreadpoolIoCallbacks my_WaitForThreadpoolIoCallbacks

WEAK_SYMBOL(VOID, WaitForThreadpoolTimerCallbacks, PTP_TIMER, BOOL);
#define WaitForThreadpoolTimerCallbacks my_WaitForThreadpoolTimerCallbacks

WEAK_SYMBOL(VOID, WaitForThreadpoolWaitCallbacks, PTP_WAIT, BOOL);
#define WaitForThreadpoolWaitCallbacks my_WaitForThreadpoolWaitCallbacks

WEAK_SYMBOL(BOOL, SetFileCompletionNotificationModes, HANDLE, UCHAR);
#define SetFileCompletionNotificationModes my_SetFileCompletionNotificationModes

WEAK_SYMBOL(BOOL, TrySubmitThreadpoolCallback, PTP_SIMPLE_CALLBACK pfns, 
  PVOID pv,PTP_CALLBACK_ENVIRON pcbe);
#define TrySubmitThreadpoolCallback my_TrySubmitThreadpoolCallback

WEAK_SYMBOL(PTP_WORK, CreateThreadpoolWork, PTP_WORK_CALLBACK pfnwk, PVOID pv,
  PTP_CALLBACK_ENVIRON pcbe);
#define CreateThreadpoolWork my_CreateThreadpoolWork

WEAK_SYMBOL(VOID, SubmitThreadpoolWork,PTP_WORK pwk);
#define SubmitThreadpoolWork my_SubmitThreadpoolWork

WEAK_SYMBOL(VOID, CloseThreadpoolWork, PTP_WORK pwk);
#define CloseThreadpoolWork my_CloseThreadpoolWork 

WEAK_SYMBOL(BOOL, CallbackMayRunLong, PTP_CALLBACK_INSTANCE pci);
#define CallbackMayRunLong my_CallbackMayRunLong

#if _MSC_VER >= 1600
/* Stack size manipulation available only on Win7+ /declarations in VS10 */
WEAK_SYMBOL(BOOL, SetThreadpoolStackInformation, PTP_POOL, 
  PTP_POOL_STACK_INFORMATION);
#define SetThreadpoolStackInformation my_SetThreadpoolStackInformation
#else /* _MSC_VER < 1600 */
#define SetThreadpoolCallbackPriority(env,prio)
typedef enum _TP_CALLBACK_PRIORITY {
    TP_CALLBACK_PRIORITY_HIGH,
    TP_CALLBACK_PRIORITY_NORMAL,
    TP_CALLBACK_PRIORITY_LOW,
    TP_CALLBACK_PRIORITY_INVALID
} TP_CALLBACK_PRIORITY;
#endif


/* Log a warning */
static void tp_log_warning(const char *msg, const char *fct)
{
  sql_print_warning("Threadpool: %s. %s failed (last error %d)",msg, fct,
    GetLastError());
}


PTP_POOL pool;
DWORD fls;

static bool skip_completion_port_on_success = false;

/*
  Threadpool callbacks.

  io_completion_callback  - handle client request
  timer_callback - handle wait timeout (kill connection)
  shm_read_callback, shm_close_callback - shared memory stuff
  login_callback - user login (submitted as threadpool work)

*/

static void CALLBACK timer_callback(PTP_CALLBACK_INSTANCE instance, 
  PVOID context, PTP_TIMER timer);

static void CALLBACK io_completion_callback(PTP_CALLBACK_INSTANCE instance, 
  PVOID context,  PVOID overlapped,  ULONG io_result, ULONG_PTR nbytes, PTP_IO io);

static void CALLBACK shm_read_callback(PTP_CALLBACK_INSTANCE instance,
  PVOID Context, PTP_WAIT wait,TP_WAIT_RESULT wait_result);

static void CALLBACK shm_close_callback(PTP_CALLBACK_INSTANCE instance,
  PVOID Context, PTP_WAIT wait,TP_WAIT_RESULT wait_result);

static void check_thread_init();

/* Get current time as Windows time */
static ulonglong now()
{
  ulonglong current_time;
  GetSystemTimeAsFileTime((PFILETIME)&current_time);
  return current_time;
}

/* 
  Connection structure, encapsulates THD + structures for asynchronous
  IO and pool.
*/

struct connection_t
{
  THD *thd;
  HANDLE handle;
  OVERLAPPED overlapped;
  /* absolute time for wait timeout (as Windows time) */
  volatile ulonglong timeout; 
  TP_CALLBACK_ENVIRON callback_environ;
  PTP_IO  io;
  PTP_TIMER timer;
  PTP_WAIT shm_read;
  /* Callback instance, used to inform treadpool about long callbacks */
  PTP_CALLBACK_INSTANCE callback_instance;
  bool logged_in;
};


void init_connection(connection_t *connection)
{
  connection->logged_in = false;
  connection->handle= 0;
  connection->io= 0;
  connection->shm_read= 0;
  connection->timer= 0;
  connection->logged_in = false;
  connection->timeout= ULONGLONG_MAX;
  connection->callback_instance= 0;
  memset(&connection->overlapped, 0, sizeof(OVERLAPPED));
  InitializeThreadpoolEnvironment(&connection->callback_environ);
  SetThreadpoolCallbackPool(&connection->callback_environ, pool);
  connection->thd = 0;
}


int init_io(connection_t *connection, THD *thd)
{
  connection->thd= thd;
  Vio *vio = thd->net.vio;
  switch(vio->type)
  {
    case VIO_TYPE_SSL:
    case VIO_TYPE_TCPIP:
      connection->handle= (HANDLE)vio->sd;
      break;
    case VIO_TYPE_NAMEDPIPE:
      connection->handle= (HANDLE)vio->hPipe;
      break;
    case VIO_TYPE_SHARED_MEMORY:
      connection->shm_read=  CreateThreadpoolWait(shm_read_callback, connection, 
        &connection->callback_environ);
      if (!connection->shm_read)
      {
        tp_log_warning("Allocation failed", "CreateThreadpoolWait");
        return -1;
      }
      break;
    default:
      abort();
  }

  if (connection->handle)
  {
    /* Performance tweaks (s. MSDN documentation)*/
    UCHAR flags= FILE_SKIP_SET_EVENT_ON_HANDLE;
    if (skip_completion_port_on_success)
    {
      flags |= FILE_SKIP_COMPLETION_PORT_ON_SUCCESS;
    }
    (void)SetFileCompletionNotificationModes(connection->handle, flags);

    /* Assign io completion callback */
    connection->io= CreateThreadpoolIo(connection->handle, 
      io_completion_callback, connection, &connection->callback_environ);
    if(!connection->io)
    {
      tp_log_warning("Allocation failed", "CreateThreadpoolWait");
      return -1;
    }
  }
  connection->timer= CreateThreadpoolTimer(timer_callback, connection, 
    &connection->callback_environ);
  if (!connection->timer)
  {
    tp_log_warning("Allocation failed", "CreateThreadpoolWait");
    return -1;
  }

  return 0;
}


/*
  Start asynchronous read
*/
int start_io(connection_t *connection, PTP_CALLBACK_INSTANCE instance)
{
  /* Start async read */
  DWORD num_bytes = 0;
  static char c;
  WSABUF buf;
  buf.buf= &c;
  buf.len= 0;
  DWORD flags=0;
  DWORD last_error= 0;

  int retval;
  Vio *vio= connection->thd->net.vio;

  if (vio->type == VIO_TYPE_SHARED_MEMORY)
  {
      SetThreadpoolWait(connection->shm_read, vio->event_server_wrote, NULL);
      return 0;
  }
  if (vio->type == VIO_CLOSED)
  {
    return -1;
  }

  DBUG_ASSERT(vio->type == VIO_TYPE_TCPIP || 
    vio->type == VIO_TYPE_SSL ||
    vio->type == VIO_TYPE_NAMEDPIPE);

  OVERLAPPED *overlapped= &connection->overlapped;
  PTP_IO io= connection->io;
  StartThreadpoolIo(io);

  if (vio->type == VIO_TYPE_TCPIP || vio->type == VIO_TYPE_SSL)
  {
    /* Start async io (sockets). */
    if (WSARecv(vio->sd , &buf, 1, &num_bytes, &flags,
          overlapped,  NULL) == 0)
    {
        retval= last_error= 0;
    }
    else
    {
      retval= -1;
      last_error=  WSAGetLastError();
    }
  }
  else
  {
    /* Start async io (named pipe) */
    if (ReadFile(vio->hPipe, &c, 0, &num_bytes ,overlapped))
    {
      retval= last_error= 0;
    }
    else
    {
      retval= -1;
      last_error= GetLastError();
    }
  }

  if (retval == 0 || last_error == ERROR_MORE_DATA)
  {
    /*
      IO successfully finished (synchronously). 
      If skip_completion_port_on_success is set, we need to handle it right 
      here, because completion callback would not be executed by the pool.
    */
    if(skip_completion_port_on_success)
    {
      CancelThreadpoolIo(io);
      io_completion_callback(instance, connection, overlapped, last_error, 
        num_bytes, io);
    }
    return 0;
  }

  if(last_error == ERROR_IO_PENDING)
  {
    return 0;
  }

  /* Some error occured */
  CancelThreadpoolIo(io);
  return -1;
}


int login(connection_t *connection, PTP_CALLBACK_INSTANCE instance)
{
  if (threadpool_add_connection(connection->thd) == 0
      && init_io(connection, connection->thd) == 0 
      && start_io(connection, instance) == 0)
  {
    return 0;
  }
  return -1;
}

/*
  Recalculate wait timeout, maybe reset timer. 
*/
void set_wait_timeout(connection_t *connection, ulonglong old_timeout)
{
  ulonglong new_timeout = now() + 
    10000000LL*connection->thd->variables.net_wait_timeout;

  if (new_timeout < old_timeout)
  {
    SetThreadpoolTimer(connection->timer, (PFILETIME) &new_timeout, 0, 1000);
  }
  connection->timeout = new_timeout;
}


/* Connection destructor */
void destroy_connection(connection_t *connection, PTP_CALLBACK_INSTANCE instance)
{
  if (instance)
    DisassociateCurrentThreadFromCallback(instance);
  if (connection->io)
  {
     WaitForThreadpoolIoCallbacks(connection->io, TRUE); 
     CloseThreadpoolIo(connection->io);
  }

  if(connection->shm_read)
  {
    WaitForThreadpoolWaitCallbacks(connection->shm_read, TRUE);
    CloseThreadpoolWait(connection->shm_read);
  }

  if(connection->timer)
  {
    SetThreadpoolTimer(connection->timer, 0, 0, 0);
    WaitForThreadpoolTimerCallbacks(connection->timer, TRUE);
    CloseThreadpoolTimer(connection->timer);
  }
  
  if (connection->thd)
  {
    threadpool_remove_connection(connection->thd);
  }

  DestroyThreadpoolEnvironment(&connection->callback_environ);
}



/* 
  This function should be called first whenever a callback is invoked in the 
  threadpool, does my_thread_init() if not yet done
*/
extern ulong thread_created;
static void check_thread_init()
{
  if (FlsGetValue(fls) == NULL)
  {
    FlsSetValue(fls, (void *)1);
    thread_created++;
    InterlockedIncrement((volatile long *)&tp_stats.num_worker_threads);
  }
}


/*
  Decrement number of threads when a thread exits . 
  On Windows, FlsAlloc() provides the thread destruction callbacks.
*/
static VOID WINAPI thread_destructor(void *data)
{
  if(data)
  {
    InterlockedDecrement((volatile long *)&tp_stats.num_worker_threads);
  }
}


/* Scheduler callback : init */
bool tp_init(void)
{
  fls= FlsAlloc(thread_destructor);
  pool= CreateThreadpool(NULL);
  if(!pool)
  {
    sql_print_error("Can't create threadpool. "
      "CreateThreadpool() failed with %d. Likely cause is memory pressure", 
      GetLastError());
    exit(1);
  }

  if (threadpool_max_threads)
  {
    SetThreadpoolThreadMaximum(pool,threadpool_max_threads);
  }

  if (threadpool_min_threads)
  {
    if (!SetThreadpoolThreadMinimum(pool, threadpool_min_threads))
    {
      tp_log_warning( "Can't set threadpool minimum threads", 
        "SetThreadpoolThreadMinimum");
    }
  }

  /*
    Control stack size (OS must be Win7 or later, plus corresponding SDK)
  */
#if _MSC_VER >=1600
  if (SetThreadpoolStackInformation)
  {
    TP_POOL_STACK_INFORMATION stackinfo;
    stackinfo.StackCommit = 0;
    stackinfo.StackReserve = (SIZE_T)my_thread_stack_size;
    if (!SetThreadpoolStackInformation(pool, &stackinfo))
    {
      tp_log_warning("Can't set threadpool stack size", 
        "SetThreadpoolStackInformation");
    }
  }
#endif

  return 0;
}


/**
  Scheduler callback : Destroy the scheduler.
*/
void tp_end(void)
{
  if(pool)
  {
    SetThreadpoolThreadMaximum(pool, 0);
    CloseThreadpool(pool);
  }
}

/**
  Notify pool about connection being killed.
*/
void tp_post_kill_notification(THD *thd)
{
   if (current_thd == thd)
    return; /* There is nothing to do.*/

  if (thd->system_thread)
   return; /* Will crash if we attempt to kill system thread. */

  Vio *vio= thd->net.vio;

  vio_shutdown(vio, SD_BOTH);

}

/*
  Handle read completion/notification.
*/
static VOID CALLBACK io_completion_callback(PTP_CALLBACK_INSTANCE instance, 
  PVOID context,  PVOID overlapped,  ULONG io_result, ULONG_PTR nbytes, PTP_IO io)
{
  if(instance)
  {
    check_thread_init();
  }

  connection_t *connection = (connection_t*)context;

  if (io_result != ERROR_SUCCESS)
    goto error;

  THD *thd= connection->thd;
  ulonglong old_timeout = connection->timeout;
  connection->timeout = ULONGLONG_MAX;
  connection->callback_instance= instance;
  if (threadpool_process_request(connection->thd))
    goto error;

  set_wait_timeout(connection, old_timeout);
  if(start_io(connection, instance))
    goto error;

  return;

error:
  /* Some error has occured. */

  destroy_connection(connection, instance);
  free(connection);
}


/* Simple callback for login */
static void CALLBACK login_callback(PTP_CALLBACK_INSTANCE instance, 
  PVOID context, PTP_WORK work)
{
  if(instance)
  {
    check_thread_init();
  }

  connection_t *connection =(connection_t *)context;
  if (login(connection, instance) != 0)
  {
    destroy_connection(connection, instance);
    free(connection);
  }
}

/*
  Timer callback.
  Invoked when connection times out (wait_timeout)
*/
static VOID CALLBACK timer_callback(PTP_CALLBACK_INSTANCE instance, 
  PVOID parameter, PTP_TIMER timer)
{
  check_thread_init();

  connection_t *con= (connection_t*)parameter;
  ulonglong timeout= con->timeout;

  if (timeout <= now())
  {
    con->thd->killed = KILL_CONNECTION;
    if(con->thd->net.vio)
      vio_shutdown(con->thd->net.vio, SD_BOTH);
  }
  else if(timeout != ULONGLONG_MAX)
  {
    /* 
      Reset timer. 
      There is a tiny possibility of a race condition, since the value of timeout 
      could have changed to smaller value in the thread doing io callback. 

      Given the relative unimportance of the wait timeout, we accept race 
      condition.
    */
    SetThreadpoolTimer(timer, (PFILETIME)&timeout, 0, 1000);
  }
}


/*
  Shared memory read callback.
  Invoked when read event is set on connection.
*/
static void CALLBACK shm_read_callback(PTP_CALLBACK_INSTANCE instance,
  PVOID context, PTP_WAIT wait,TP_WAIT_RESULT wait_result)
{
  connection_t *con= (connection_t *)context;
  /* Disarm wait. */
  SetThreadpoolWait(wait, NULL, NULL);

  /* 
    This is an autoreset event, and one wakeup is eaten already by threadpool,
    and the current state is "not set". Thus we need to reset the event again, 
    or vio_read will hang.
  */
  HANDLE h = con->thd->net.vio->event_server_wrote;
  SetEvent(h);
  io_completion_callback(instance, context, NULL, 0, 0 , 0);
}


/*
  Notify the thread pool about a new connection.
  NOTE: LOCK_thread_count is locked on entry. This function must unlock it.
*/
void tp_add_connection(THD *thd)
{
  threads.append(thd);
  mysql_mutex_unlock(&LOCK_thread_count);

  connection_t *con = (connection_t *)malloc(sizeof(connection_t));
  if(!con)
  {
    tp_log_warning("Allocation failed", "tp_add_connection");
    threadpool_remove_connection(thd);
    return;
  }

  init_connection(con);
  con->thd= thd;
  thd->event_scheduler.data= con;

  /* Try to login asynchronously, using threads in the pool */
  PTP_WORK wrk =  CreateThreadpoolWork(login_callback,con, &con->callback_environ);
  if (wrk)
  {
    SubmitThreadpoolWork(wrk);
    CloseThreadpoolWork(wrk);
  }
  else
  {
    /* Likely memory pressure */
    login_callback(NULL, con, NULL); /* deletes connection if something goes wrong */
  }
}


/**
  Sets the number of idle threads the thread pool maintains in anticipation of new
  requests.
*/
void tp_set_min_threads(uint val)
{
  if (pool)
    SetThreadpoolThreadMinimum(pool, val);
}

void tp_set_max_threads(uint val)
{
  if (pool)
    SetThreadpoolThreadMaximum(pool, val);
}

void tp_wait_begin(THD *thd, int type)
{
  DBUG_ASSERT(thd);

  /*
    Signal to the threadpool whenever callback can run long. Currently, binlog
    waits are a good candidate, its waits are really long
  */
  if (type == THD_WAIT_BINLOG)
  {
    connection_t *connection= (connection_t *)thd->event_scheduler.data;
    if(connection && connection->callback_instance)
    {
      CallbackMayRunLong(connection->callback_instance);
      /* 
        Reset instance, to avoid calling CallbackMayRunLong  twice within 
        the same callback (it is an error according to docs).
      */
      connection->callback_instance= 0;
    }
  }
}

void tp_wait_end(THD *thd) 
{
  /* Do we need to do anything ? */
}


/**
 Number of idle threads in pool.
 This info is not available in Windows implementation,
 thus function always returns 0.
*/
int tp_get_idle_thread_count()
{
  return 0;
}

