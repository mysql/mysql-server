/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

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
  @file mysys/my_malloc.cc
*/

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifdef _WIN32
#include "jemalloc_win.h"
#endif  // _WIN32

#include "memory_debugging.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_psi_config.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/psi/mysql_memory.h"
#include "mysql/psi/psi_memory.h"
#include "mysys_err.h"

struct PSI_thread;

#ifdef HAVE_PSI_MEMORY_INTERFACE
#define USE_MALLOC_WRAPPER
#endif

typedef void *(*allocator_func)(size_t, myf);
typedef void *(*realloc_func)(void *, size_t);
typedef void (*deallocator_func)(void *);

// my_std_malloc, my_std_realloc and my_std_free are intended for
// use when jemalloc is not suitable. For example, when providing
// memory management routines to OpenSSL jemalloc fails so we
// use these functions instead.
static inline void *std_allocator(size_t size, myf my_flags) {
  void *point;
  if (my_flags & MY_ZEROFILL)
    point = std::calloc(size, 1);
  else
    point = std::malloc(size);
  return point;
}

static inline void std_deallocator(void *ptr) { std::free(ptr); }

#ifndef USE_MALLOC_WRAPPER
static inline void *std_realloc(void *ptr, size_t size) {
  return std::realloc(ptr, size);
}
#endif  // !USE_MALLOC_WRAPPER

#ifdef _WIN32
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

extern "C" bool use_jemalloc_allocations();
namespace mysys {
namespace detail {
void *(*pfn_malloc)(size_t size);
void *(*pfn_calloc)(size_t number, size_t size);
void *(*pfn_realloc)(void *ptr, size_t size);
void (*pfn_free)(void *ptr);
}  // namespace detail
}  // namespace mysys

static std::string win_error_to_string(DWORD err) {
  struct local_freer {
    void operator()(LPTSTR ptr) { LocalFree(ptr); }
  };
  LPTSTR pmsg = nullptr;
  if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                         FORMAT_MESSAGE_IGNORE_INSERTS |
                         FORMAT_MESSAGE_ALLOCATE_BUFFER,
                     nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                     (LPTSTR)&pmsg, 0, nullptr)) {
    return "";
  }
  std::unique_ptr<TCHAR, local_freer> scope_guard{pmsg};
  std::string message(pmsg);
  // Remove any trailing CRLF
  if (message.rfind("\r\n") == (message.length() - 2)) {
    message.resize(message.length() - 2);
  }
  return message;
}

namespace mysys {
static bool using_jemalloc = false;
bool is_my_malloc_using_jemalloc() { return using_jemalloc; }
// This unwieldy add_or_return_messages function is a workaround for the fact
// that we can't control the order of global initializers, and the server
// logger is not available at the time the jemalloc allocation
// functions are loaded. We can't split the add_or_return_messages function
// into separate add_messages and return_messages functions with the
// messages stored in a file scoped static vector, as messages stored in
// that vector during the initialization of _another_ file scoped static
// object could be lost when a file scoped static vector is itself
// subsequently initialized.
// So message codes and strings are stored in a static function scoped vector
// for subsequent retrieval when the server logger IS available. This static
// function scoped vector is immune to the order of initialization of file
// scoped variables.
// Note also that we can't use error codes from mysqld_error.h
// here, as the mysys library is used in the generation of those
// error codes.  To avoid a circular dependency, the codes stored
// in the LogMessageInfo::m_ecode are offsets to be applied to
// the base error code ER_MY_MALLOC_USING_JEMALLOC to find the
// "real" error code.
static void add_or_return_messages(
    const LogMessageInfo &msg_info,
    std::vector<LogMessageInfo> *pvec = nullptr) {
  static std::vector<LogMessageInfo> vec_messages;

  if (pvec) {
    *pvec = vec_messages;
    return;
  }
  vec_messages.emplace_back(msg_info);
}

static void log_err_deferred(loglevel severity, int64_t ecode,
                             const std::string &message) {
  add_or_return_messages({severity, ecode, message});
}

std::vector<LogMessageInfo> fetch_jemalloc_initialization_messages() {
  std::vector<LogMessageInfo> jemalloc_init_messages;
  LogMessageInfo dummy;
  add_or_return_messages(dummy, &jemalloc_init_messages);
  return jemalloc_init_messages;
}
template <class T>
static T get_proc_address(const char *function_name, HMODULE hlib,
                          const char *dll_path) {
  auto tmp_pfn = reinterpret_cast<T>(GetProcAddress(hlib, function_name));
  if (!tmp_pfn) {
    DWORD err = GetLastError();
    std::ostringstream os;
    os << "GetProcAddress failed for \"" << function_name << "\" from \""
       << dll_path << "\" with error code " << err << ": \""
       << win_error_to_string(err) << "\" Falling back to default malloc";

    log_err_deferred(ERROR_LEVEL, MY_MALLOC_GETPROCADDRESS_FAILED_ER, os.str());
  }
  return tmp_pfn;
}
namespace detail {
std::once_flag init_malloc_pointers_flag;
/**
  Initialize (Windows only) function pointers used for some memory allocations,
  including the my_malloc family.  Note that these memory allocations can occur
  early in the process lifecycle (during construction of global objects) and
  thus init_malloc_pointers is called using std::call_once from the functions
  that wrap memory allocations rather than from my_win_init, where it would be
  too late.
  Note that as this function is invoked very early in the lifetime of the
  process (typically before main), the logging system is not yet initialized
  so calls to my_message_local send their output to stderr.
  Also note that when used from the MySQL server on Windows, there are
  typically two instances of the MySQL server process launched (see the
  --no-monitor option for launching a single instance) and thus the information
  message emitted on not finding the jemalloc dll will typically appear on
  stderr twice.
*/
void init_malloc_pointers() {
  // Set up to use the MSVC library routines by default.
  pfn_malloc = std::malloc;
  pfn_calloc = std::calloc;
  pfn_realloc = std::realloc;
  pfn_free = std::free;
  if (!use_jemalloc_allocations()) {
    return;
  }
  // If we can find the jemalloc library in the same directory as this
  // executable, and load all the required functions successfully, use the
  // jemalloc routines instead.
  // Note that the handle returned by LoadLibraryEx is deliberately NOT
  // released with a FreeLibrary call, as the functions used from this
  // library can be called from global object destructors late in the
  // process lifecycle. Not calling FreeLibrary is benign is this case.
  HMODULE hlib = LoadLibraryEx(jemalloc_dll_name, NULL,
                               LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
  if (NULL == hlib) {
    DWORD err = GetLastError();
    if (ERROR_MOD_NOT_FOUND == err) {
      // Normal behaviour: not finding the jemalloc dll means
      // we don't want to use it.
      std::ostringstream os;
      os << jemalloc_dll_name << " not found. Falling back to default malloc";
      log_err_deferred(INFORMATION_LEVEL, MY_MALLOC_USING_STD_MALLOC_ER,
                       os.str());
      return;
    }

    std::ostringstream os;
    os << "LoadLibraryEx(\"" << jemalloc_dll_name
       << "\", NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR) failed with error "
          "code "
       << err << ": \"" << win_error_to_string(err)
       << "\", using default malloc";

    log_err_deferred(ERROR_LEVEL, MY_MALLOC_LOADLIBRARY_FAILED_ER, os.str());
    return;
  }

  auto tmp_je_malloc = get_proc_address<decltype(pfn_malloc)>(
      jemalloc_malloc_function_name, hlib, jemalloc_dll_name);
  auto tmp_je_calloc = get_proc_address<decltype(pfn_calloc)>(
      jemalloc_calloc_function_name, hlib, jemalloc_dll_name);
  auto tmp_je_realloc = get_proc_address<decltype(pfn_realloc)>(
      jemalloc_realloc_function_name, hlib, jemalloc_dll_name);
  auto tmp_je_free = get_proc_address<decltype(pfn_free)>(
      jemalloc_free_function_name, hlib, jemalloc_dll_name);

  if (!tmp_je_malloc || !tmp_je_calloc || !tmp_je_realloc || !tmp_je_free) {
    return;
  }

  pfn_malloc = tmp_je_malloc;
  pfn_calloc = tmp_je_calloc;
  pfn_realloc = tmp_je_realloc;
  pfn_free = tmp_je_free;
  using_jemalloc = true;
  return;
}
}  // namespace detail
}  // namespace mysys

#define malloc(size) mysys::detail::pfn_malloc((size))
#define calloc(count, size) mysys::detail::pfn_calloc((count), (size))
#define realloc(p, size) mysys::detail::pfn_realloc((p), (size))
#define free(p) mysys::detail::pfn_free((p))

#endif  // _WIN32

static inline void *redirecting_allocator(size_t size, myf my_flags) {
#ifdef _WIN32
  std::call_once(mysys::detail::init_malloc_pointers_flag,
                 mysys::detail::init_malloc_pointers);
#endif  // _WIN32
  void *point;
  if (my_flags & MY_ZEROFILL)
    point = calloc(size, 1);
  else
    point = malloc(size);
  return point;
}

#ifndef USE_MALLOC_WRAPPER
static inline void *redirecting_realloc(void *ptr, size_t size) {
#ifdef _WIN32
  std::call_once(mysys::detail::init_malloc_pointers_flag,
                 mysys::detail::init_malloc_pointers);
#endif  // _WIN32
  return realloc(ptr, size);
}
#endif  // !USE_MALLOC_WRAPPER

static inline void redirecting_deallocator(void *ptr) {
#ifdef _WIN32
  std::call_once(mysys::detail::init_malloc_pointers_flag,
                 mysys::detail::init_malloc_pointers);
#endif  // _WIN32
  free(ptr);
}

/**
  Allocate a sized block of memory.

  @param size   The size of the memory block in bytes.
  @param my_flags  Failure action modifiers (bitmasks).

  @return A pointer to the allocated memory block, or NULL on failure.
*/
template <allocator_func allocator>
void *my_raw_malloc(size_t size, myf my_flags) {
  void *point;

  /* Safety */
  if (!size) size = 1;

#if defined(MY_MSCRT_DEBUG)
  if (my_flags & MY_ZEROFILL)
    point = _calloc_dbg(size, 1, _CLIENT_BLOCK, __FILE__, __LINE__);
  else
    point = _malloc_dbg(size, _CLIENT_BLOCK, __FILE__, __LINE__);
#else
  point = allocator(size, my_flags);
#endif  // MY_MSCRT_DEBUG

  DBUG_EXECUTE_IF("simulate_out_of_memory", {
    free(point);
    point = nullptr;
  });
  DBUG_EXECUTE_IF("simulate_persistent_out_of_memory", {
    free(point);
    point = nullptr;
  });

  if (point == nullptr) {
    set_my_errno(errno);
    if (my_flags & MY_FAE) error_handler_hook = my_message_stderr;
    if (my_flags & (MY_FAE + MY_WME))
      my_error(EE_OUTOFMEMORY, MYF(ME_ERRORLOG + ME_FATALERROR), size);
    DBUG_EXECUTE_IF("simulate_out_of_memory",
                    DBUG_SET("-d,simulate_out_of_memory"););
    if (my_flags & MY_FAE) exit(1);
  }

  return (point);
}

/**
  Free memory allocated with my_raw_malloc.

  @remark Relies on free being able to handle a NULL argument.

  @param ptr Pointer to the memory allocated by my_raw_malloc.
*/
template <deallocator_func deallocator>
void my_raw_free(void *ptr) {
#if defined(MY_MSCRT_DEBUG)
  _free_dbg(ptr, _CLIENT_BLOCK);
#else
  deallocator(ptr);
#endif  // MY_MSCRT_DEBUG
}

#ifdef USE_MALLOC_WRAPPER
template <allocator_func allocator>
void *my_internal_malloc(PSI_memory_key key, size_t size, myf flags) {
  my_memory_header *mh;
  size_t raw_size;
  static_assert(sizeof(my_memory_header) <= PSI_HEADER_SIZE,
                "We must reserve enough memory to hold the header.");

  raw_size = PSI_HEADER_SIZE + size;
  mh = (my_memory_header *)my_raw_malloc<allocator>(raw_size, flags);
  if (likely(mh != nullptr)) {
    void *user_ptr;
    mh->m_magic = PSI_MEMORY_MAGIC;
    mh->m_size = size;
    mh->m_key = PSI_MEMORY_CALL(memory_alloc)(key, raw_size, &mh->m_owner);
    user_ptr = HEADER_TO_USER(mh);
    MEM_MALLOCLIKE_BLOCK(user_ptr, size, 0, (flags & MY_ZEROFILL));
    return user_ptr;
  }
  return nullptr;
}

void *my_malloc(PSI_memory_key key, size_t size, myf flags) {
  return my_internal_malloc<redirecting_allocator>(key, size, flags);
}

void *my_std_malloc(PSI_memory_key key, size_t size, myf flags) {
  return my_internal_malloc<std_allocator>(key, size, flags);
}

template <deallocator_func deallocator>
void my_internal_free(void *ptr) {
  my_memory_header *mh;

  if (ptr == nullptr) return;

  mh = USER_TO_HEADER(ptr);
  assert(mh->m_magic == PSI_MEMORY_MAGIC);
  PSI_MEMORY_CALL(memory_free)
  (mh->m_key, mh->m_size + PSI_HEADER_SIZE, mh->m_owner);
  /* Catch double free */
  mh->m_magic = 0xDEAD;
  MEM_FREELIKE_BLOCK(ptr, 0);
  my_raw_free<deallocator>(mh);
}

template <allocator_func allocator, deallocator_func deallocator>
void *my_internal_realloc(PSI_memory_key key, void *ptr, size_t size,
                          myf flags) {
  my_memory_header *old_mh;
  size_t old_size;
  size_t min_size;
  void *new_ptr;

  if (ptr == nullptr) return my_internal_malloc<allocator>(key, size, flags);

  old_mh = USER_TO_HEADER(ptr);
  assert((PSI_REAL_MEM_KEY(old_mh->m_key) == key) ||
         (old_mh->m_key == PSI_NOT_INSTRUMENTED));
  assert(old_mh->m_magic == PSI_MEMORY_MAGIC);

  old_size = old_mh->m_size;

  if (old_size == size) return ptr;

  new_ptr = my_internal_malloc<allocator>(key, size, flags);
  if (likely(new_ptr != nullptr)) {
#ifndef NDEBUG
    my_memory_header *new_mh = USER_TO_HEADER(new_ptr);
#endif

    assert((PSI_REAL_MEM_KEY(new_mh->m_key) == key) ||
           (new_mh->m_key == PSI_NOT_INSTRUMENTED));
    assert(new_mh->m_magic == PSI_MEMORY_MAGIC);
    assert(new_mh->m_size == size);

    min_size = (old_size < size) ? old_size : size;
    memcpy(new_ptr, ptr, min_size);
    my_internal_free<deallocator>(ptr);

    return new_ptr;
  }
  return nullptr;
}

void *my_realloc(PSI_memory_key key, void *ptr, size_t size, myf flags) {
  return my_internal_realloc<redirecting_allocator, redirecting_deallocator>(
      key, ptr, size, flags);
}

void *my_std_realloc(PSI_memory_key key, void *ptr, size_t size, myf flags) {
  return my_internal_realloc<std_allocator, std_deallocator>(key, ptr, size,
                                                             flags);
}
void my_claim(const void *ptr, bool claim) {
  my_memory_header *mh;

  if (ptr == nullptr) return;

  mh = USER_TO_HEADER(const_cast<void *>(ptr));
  assert(mh->m_magic == PSI_MEMORY_MAGIC);
  mh->m_key = PSI_MEMORY_CALL(memory_claim)(
      mh->m_key, mh->m_size + PSI_HEADER_SIZE, &mh->m_owner, claim);
}

void my_free(void *ptr) { my_internal_free<redirecting_deallocator>(ptr); }

void my_std_free(void *ptr) { my_internal_free<std_deallocator>(ptr); }

#else
void *my_malloc(PSI_memory_key key [[maybe_unused]], size_t size,
                myf my_flags) {
  return my_raw_malloc<redirecting_allocator>(size, my_flags);
}

void *my_std_malloc(PSI_memory_key key [[maybe_unused]], size_t size,
                    myf my_flags) {
  return my_raw_malloc<std_allocator>(size, my_flags);
}

template <realloc_func reallocator, allocator_func allocator,
          deallocator_func deallocator>
void *my_internal_realloc(void *oldpoint, size_t size, myf my_flags) {
  void *point;
  DBUG_TRACE;
  DBUG_PRINT("my", ("ptr: %p  size: %lu  my_flags: %d", oldpoint, (ulong)size,
                    my_flags));

  assert(size > 0);
  /* These flags are mutually exclusive. */
  assert(!((my_flags & MY_FREE_ON_ERROR) && (my_flags & MY_HOLD_ON_ERROR)));
  DBUG_EXECUTE_IF("simulate_out_of_memory", point = NULL; goto end;);
  if (!oldpoint && (my_flags & MY_ALLOW_ZERO_PTR))
    return my_raw_malloc<allocator>(size, my_flags);
#if defined(MY_MSCRT_DEBUG)
  point = _realloc_dbg(oldpoint, size, _CLIENT_BLOCK, __FILE__, __LINE__);
#else
  point = reallocator(oldpoint, size);
#endif  // MY_MSCRT_DEBUG
#ifndef NDEBUG
end:
#endif
  if (point == nullptr) {
    if (my_flags & MY_HOLD_ON_ERROR) return oldpoint;
    if (my_flags & MY_FREE_ON_ERROR) my_raw_free<deallocator>(oldpoint);
    set_my_errno(errno);
    if (my_flags & (MY_FAE + MY_WME))
      my_error(EE_OUTOFMEMORY, MYF(ME_FATALERROR), size);
    DBUG_EXECUTE_IF("simulate_out_of_memory",
                    DBUG_SET("-d,simulate_out_of_memory"););
  }
  DBUG_PRINT("exit", ("ptr: %p", point));
  return point;
}

void *my_realloc(PSI_memory_key key [[maybe_unused]], void *ptr, size_t size,
                 myf flags) {
  return my_internal_realloc<redirecting_realloc, redirecting_allocator,
                             redirecting_deallocator>(ptr, size, flags);
}

void *my_std_realloc(PSI_memory_key key [[maybe_unused]], void *ptr,
                     size_t size, myf flags) {
  return my_internal_realloc<std_realloc, std_allocator, std_deallocator>(
      ptr, size, flags);
}

void my_claim(const void *ptr [[maybe_unused]],
              bool claim [[maybe_unused]]) { /* Empty */
}

void my_free(void *ptr) { my_raw_free<redirecting_deallocator>(ptr); }

void my_std_free(void *ptr) { my_raw_free<std_deallocator>(ptr); }
#endif

void *my_memdup(PSI_memory_key key, const void *from, size_t length,
                myf my_flags) {
  void *ptr;
  if ((ptr = my_malloc(key, length, my_flags)) != nullptr)
    memcpy(ptr, from, length);
  return ptr;
}

char *my_strdup(PSI_memory_key key, const char *from, myf my_flags) {
  char *ptr;
  size_t length = strlen(from) + 1;
  if ((ptr = (char *)my_malloc(key, length, my_flags)))
    memcpy(ptr, from, length);
  return ptr;
}

char *my_strndup(PSI_memory_key key, const char *from, size_t length,
                 myf my_flags) {
  char *ptr;
  if ((ptr = (char *)my_malloc(key, length + 1, my_flags))) {
    memcpy(ptr, from, length);
    ptr[length] = 0;
  }
  return ptr;
}
