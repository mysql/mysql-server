/* Copyright (c) 2011, Oracle and/or its affiliates.
   Copyright (c) 2011, Monty Program Ab.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "my_global.h"
#include <signal.h>

//#include "sys_vars.h"
#include <keycache.h>
#include "mysqld.h"
#include "sql_class.h"
#include "my_stacktrace.h"

#ifdef __WIN__
#include <crtdbg.h>
#define SIGNAL_FMT "exception 0x%x"
#else
#define SIGNAL_FMT "signal %d"
#endif

/*
  We are handling signals/exceptions in this file.
  Any global variables we read should be 'volatile sig_atomic_t'
  to guarantee that we read some consistent value.
 */
static volatile sig_atomic_t segfaulted= 0;
extern ulong max_used_connections;
extern volatile sig_atomic_t calling_initgroups;
#ifdef HAVE_NPTL
extern volatile sig_atomic_t ld_assume_kernel_is_set;
#endif

extern const char *optimizer_switch_names[];

/**
 * Handler for fatal signals on POSIX, exception handler on Windows.
 *
 * Fatal events (seg.fault, bus error etc.) will trigger
 * this signal handler.  The handler will try to dump relevant
 * debugging information to stderr and dump a core image.
 *
 * POSIX : Signal handlers should, if possible, only use a set of 'safe' system 
 * calls and library functions.  A list of safe calls in POSIX systems
 * are available at:
 *  http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_04.html
 *
 * @param sig Signal number /Exception code
*/
extern "C" sig_handler handle_fatal_signal(int sig)
{
  time_t curr_time;
  struct tm tm;
#ifdef HAVE_STACKTRACE
  THD *thd;
#endif

  if (segfaulted)
  {
    my_safe_printf_stderr("Fatal " SIGNAL_FMT " while backtracing\n", sig);
    _exit(1); /* Quit without running destructors */
  }

  segfaulted = 1;

  curr_time= my_time(0);
  localtime_r(&curr_time, &tm);

  my_safe_printf_stderr("%02d%02d%02d %2d:%02d:%02d ",
                        tm.tm_year % 100, tm.tm_mon+1, tm.tm_mday,
                        tm.tm_hour, tm.tm_min, tm.tm_sec);
  if (opt_expect_abort
#ifdef _WIN32
    && sig == EXCEPTION_BREAKPOINT /* __debugbreak in my_sigabrt_hander() */
#else
    && sig == SIGABRT
#endif
    )
  {
    fprintf(stderr,"[Note] mysqld did an expected abort\n");
    goto end;
  }

  my_safe_printf_stderr("[ERROR] mysqld got " SIGNAL_FMT " ;\n",sig);

  my_safe_printf_stderr("%s",
    "This could be because you hit a bug. It is also possible that this binary\n"
    "or one of the libraries it was linked against is corrupt, improperly built,\n"
    "or misconfigured. This error can also be caused by malfunctioning hardware.\n\n");

  my_safe_printf_stderr("%s",
                        "To report this bug, see http://kb.askmonty.org/en/reporting-bugs\n\n");

  my_safe_printf_stderr("%s",
    "We will try our best to scrape up some info that will hopefully help\n"
    "diagnose the problem, but since we have already crashed, \n"
    "something is definitely wrong and this may fail.\n\n");

  set_server_version();
  my_safe_printf_stderr("Server version: %s\n", server_version);

  my_safe_printf_stderr("key_buffer_size=%lu\n",
                        (ulong) dflt_key_cache->key_cache_mem_size);

  my_safe_printf_stderr("read_buffer_size=%ld\n",
                        (long) global_system_variables.read_buff_size);

  my_safe_printf_stderr("max_used_connections=%lu\n",
                        (ulong) max_used_connections);

  my_safe_printf_stderr("max_threads=%u\n",
                        (uint) thread_scheduler->max_threads +
                        (uint) extra_max_connections);

  my_safe_printf_stderr("thread_count=%u\n", (uint) thread_count);

  my_safe_printf_stderr("It is possible that mysqld could use up to \n"
                        "key_buffer_size + "
                        "(read_buffer_size + sort_buffer_size)*max_threads = "
                        "%lu K  bytes of memory\n",
                        (ulong)(dflt_key_cache->key_cache_mem_size +
                         (global_system_variables.read_buff_size +
                          global_system_variables.sortbuff_size) *
                         (thread_scheduler->max_threads + extra_max_connections) +
                         (max_connections + extra_max_connections)* sizeof(THD)) / 1024);

  my_safe_printf_stderr("%s",
    "Hope that's ok; if not, decrease some variables in the equation.\n\n");

#ifdef HAVE_STACKTRACE
  thd= current_thd;

  if (opt_stack_trace)
  {
    my_safe_printf_stderr("Thread pointer: 0x%p\n", thd);
    my_safe_printf_stderr("%s",
      "Attempting backtrace. You can use the following "
      "information to find out\n"
      "where mysqld died. If you see no messages after this, something went\n"
      "terribly wrong...\n");
    my_print_stacktrace(thd ? (uchar*) thd->thread_stack : NULL,
                        (ulong)my_thread_stack_size);
  }
  if (thd)
  {
    const char *kreason= "UNKNOWN";
    switch (thd->killed) {
    case NOT_KILLED:
    case KILL_HARD_BIT:
      kreason= "NOT_KILLED";
      break;
    case KILL_BAD_DATA:
    case KILL_BAD_DATA_HARD:
      kreason= "KILL_BAD_DATA";
      break;
    case KILL_CONNECTION:
    case KILL_CONNECTION_HARD:
      kreason= "KILL_CONNECTION";
      break;
    case KILL_QUERY:
    case KILL_QUERY_HARD:
      kreason= "KILL_QUERY";
      break;
    case KILL_SYSTEM_THREAD:
    case KILL_SYSTEM_THREAD_HARD:
      kreason= "KILL_SYSTEM_THREAD";
      break;
    case KILL_SERVER:
    case KILL_SERVER_HARD:
      kreason= "KILL_SERVER";
      break;
    case ABORT_QUERY:
    case ABORT_QUERY_HARD:
      kreason= "ABORT_QUERY";
      break;
    }
    my_safe_printf_stderr("%s", "\n"
      "Trying to get some variables.\n"
      "Some pointers may be invalid and cause the dump to abort.\n");

    my_safe_printf_stderr("Query (%p): ", thd->query());
    my_safe_print_str(thd->query(), min(65536U, thd->query_length()));
    my_safe_printf_stderr("\nConnection ID (thread ID): %lu\n",
                          (ulong) thd->thread_id);
    my_safe_printf_stderr("Status: %s\n\n", kreason);
    my_safe_printf_stderr("%s", "Optimizer switch: ");
    ulonglong optsw= thd->variables.optimizer_switch;
    for (uint i= 0; optimizer_switch_names[i+1]; i++, optsw >>= 1)
    {
      if (i)
        my_safe_printf_stderr("%s", ",");
      my_safe_printf_stderr("%s=%s",
              optimizer_switch_names[i], optsw & 1 ? "on" : "off");
    }
    my_safe_printf_stderr("%s", "\n\n");
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
    fflush(stderr);
    my_write_core(sig);
  }
#endif

end:
#ifndef __WIN__
  /*
     Quit, without running destructors (etc.)
     On Windows, do not terminate, but pass control to exception filter.
  */
  _exit(1);  // Using _exit(), since exit() is not async signal safe
#else
  return;
#endif
}
