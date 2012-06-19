/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

#include "my_global.h"
#include <signal.h>

#include "sys_vars.h"
#include "my_stacktrace.h"

#ifdef __WIN__
#include <crtdbg.h>
#define SIGNAL_FMT "exception 0x%x"
#else
#define SIGNAL_FMT "signal %d"
#endif

/*
  We are handling signals in this file.
  Any global variables we read should be 'volatile sig_atomic_t'
  to guarantee that we read some consistent value.
 */
static volatile sig_atomic_t segfaulted= 0;
extern ulong max_used_connections;
extern volatile sig_atomic_t calling_initgroups;
#ifdef HAVE_NPTL
extern volatile sig_atomic_t ld_assume_kernel_is_set;
#endif

/**
 * Handler for fatal signals
 *
 * Fatal events (seg.fault, bus error etc.) will trigger
 * this signal handler.  The handler will try to dump relevant
 * debugging information to stderr and dump a core image.
 *
 * Signal handlers can only use a set of 'safe' system calls
 * and library functions.  A list of safe calls in POSIX systems
 * are available at:
 *  http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_04.html
 * For MS Windows, guidelines are available at:
 *  http://msdn.microsoft.com/en-us/library/xdkz3x12(v=vs.71).aspx
 *
 * @param sig Signal number
*/
extern "C" sig_handler handle_fatal_signal(int sig)
{
  if (segfaulted)
  {
    my_safe_printf_stderr("Fatal " SIGNAL_FMT " while backtracing\n", sig);
    _exit(1); /* Quit without running destructors */
  }

  segfaulted = 1;

#ifdef __WIN__
  SYSTEMTIME utc_time;
  GetSystemTime(&utc_time);
  const long hrs=  utc_time.wHour;
  const long mins= utc_time.wMinute;
  const long secs= utc_time.wSecond;
#else
  /* Using time() instead of my_time() to avoid looping */
  const time_t curr_time= time(NULL);
  /* Calculate time of day */
  const long tmins = curr_time / 60;
  const long thrs  = tmins / 60;
  const long hrs   = thrs  % 24;
  const long mins  = tmins % 60;
  const long secs  = curr_time % 60;
#endif

  char hrs_buf[3]= "00";
  char mins_buf[3]= "00";
  char secs_buf[3]= "00";
  my_safe_itoa(10, hrs, &hrs_buf[2]);
  my_safe_itoa(10, mins, &mins_buf[2]);
  my_safe_itoa(10, secs, &secs_buf[2]);

  my_safe_printf_stderr("%s:%s:%s UTC - mysqld got " SIGNAL_FMT " ;\n",
                        hrs_buf, mins_buf, secs_buf, sig);

  my_safe_printf_stderr("%s",
    "This could be because you hit a bug. It is also possible that this binary\n"
    "or one of the libraries it was linked against is corrupt, improperly built,\n"
    "or misconfigured. This error can also be caused by malfunctioning hardware.\n");

  my_safe_printf_stderr("%s",
    "We will try our best to scrape up some info that will hopefully help\n"
    "diagnose the problem, but since we have already crashed, \n"
    "something is definitely wrong and this may fail.\n\n");

  my_safe_printf_stderr("key_buffer_size=%lu\n",
                        (ulong) dflt_key_cache->key_cache_mem_size);

  my_safe_printf_stderr("read_buffer_size=%ld\n",
                        (long) global_system_variables.read_buff_size);

  my_safe_printf_stderr("max_used_connections=%lu\n",
                        (ulong) max_used_connections);

  my_safe_printf_stderr("max_threads=%u\n",
                        (uint) thread_scheduler->max_threads);

  my_safe_printf_stderr("thread_count=%u\n", (uint) thread_count);

  my_safe_printf_stderr("connection_count=%u\n", (uint) connection_count);

  my_safe_printf_stderr("It is possible that mysqld could use up to \n"
                        "key_buffer_size + "
                        "(read_buffer_size + sort_buffer_size)*max_threads = "
                        "%lu K  bytes of memory\n",
                        ((ulong) dflt_key_cache->key_cache_mem_size +
                         (global_system_variables.read_buff_size +
                          global_system_variables.sortbuff_size) *
                         thread_scheduler->max_threads +
                         max_connections * sizeof(THD)) / 1024);

  my_safe_printf_stderr("%s",
    "Hope that's ok; if not, decrease some variables in the equation.\n\n");

#if defined(HAVE_LINUXTHREADS)
#define UNSAFE_DEFAULT_LINUX_THREADS 200
  if (sizeof(char*) == 4 && thread_count > UNSAFE_DEFAULT_LINUX_THREADS)
  {
    my_safe_printf_stderr(
      "You seem to be running 32-bit Linux and have "
      "%d concurrent connections.\n"
      "If you have not changed STACK_SIZE in LinuxThreads "
      "and built the binary \n"
      "yourself, LinuxThreads is quite likely to steal "
      "a part of the global heap for\n"
      "the thread stack. Please read "
      "http://dev.mysql.com/doc/mysql/en/linux-installation.html\n\n"
      thread_count);
  }
#endif /* HAVE_LINUXTHREADS */

#ifdef HAVE_STACKTRACE
  THD *thd=current_thd;

  if (!(test_flags & TEST_NO_STACKTRACE))
  {
    my_safe_printf_stderr("Thread pointer: 0x%p\n", thd);
    my_safe_printf_stderr("%s",
      "Attempting backtrace. You can use the following "
      "information to find out\n"
      "where mysqld died. If you see no messages after this, something went\n"
      "terribly wrong...\n");
    my_print_stacktrace(thd ? (uchar*) thd->thread_stack : NULL,
                        my_thread_stack_size);
  }
  if (thd)
  {
    const char *kreason= "UNKNOWN";
    switch (thd->killed) {
    case THD::NOT_KILLED:
      kreason= "NOT_KILLED";
      break;
    case THD::KILL_BAD_DATA:
      kreason= "KILL_BAD_DATA";
      break;
    case THD::KILL_CONNECTION:
      kreason= "KILL_CONNECTION";
      break;
    case THD::KILL_QUERY:
      kreason= "KILL_QUERY";
      break;
    case THD::KILLED_NO_VALUE:
      kreason= "KILLED_NO_VALUE";
      break;
    }
    my_safe_printf_stderr("%s", "\n"
      "Trying to get some variables.\n"
      "Some pointers may be invalid and cause the dump to abort.\n");

    my_safe_printf_stderr("Query (%p): ", thd->query());
    my_safe_print_str(thd->query(), min(1024U, thd->query_length()));
    my_safe_printf_stderr("Connection ID (thread ID): %lu\n",
                          (ulong) thd->thread_id);
    my_safe_printf_stderr("Status: %s\n\n", kreason);
  }
  my_safe_printf_stderr("%s",
    "The manual page at "
    "http://dev.mysql.com/doc/mysql/en/crashing.html contains\n"
    "information that should help you find out what is causing the crash.\n");

#endif /* HAVE_STACKTRACE */

#ifdef HAVE_INITGROUPS
  if (calling_initgroups)
  {
    my_safe_printf_stderr("%s", "\n"
      "This crash occured while the server was calling initgroups(). This is\n"
      "often due to the use of a mysqld that is statically linked against \n"
      "glibc and configured to use LDAP in /etc/nsswitch.conf.\n"
      "You will need to either upgrade to a version of glibc that does not\n"
      "have this problem (2.3.4 or later when used with nscd),\n"
      "disable LDAP in your nsswitch.conf, or use a "
      "mysqld that is not statically linked.\n");
  }
#endif

#ifdef HAVE_NPTL
  if (thd_lib_detected == THD_LIB_LT && !ld_assume_kernel_is_set)
  {
    my_safe_printf_stderr("%s",
      "You are running a statically-linked LinuxThreads binary on an NPTL\n"
      "system. This can result in crashes on some distributions due to "
      "LT/NPTL conflicts.\n"
      "You should either build a dynamically-linked binary, "
      "or force LinuxThreads\n"
      "to be used with the LD_ASSUME_KERNEL environment variable.\n"
      "Please consult the documentation for your distribution "
      "on how to do that.\n");
  }
#endif

  if (locked_in_memory)
  {
    my_safe_printf_stderr("%s", "\n"
      "The \"--memlock\" argument, which was enabled, "
      "uses system calls that are\n"
      "unreliable and unstable on some operating systems and "
      "operating-system versions (notably, some versions of Linux).\n"
      "This crash could be due to use of those buggy OS calls.\n"
      "You should consider whether you really need the "
      "\"--memlock\" parameter and/or consult the OS distributer about "
      "\"mlockall\" bugs.\n");
  }

#ifdef HAVE_WRITE_CORE
  if (test_flags & TEST_CORE_ON_SIGNAL)
  {
    my_safe_printf_stderr("%s", "Writing a core file\n");
    my_write_core(sig);
  }
#endif

#ifndef __WIN__
  /*
     Quit, without running destructors (etc.)
     On Windows, do not terminate, but pass control to exception filter.
  */
  _exit(1);  // Using _exit(), since exit() is not async signal safe
#endif
}
