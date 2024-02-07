/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/my_thread.cc
*/

#include "my_config.h"

#ifdef HAVE_PTHREAD_SETNAME_NP_LINUX
#include <cstring>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>
#endif /* HAVE_PTHREAD_SETNAME_NP_LINUX */

#ifdef HAVE_PTHREAD_SETNAME_NP_MACOS
#include <pthread.h>
#endif /* HAVE_PTHREAD_SETNAME_NP_MACOS */

#ifdef _WIN32
#include <windows.h>

#include <processthreadsapi.h>

#include <stringapiset.h>
#endif /* _WIN32 */
#include "my_thread.h"
#include "mysql/components/services/bits/my_thread_bits.h"

#ifdef _WIN32
#include <errno.h>
#include <process.h>
#include <signal.h>
#include "my_sys.h" /* my_osmaperr */

struct thread_start_parameter {
  my_start_routine func;
  void *arg;
};

static unsigned int __stdcall win_thread_start(void *p) {
  struct thread_start_parameter *par = (struct thread_start_parameter *)p;
  my_start_routine func = par->func;
  void *arg = par->arg;
  free(p);
  (*func)(arg);
  return 0;
}
#endif

int my_thread_create(my_thread_handle *thread, const my_thread_attr_t *attr,
                     my_start_routine func, void *arg) {
#ifndef _WIN32
  return pthread_create(&thread->thread, attr, func, arg);
#else
  struct thread_start_parameter *par;
  unsigned int stack_size;

  par = (struct thread_start_parameter *)malloc(sizeof(*par));
  if (!par) goto error_return;

  par->func = func;
  par->arg = arg;
  stack_size = attr ? attr->dwStackSize : 0;

  thread->handle =
      (HANDLE)_beginthreadex(nullptr, stack_size, win_thread_start, par, 0,
                             (unsigned int *)&thread->thread);

  if (thread->handle) {
    /* Note that JOINABLE is default, so attr == nullptr => JOINABLE. */
    if (attr && attr->detachstate == MY_THREAD_CREATE_DETACHED) {
      /*
        Close handles for detached threads right away to avoid leaking
        handles. For joinable threads we need the handle during
        my_thread_join. It will be closed there.
      */
      CloseHandle(thread->handle);
      thread->handle = nullptr;
    }
    return 0;
  }

  my_osmaperr(GetLastError());
  free(par);

error_return:
  thread->thread = 0;
  thread->handle = nullptr;
  return 1;
#endif
}

int my_thread_join(my_thread_handle *thread, void **value_ptr) {
#ifndef _WIN32
  return pthread_join(thread->thread, value_ptr);
#else
  (void)value_ptr;  // maybe unused

  DWORD ret;
  int result = 0;
  ret = WaitForSingleObject(thread->handle, INFINITE);
  if (ret != WAIT_OBJECT_0) {
    my_osmaperr(GetLastError());
    result = 1;
  }
  if (thread->handle) CloseHandle(thread->handle);
  thread->thread = 0;
  thread->handle = nullptr;
  return result;
#endif
}

int my_thread_cancel(my_thread_handle *thread) {
#ifndef _WIN32
  return pthread_cancel(thread->thread);
#else
  bool ok = false;

  if (thread->handle) {
    ok = TerminateThread(thread->handle, 0);
    CloseHandle(thread->handle);
  }
  if (ok) return 0;

  errno = EINVAL;
  return -1;
#endif
}

// _endthreadex(_ReturnCode) is not tagged with noreturn.
#ifdef _WIN32
MY_COMPILER_DIAGNOSTIC_PUSH()
MY_COMPILER_CLANG_DIAGNOSTIC_IGNORE("-Winvalid-noreturn")
#endif
void my_thread_exit(void *value_ptr [[maybe_unused]]) {
#ifndef _WIN32
  pthread_exit(value_ptr);
#else
  _endthreadex(0);
#endif
}
#ifdef _WIN32
MY_COMPILER_DIAGNOSTIC_POP()
#endif

/**
  Maximum name length used for my_thread_self_setname(),
  including the terminating NUL character.
  Linux pthread_setname_np(3) is restricted to 15+1 chars,
  so we use the same limit on all platforms.
*/
#define SETNAME_MAX_LENGTH 16

#ifdef _WIN32
template <class TMethod>
class Win32_library_procedure {
 public:
  Win32_library_procedure(std::string module, std::string func_name)
      : m_module(LoadLibrary(module.c_str())), m_func(nullptr) {
    if (m_module != nullptr) {
      m_func = reinterpret_cast<TMethod *>(
          GetProcAddress(m_module, func_name.c_str()));
    }
  }
  ~Win32_library_procedure() {
    if (m_module != nullptr) {
      FreeLibrary(m_module);
    }
  }
  bool is_valid() { return m_func != nullptr; }
  template <typename... TArgs>
  auto operator()(TArgs... args) {
    return m_func(std::forward<TArgs>(args)...);
  }

 private:
  HMODULE m_module;
  TMethod *m_func;
};

static Win32_library_procedure<decltype(SetThreadDescription)>
    set_thread_name_proc("kernel32.dll", "SetThreadDescription");
#endif

void my_thread_self_setname(const char *name [[maybe_unused]]) {
#ifdef HAVE_PTHREAD_SETNAME_NP_LINUX
  /*
    GNU extension, see pthread_setname_np(3)
  */
  char truncated_name[SETNAME_MAX_LENGTH];
  strncpy(truncated_name, name, sizeof(truncated_name) - 1);
  truncated_name[sizeof(truncated_name) - 1] = '\0';
  pthread_setname_np(pthread_self(), truncated_name);
#elif defined(HAVE_PTHREAD_SETNAME_NP_MACOS)
  pthread_setname_np(name);
#elif _WIN32
  /* Check if we can use the new Windows 10 API. */
  if (set_thread_name_proc.is_valid()) {
    wchar_t w_name[SETNAME_MAX_LENGTH];
    int size;

    size =
        MultiByteToWideChar(CP_UTF8, 0, name, -1, w_name, SETNAME_MAX_LENGTH);
    if (size > 0 && size <= SETNAME_MAX_LENGTH) {
      /* Make sure w_name is NUL terminated when truncated. */
      w_name[SETNAME_MAX_LENGTH - 1] = 0;
      set_thread_name_proc(GetCurrentThread(), w_name);
    }
  }

  /* According to Microsoft documentation there is a "secret handshake" between
  debuggee & debugger using the special values used below. We use it always in
  case there is a debugger attached, even if the new Win10 API for thread names
  is available. */
  constexpr DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push, 8)
  struct THREADNAME_INFO {
    DWORD dwType;
    LPCSTR szName;
    DWORD dwThreadID;
    DWORD dwFlags;
  };
#pragma pack(pop)

  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = name;
  info.dwThreadID = GetCurrentThreadId();
  info.dwFlags = 0;

#pragma warning(push)
#pragma warning(disable : 6320 6322)
  __try {
    RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR),
                   (ULONG_PTR *)&info);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
#pragma warning(pop)

#else
  /* Do nothing for this platform. */
  return;
#endif
}
