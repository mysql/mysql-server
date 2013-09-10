## feature detection
find_package(Threads)
find_package(ZLIB REQUIRED)

option(USE_BDB "Build some tools and tests with bdb (requires a proper BerkeleyDB include directory and library)." ON)
if(USE_BDB)
  find_package(BDB REQUIRED)
endif()

option(USE_VALGRIND "Build to run safely under valgrind (often slower)." ON)
if(USE_VALGRIND)
  find_package(Valgrind REQUIRED)
endif()

option(TOKU_DEBUG_PARANOID "Enable paranoid asserts." ON)

include(CheckIncludeFiles)

## check for some include files
check_include_files(alloca.h HAVE_ALLOCA_H)
check_include_files(arpa/inet.h HAVE_ARPA_INET_H)
check_include_files(byteswap.h HAVE_BYTESWAP_H)
check_include_files(endian.h HAVE_ENDIAN_H)
check_include_files(fcntl.h HAVE_FCNTL_H)
check_include_files(inttypes.h HAVE_INTTYPES_H)
check_include_files(libkern/OSAtomic.h HAVE_LIBKERN_OSATOMIC_H)
check_include_files(libkern/OSByteOrder.h HAVE_LIBKERN_OSBYTEORDER_H)
check_include_files(limits.h HAVE_LIMITS_H)
check_include_files(machine/endian.h HAVE_MACHINE_ENDIAN_H)
check_include_files(malloc.h HAVE_MALLOC_H)
check_include_files(malloc/malloc.h HAVE_MALLOC_MALLOC_H)
check_include_files(malloc_np.h HAVE_MALLOC_NP_H)
check_include_files(pthread.h HAVE_PTHREAD_H)
check_include_files(pthread_np.h HAVE_PTHREAD_NP_H)
check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(stdlib.h HAVE_STDLIB_H)
check_include_files(string.h HAVE_STRING_H)
check_include_files(syscall.h HAVE_SYSCALL_H)
check_include_files(sys/endian.h HAVE_SYS_ENDIAN_H)
check_include_files(sys/file.h HAVE_SYS_FILE_H)
check_include_files(sys/malloc.h HAVE_SYS_MALLOC_H)
check_include_files(sys/prctl.h HAVE_SYS_PRCTL_H)
check_include_files(sys/resource.h HAVE_SYS_RESOURCE_H)
check_include_files(sys/statvfs.h HAVE_SYS_STATVFS_H)
check_include_files(sys/syscall.h HAVE_SYS_SYSCALL_H)
check_include_files(sys/sysctl.h HAVE_SYS_SYSCTL_H)
check_include_files(sys/syslimits.h HAVE_SYS_SYSLIMITS_H)
check_include_files(sys/time.h HAVE_SYS_TIME_H)
check_include_files(unistd.h HAVE_UNISTD_H)

include(CheckSymbolExists)

## check whether we can set the mmap threshold like we can in gnu libc's malloc
check_symbol_exists(M_MMAP_THRESHOLD "malloc.h" HAVE_M_MMAP_THRESHOLD)
## check whether we have CLOCK_REALTIME
check_symbol_exists(CLOCK_REALTIME "time.h" HAVE_CLOCK_REALTIME)
## check how to do direct I/O
if (NOT CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
  set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
endif ()
check_symbol_exists(O_DIRECT "fcntl.h" HAVE_O_DIRECT)
check_symbol_exists(F_NOCACHE "fcntl.h" HAVE_F_NOCACHE)
check_symbol_exists(MAP_ANONYMOUS "sys/mman.h" HAVE_MAP_ANONYMOUS)
check_symbol_exists(PR_SET_PTRACER "sys/prctl.h" HAVE_PR_SET_PTRACER)
check_symbol_exists(PR_SET_PTRACER_ANY "sys/prctl.h" HAVE_PR_SET_PTRACER_ANY)

include(CheckFunctionExists)

## check for the right way to get the actual allocation size of a pointer
check_function_exists(malloc_size HAVE_MALLOC_SIZE)
check_function_exists(malloc_usable_size HAVE_MALLOC_USABLE_SIZE)
## check whether we have memalign or valloc (a weak substitute for memalign on darwin)
check_function_exists(memalign HAVE_MEMALIGN)
check_function_exists(valloc HAVE_VALLOC)
## check whether we have random_r or nrand48 to use as a reentrant random function
check_function_exists(nrand48 HAVE_NRAND48)
check_function_exists(random_r HAVE_RANDOM_R)
check_function_exists(mincore HAVE_MINCORE)

## clear this out in case mysql modified it
set(CMAKE_REQUIRED_LIBRARIES "")
set(EXTRA_SYSTEM_LIBS "")
check_function_exists(dlsym HAVE_DLSYM_WITHOUT_DL)
if (NOT HAVE_DLSYM_WITHOUT_DL)
  set(CMAKE_REQUIRED_LIBRARIES dl)
  check_function_exists(dlsym HAVE_DLSYM_WITH_DL)
  if (HAVE_DLSYM_WITH_DL)
    list(APPEND EXTRA_SYSTEM_LIBS dl)
  else ()
    message(FATAL_ERROR "Cannot find dlsym(), even with -ldl.")
  endif ()
endif ()
check_function_exists(backtrace HAVE_BACKTRACE_WITHOUT_EXECINFO)
if (NOT HAVE_BACKTRACE_WITHOUT_EXECINFO)
  set(CMAKE_REQUIRED_LIBRARIES execinfo)
  check_function_exists(backtrace HAVE_BACKTRACE_WITH_EXECINFO)
  if (HAVE_BACKTRACE_WITH_EXECINFO)
    list(APPEND EXTRA_SYSTEM_LIBS execinfo)
  else ()
    message(FATAL_ERROR "Cannot find backtrace(), even with -lexecinfo.")
  endif ()
endif ()

if(HAVE_CLOCK_REALTIME)
  list(APPEND EXTRA_SYSTEM_LIBS rt)
else()
  list(APPEND EXTRA_SYSTEM_LIBS System)
endif()

set(CMAKE_REQUIRED_LIBRARIES pthread)
## check whether we can change rwlock preference
check_function_exists(pthread_rwlockattr_setkind_np HAVE_PTHREAD_RWLOCKATTR_SETKIND_NP)
## check for the right way to yield using pthreads
check_function_exists(pthread_yield HAVE_PTHREAD_YIELD)
check_function_exists(pthread_yield_np HAVE_PTHREAD_YIELD_NP)
## check if we have pthread_getthreadid_np() (i.e. freebsd)
check_function_exists(pthread_getthreadid_np HAVE_PTHREAD_GETTHREADID_NP)

include(CheckCSourceCompiles)

if (HAVE_PTHREAD_YIELD)
  include(CheckPrototypeDefinition)

  check_prototype_definition(pthread_yield "void pthread_yield(void)" 0 "pthread.h" PTHREAD_YIELD_RETURNS_VOID)
  check_c_source_compiles("#include <pthread.h>
int main(void) {
  int r = pthread_yield();
  return r;
}" PTHREAD_YIELD_RETURNS_INT)
endif (HAVE_PTHREAD_YIELD)

## check whether we have gcc-style thread-local storage using a storage class modifier
check_c_source_compiles("#include <pthread.h>
static __thread int tlsvar = 0;
int main(void) { return tlsvar; }" HAVE_GNU_TLS)

## set TOKUDB_REVISION
set(CMAKE_TOKUDB_REVISION 0 CACHE INTEGER "Revision of tokudb.")
