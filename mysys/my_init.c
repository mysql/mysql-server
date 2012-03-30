/* Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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

#include "mysys_priv.h"
#include "my_static.h"
#include "mysys_err.h"
#include <m_string.h>
#include <m_ctype.h>
#include <signal.h>
#ifdef __WIN__
#ifdef _MSC_VER
#include <locale.h>
#include <crtdbg.h>
/* WSAStartup needs winsock library*/
#pragma comment(lib, "ws2_32")
#endif
my_bool have_tcpip=0;
static void my_win_init(void);
static my_bool win32_init_tcp_ip();
#else
#define my_win_init()
#endif

#define SCALE_SEC       100
#define SCALE_USEC      10000

my_bool my_init_done= 0;
uint	mysys_usage_id= 0;              /* Incremented for each my_init() */
ulong   my_thread_stack_size= 65536;

static ulong atoi_octal(const char *str)
{
  long int tmp;
  while (*str && my_isspace(&my_charset_latin1, *str))
    str++;
  str2int(str,
	  (*str == '0' ? 8 : 10),       /* Octalt or decimalt */
	  0, INT_MAX, &tmp);
  return (ulong) tmp;
}

MYSQL_FILE *mysql_stdin= NULL;
static MYSQL_FILE instrumented_stdin;


/**
  Initialize my_sys functions, resources and variables

  @return Initialization result
    @retval 0 Success
    @retval 1 Error. Couldn't initialize environment
*/
my_bool my_init(void)
{
  char *str;

  if (my_init_done)
    return 0;

  my_init_done= 1;

  mysys_usage_id++;
  my_umask= 0660;                       /* Default umask for new files */
  my_umask_dir= 0700;                   /* Default umask for new directories */

  /* Default creation of new files */
  if ((str= getenv("UMASK")) != 0)
    my_umask= (int) (atoi_octal(str) | 0600);
  /* Default creation of new dir's */
  if ((str= getenv("UMASK_DIR")) != 0)
    my_umask_dir= (int) (atoi_octal(str) | 0700);

  init_glob_errs();

  instrumented_stdin.m_file= stdin;
  instrumented_stdin.m_psi= NULL;       /* not yet instrumented */
  mysql_stdin= & instrumented_stdin;

  if (my_thread_global_init())
    return 1;

#if defined(SAFE_MUTEX)
  safe_mutex_global_init();		/* Must be called early */
#endif

#if defined(MY_PTHREAD_FASTMUTEX) && !defined(SAFE_MUTEX)
  fastmutex_global_init();              /* Must be called early */
#endif

  /* $HOME is needed early to parse configuration files located in ~/ */
  if ((home_dir= getenv("HOME")) != 0)
    home_dir= intern_filename(home_dir_buff, home_dir);

  {
    DBUG_ENTER("my_init");
    DBUG_PROCESS((char*) (my_progname ? my_progname : "unknown"));
    my_win_init();
    DBUG_PRINT("exit", ("home: '%s'", home_dir));
#ifdef __WIN__
    win32_init_tcp_ip();
#endif
    DBUG_RETURN(0);
  }
} /* my_init */


	/* End my_sys */

void my_end(int infoflag)
{
  /*
    this code is suboptimal to workaround a bug in
    Sun CC: Sun C++ 5.6 2004/06/02 for x86, and should not be
    optimized until this compiler is not in use anymore
  */
  FILE *info_file= DBUG_FILE;
  my_bool print_info= (info_file != stderr);

  if (!my_init_done)
    return;

  /*
    We do not use DBUG_ENTER here, as after cleanup DBUG is no longer
    operational, so we cannot use DBUG_RETURN.
  */
  DBUG_PRINT("info",("Shutting down: infoflag: %d  print_info: %d",
                     infoflag, print_info));
  if (!info_file)
  {
    info_file= stderr;
    print_info= 0;
  }

  if ((infoflag & MY_CHECK_ERROR) || print_info)

  {					/* Test if some file is left open */
    if (my_file_opened | my_stream_opened)
    {
      char ebuff[512];
      my_snprintf(ebuff, sizeof(ebuff), EE(EE_OPEN_WARNING),
                  my_file_opened, my_stream_opened);
      my_message_stderr(EE_OPEN_WARNING, ebuff, ME_BELL);
      DBUG_PRINT("error", ("%s", ebuff));
      my_print_open_files();
    }
  }
  free_charsets();
  my_error_unregister_all();
  my_once_free();

  if ((infoflag & MY_GIVE_INFO) || print_info)
  {
#ifdef HAVE_GETRUSAGE
    struct rusage rus;
#ifdef HAVE_purify
    /* Purify assumes that rus is uninitialized after getrusage call */
    bzero((char*) &rus, sizeof(rus));
#endif
    if (!getrusage(RUSAGE_SELF, &rus))
      fprintf(info_file,"\n\
User time %.2f, System time %.2f\n\
Maximum resident set size %ld, Integral resident set size %ld\n\
Non-physical pagefaults %ld, Physical pagefaults %ld, Swaps %ld\n\
Blocks in %ld out %ld, Messages in %ld out %ld, Signals %ld\n\
Voluntary context switches %ld, Involuntary context switches %ld\n",
	      (rus.ru_utime.tv_sec * SCALE_SEC +
	       rus.ru_utime.tv_usec / SCALE_USEC) / 100.0,
	      (rus.ru_stime.tv_sec * SCALE_SEC +
	       rus.ru_stime.tv_usec / SCALE_USEC) / 100.0,
	      rus.ru_maxrss, rus.ru_idrss,
	      rus.ru_minflt, rus.ru_majflt,
	      rus.ru_nswap, rus.ru_inblock, rus.ru_oublock,
	      rus.ru_msgsnd, rus.ru_msgrcv, rus.ru_nsignals,
	      rus.ru_nvcsw, rus.ru_nivcsw);
#endif
#if defined(__WIN__) && defined(_MSC_VER)
   _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE );
   _CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDERR );
   _CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
   _CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDERR );
   _CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
   _CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDERR );
   _CrtCheckMemory();
   _CrtDumpMemoryLeaks();
#endif
  }

  if (!(infoflag & MY_DONT_FREE_DBUG))
  {
    DBUG_END();                /* Must be done before my_thread_end */
  }

  my_thread_end();
  my_thread_global_end();
#if defined(SAFE_MUTEX)
  /*
    Check on destroying of mutexes. A few may be left that will get cleaned
    up by C++ destructors
  */
  safe_mutex_end((infoflag & (MY_GIVE_INFO | MY_CHECK_ERROR)) ? stderr :
                 (FILE *) 0);
#endif /* defined(SAFE_MUTEX) */

#ifdef __WIN__
  if (have_tcpip)
    WSACleanup();
#endif /* __WIN__ */

  my_init_done=0;
} /* my_end */


#ifdef __WIN__


/*
  my_parameter_handler
  
  Invalid parameter handler we will use instead of the one "baked"
  into the CRT for MSC v8.  This one just prints out what invalid
  parameter was encountered.  By providing this routine, routines like
  lseek will return -1 when we expect them to instead of crash.
*/

void my_parameter_handler(const wchar_t * expression, const wchar_t * function,
                          const wchar_t * file, unsigned int line,
                          uintptr_t pReserved)
{
  DBUG_PRINT("my",("Expression: %s  function: %s  file: %s, line: %d",
		   expression, function, file, line));
}


#ifdef __MSVC_RUNTIME_CHECKS
#include <rtcapi.h>

/* Turn off runtime checks for 'handle_rtc_failure' */
#pragma runtime_checks("", off)

/*
  handle_rtc_failure
  Catch the RTC error and dump it to stderr
*/

int handle_rtc_failure(int err_type, const char *file, int line,
                       const char* module, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  fprintf(stderr, "Error:");
  vfprintf(stderr, format, args);
  fprintf(stderr, " At %s:%d\n", file, line);
  va_end(args);
  (void) fflush(stderr);

  return 0; /* Error is handled */
}
#pragma runtime_checks("", restore)
#endif

#define OFFSET_TO_EPOC ((__int64) 134774 * 24 * 60 * 60 * 1000 * 1000 * 10)
#define MS 10000000

static void win_init_time(void)
{
  /* The following is used by time functions */
  FILETIME ft;
  LARGE_INTEGER li, t_cnt;

  DBUG_ASSERT(sizeof(LARGE_INTEGER) == sizeof(query_performance_frequency));

  if (QueryPerformanceFrequency((LARGE_INTEGER *)&query_performance_frequency) == 0)
    query_performance_frequency= 0;
  else
  {
    GetSystemTimeAsFileTime(&ft);
    li.LowPart=  ft.dwLowDateTime;
    li.HighPart= ft.dwHighDateTime;
    query_performance_offset= li.QuadPart-OFFSET_TO_EPOC;
    QueryPerformanceCounter(&t_cnt);
    query_performance_offset-= (t_cnt.QuadPart /
                                query_performance_frequency * MS +
                                t_cnt.QuadPart %
                                query_performance_frequency * MS /
                                query_performance_frequency);
  }
}


/*
  Open HKEY_LOCAL_MACHINE\SOFTWARE\MySQL and set any strings found
  there as environment variables
*/
static void win_init_registry(void)
{
  HKEY key_handle;

  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, (LPCTSTR)"SOFTWARE\\MySQL",
                    0, KEY_READ, &key_handle) == ERROR_SUCCESS)
  {
    LONG ret;
    DWORD index= 0;
    DWORD type;
    char key_name[256], key_data[1024];
    DWORD key_name_len= sizeof(key_name) - 1;
    DWORD key_data_len= sizeof(key_data) - 1;

    while ((ret= RegEnumValue(key_handle, index++,
                              key_name, &key_name_len,
                              NULL, &type, (LPBYTE)&key_data,
                              &key_data_len)) != ERROR_NO_MORE_ITEMS)
    {
      char env_string[sizeof(key_name) + sizeof(key_data) + 2];

      if (ret == ERROR_MORE_DATA)
      {
        /* Registry value larger than 'key_data', skip it */
        DBUG_PRINT("error", ("Skipped registry value that was too large"));
      }
      else if (ret == ERROR_SUCCESS)
      {
        if (type == REG_SZ)
        {
          strxmov(env_string, key_name, "=", key_data, NullS);

          /* variable for putenv must be allocated ! */
          putenv(strdup(env_string)) ;
        }
      }
      else
      {
        /* Unhandled error, break out of loop */
        break;
      }

      key_name_len= sizeof(key_name) - 1;
      key_data_len= sizeof(key_data) - 1;
    }

    RegCloseKey(key_handle);
  }
}


static void my_win_init(void)
{
  DBUG_ENTER("my_win_init");

#if defined(_MSC_VER)
#if _MSC_VER < 1300
  /*
    Clear the OS system variable TZ and avoid the 100% CPU usage
    Only for old versions of Visual C++
  */
  _putenv("TZ=");
#endif
#if _MSC_VER >= 1400
  /* this is required to make crt functions return -1 appropriately */
  _set_invalid_parameter_handler(my_parameter_handler);
#endif
#endif

#ifdef __MSVC_RUNTIME_CHECKS
  /*
    Install handler to send RTC (Runtime Error Check) warnings
    to log file
  */
  _RTC_SetErrorFunc(handle_rtc_failure);
#endif

  _tzset();

  win_init_time();
  win_init_registry();

  DBUG_VOID_RETURN;
}


/*------------------------------------------------------------------
  Name: CheckForTcpip| Desc: checks if tcpip has been installed on system
  According to Microsoft Developers documentation the first registry
  entry should be enough to check if TCP/IP is installed, but as expected
  this doesn't work on all Win32 machines :(
------------------------------------------------------------------*/

#define TCPIPKEY  "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters"
#define WINSOCK2KEY "SYSTEM\\CurrentControlSet\\Services\\Winsock2\\Parameters"
#define WINSOCKKEY  "SYSTEM\\CurrentControlSet\\Services\\Winsock\\Parameters"

static my_bool win32_have_tcpip(void)
{
  HKEY hTcpipRegKey;
  if (RegOpenKeyEx ( HKEY_LOCAL_MACHINE, TCPIPKEY, 0, KEY_READ,
		      &hTcpipRegKey) != ERROR_SUCCESS)
  {
    if (RegOpenKeyEx ( HKEY_LOCAL_MACHINE, WINSOCK2KEY, 0, KEY_READ,
		      &hTcpipRegKey) != ERROR_SUCCESS)
    {
      if (RegOpenKeyEx ( HKEY_LOCAL_MACHINE, WINSOCKKEY, 0, KEY_READ,
			 &hTcpipRegKey) != ERROR_SUCCESS)
	if (!getenv("HAVE_TCPIP") || have_tcpip)	/* Provide a workaround */
	  return (FALSE);
    }
  }
  RegCloseKey ( hTcpipRegKey);
  return (TRUE);
}


static my_bool win32_init_tcp_ip()
{
  if (win32_have_tcpip())
  {
    WORD wVersionRequested = MAKEWORD( 2, 2 );
    WSADATA wsaData;
 	/* Be a good citizen: maybe another lib has already initialised
 		sockets, so dont clobber them unless necessary */
    if (WSAStartup( wVersionRequested, &wsaData ))
    {
      /* Load failed, maybe because of previously loaded
	 incompatible version; try again */
      WSACleanup( );
      if (!WSAStartup( wVersionRequested, &wsaData ))
	have_tcpip=1;
    }
    else
    {
      if (wsaData.wVersion != wVersionRequested)
      {
	/* Version is no good, try again */
	WSACleanup( );
	if (!WSAStartup( wVersionRequested, &wsaData ))
	  have_tcpip=1;
      }
      else
	have_tcpip=1;
    }
  }
  return(0);
}
#endif /* __WIN__ */

#ifdef HAVE_PSI_INTERFACE

#if !defined(HAVE_PREAD) && !defined(_WIN32)
PSI_mutex_key key_my_file_info_mutex;
#endif /* !defined(HAVE_PREAD) && !defined(_WIN32) */

#if !defined(HAVE_LOCALTIME_R) || !defined(HAVE_GMTIME_R)
PSI_mutex_key key_LOCK_localtime_r;
#endif /* !defined(HAVE_LOCALTIME_R) || !defined(HAVE_GMTIME_R) */

PSI_mutex_key key_BITMAP_mutex, key_IO_CACHE_append_buffer_lock,
  key_IO_CACHE_SHARE_mutex, key_KEY_CACHE_cache_lock, key_LOCK_alarm,
  key_my_thread_var_mutex, key_THR_LOCK_charset, key_THR_LOCK_heap,
  key_THR_LOCK_isam, key_THR_LOCK_lock, key_THR_LOCK_malloc,
  key_THR_LOCK_mutex, key_THR_LOCK_myisam, key_THR_LOCK_net,
  key_THR_LOCK_open, key_THR_LOCK_threads,
  key_TMPDIR_mutex, key_THR_LOCK_myisam_mmap;

static PSI_mutex_info all_mysys_mutexes[]=
{
#if !defined(HAVE_PREAD) && !defined(_WIN32)
  { &key_my_file_info_mutex, "st_my_file_info:mutex", 0},
#endif /* !defined(HAVE_PREAD) && !defined(_WIN32) */
#if !defined(HAVE_LOCALTIME_R) || !defined(HAVE_GMTIME_R)
  { &key_LOCK_localtime_r, "LOCK_localtime_r", PSI_FLAG_GLOBAL},
#endif /* !defined(HAVE_LOCALTIME_R) || !defined(HAVE_GMTIME_R) */
  { &key_BITMAP_mutex, "BITMAP::mutex", 0},
  { &key_IO_CACHE_append_buffer_lock, "IO_CACHE::append_buffer_lock", 0},
  { &key_IO_CACHE_SHARE_mutex, "IO_CACHE::SHARE_mutex", 0},
  { &key_KEY_CACHE_cache_lock, "KEY_CACHE::cache_lock", 0},
  { &key_LOCK_alarm, "LOCK_alarm", PSI_FLAG_GLOBAL},
  { &key_my_thread_var_mutex, "my_thread_var::mutex", 0},
  { &key_THR_LOCK_charset, "THR_LOCK_charset", PSI_FLAG_GLOBAL},
  { &key_THR_LOCK_heap, "THR_LOCK_heap", PSI_FLAG_GLOBAL},
  { &key_THR_LOCK_isam, "THR_LOCK_isam", PSI_FLAG_GLOBAL},
  { &key_THR_LOCK_lock, "THR_LOCK_lock", PSI_FLAG_GLOBAL},
  { &key_THR_LOCK_malloc, "THR_LOCK_malloc", PSI_FLAG_GLOBAL},
  { &key_THR_LOCK_mutex, "THR_LOCK::mutex", 0},
  { &key_THR_LOCK_myisam, "THR_LOCK_myisam", PSI_FLAG_GLOBAL},
  { &key_THR_LOCK_net, "THR_LOCK_net", PSI_FLAG_GLOBAL},
  { &key_THR_LOCK_open, "THR_LOCK_open", PSI_FLAG_GLOBAL},
  { &key_THR_LOCK_threads, "THR_LOCK_threads", PSI_FLAG_GLOBAL},
  { &key_TMPDIR_mutex, "TMPDIR_mutex", PSI_FLAG_GLOBAL},
  { &key_THR_LOCK_myisam_mmap, "THR_LOCK_myisam_mmap", PSI_FLAG_GLOBAL}
};

PSI_cond_key key_COND_alarm, key_IO_CACHE_SHARE_cond,
  key_IO_CACHE_SHARE_cond_writer, key_my_thread_var_suspend,
  key_THR_COND_threads;

static PSI_cond_info all_mysys_conds[]=
{
  { &key_COND_alarm, "COND_alarm", PSI_FLAG_GLOBAL},
  { &key_IO_CACHE_SHARE_cond, "IO_CACHE_SHARE::cond", 0},
  { &key_IO_CACHE_SHARE_cond_writer, "IO_CACHE_SHARE::cond_writer", 0},
  { &key_my_thread_var_suspend, "my_thread_var::suspend", 0},
  { &key_THR_COND_threads, "THR_COND_threads", 0}
};

#ifdef USE_ALARM_THREAD
PSI_thread_key key_thread_alarm;

static PSI_thread_info all_mysys_threads[]=
{
  { &key_thread_alarm, "alarm", PSI_FLAG_GLOBAL}
};
#endif /* USE_ALARM_THREAD */

#ifdef HUGETLB_USE_PROC_MEMINFO
PSI_file_key key_file_proc_meminfo;
#endif /* HUGETLB_USE_PROC_MEMINFO */
PSI_file_key key_file_charset, key_file_cnf;

static PSI_file_info all_mysys_files[]=
{
#ifdef HUGETLB_USE_PROC_MEMINFO
  { &key_file_proc_meminfo, "proc_meminfo", 0},
#endif /* HUGETLB_USE_PROC_MEMINFO */
  { &key_file_charset, "charset", 0},
  { &key_file_cnf, "cnf", 0}
};

void my_init_mysys_psi_keys()
{
  const char* category= "mysys";
  int count;

  if (PSI_server == NULL)
    return;

  count= sizeof(all_mysys_mutexes)/sizeof(all_mysys_mutexes[0]);
  PSI_server->register_mutex(category, all_mysys_mutexes, count);

  count= sizeof(all_mysys_conds)/sizeof(all_mysys_conds[0]);
  PSI_server->register_cond(category, all_mysys_conds, count);

#ifdef USE_ALARM_THREAD
  count= sizeof(all_mysys_threads)/sizeof(all_mysys_threads[0]);
  PSI_server->register_thread(category, all_mysys_threads, count);
#endif /* USE_ALARM_THREAD */

  count= sizeof(all_mysys_files)/sizeof(all_mysys_files[0]);
  PSI_server->register_file(category, all_mysys_files, count);
}
#endif /* HAVE_PSI_INTERFACE */

