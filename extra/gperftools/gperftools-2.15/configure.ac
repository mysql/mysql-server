## Process this file with autoconf to produce configure.
## In general, the safest way to proceed is to run ./autogen.sh

# make sure we're interpreted by some minimal autoconf
AC_PREREQ([2.69])

AC_INIT([gperftools],[2.15],[gperftools@googlegroups.com])
# Update this value for every release!  (A:B:C will map to foo.so.(A-C).C.B)
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
TCMALLOC_SO_VERSION=9:16:5
PROFILER_SO_VERSION=5:11:5
TCMALLOC_AND_PROFILER_SO_VERSION=10:11:6

AC_SUBST(TCMALLOC_SO_VERSION)
AC_SUBST(PROFILER_SO_VERSION)
AC_SUBST(TCMALLOC_AND_PROFILER_SO_VERSION)

# The argument here is just something that should be in the current directory
# (for sanity checking)
AC_CONFIG_SRCDIR(README)
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([dist-zip foreign])
AC_CONFIG_HEADERS([src/config.h])

AM_MAINTAINER_MODE()
# Export the version information (for tc_version and friends)
TC_VERSION_MAJOR=`expr "$PACKAGE_VERSION" : '\([[0-9]]*\)'`
TC_VERSION_MINOR=`expr "$PACKAGE_VERSION" : '[[0-9]]*\.\([[0-9]]*\)'`
TC_VERSION_PATCH=`expr "$PACKAGE_VERSION" : '[[0-9]]*\.[[0-9]]*\(.*\)$'`
AC_SUBST(TC_VERSION_MAJOR)
AC_SUBST(TC_VERSION_MINOR)
AC_SUBST(TC_VERSION_PATCH)
AC_SUBST(PACKAGE_STRING)

# The user can choose not to compile in the heap-profiler, the
# heap-checker, or the cpu-profiler.  There's also the possibility
# for a 'fully minimal' compile, which leaves out the stacktrace
# code as well.  By default, we include all of these that the
# target system supports.
default_enable_cpu_profiler=yes
default_enable_heap_profiler=yes
# heap checker is Linux-only.
default_enable_heap_checker=no
heap_checker_supported=no
default_enable_debugalloc=yes
default_enable_minimal=no
default_tcmalloc_alignment=16
heap_checker_is_default=no
need_nanosleep=yes   # Used later, to decide if to run ACX_NANOSLEEP
case "$host" in
   *-mingw*) default_enable_minimal=yes; default_enable_debugalloc=no;
             need_nanosleep=no;;
   *-cygwin*) default_enable_cpu_profiler=no;;
   *-linux*) default_enable_heap_checker=yes; heap_checker_supported=yes;;
esac

# Currently only backtrace works on s390 and OSX.
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(, [
#if !defined(__s390__) && !defined(__APPLE__)
#error not s390 and not osx
#endif
return 1
])],
                  [default_enable_libunwind=no
                   default_enable_backtrace=yes],
                  [default_enable_libunwind=yes
                   default_enable_backtrace=no])

# Disable libunwind linking on ppc64 by default.
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(, [return __PPC64__])],
                  [default_enable_libunwind=no
                   default_tcmalloc_pagesize=64],
                  [default_enable_libunwind=yes
                   default_tcmalloc_pagesize=8])

AC_ARG_ENABLE([cpu-profiler],
              [AS_HELP_STRING([--disable-cpu-profiler],
                              [do not build the cpu profiler])],
              [],
              [enable_cpu_profiler="$default_enable_cpu_profiler"])
AC_ARG_ENABLE([heap-profiler],
              [AS_HELP_STRING([--disable-heap-profiler],
                              [do not build the heap profiler])],
              [],
              [enable_heap_profiler="$default_enable_heap_profiler"])
AC_ARG_ENABLE([heap-checker],
              [AS_HELP_STRING([--disable-heap-checker],
                              [do not build the heap checker])],
              [],
              [enable_heap_checker="$default_enable_heap_checker"; heap_checker_is_default=yes])
AC_ARG_ENABLE([debugalloc],
              [AS_HELP_STRING([--disable-debugalloc],
                              [do not build versions of libs with debugalloc])],
              [],
              [enable_debugalloc="$default_enable_debugalloc"])
AC_ARG_ENABLE([minimal],
              [AS_HELP_STRING([--enable-minimal],
                              [build only tcmalloc-minimal (and maybe tcmalloc-minimal-debug)])],
              [],
              [enable_minimal="$default_enable_minimal"])
if test "$enable_minimal" = yes; then
  enable_cpu_profiler=no
  enable_heap_profiler=no
  enable_heap_checker=no
fi
AC_ARG_ENABLE([stacktrace-via-backtrace],
              [AS_HELP_STRING([--enable-stacktrace-via-backtrace],
                              [enable use of backtrace() for stacktrace capturing (may deadlock)])],
              [enable_backtrace=yes],
              [enable_backtrace="$default_enable_backtrace"])
AC_ARG_ENABLE([libgcc-unwinder-by-default],
              [AS_HELP_STRING([--enable-libgcc-unwinder-by-default],
                              [prefer libgcc's _Unwind_Backtrace as default stacktrace capturing method])],
              [enable_libgcc_by_default=yes],
              [enable_libgcc_by_default=no])
AS_IF([test "x$enable_libgcc_by_default" = xyes],
      [AC_DEFINE(PREFER_LIBGCC_UNWINDER, 1, [if libgcc stacktrace method should be default])])
AC_ARG_ENABLE([libunwind],
              [AS_HELP_STRING([--enable-libunwind],
                              [enable libunwind linking])],
              [],
              [enable_libunwind="$default_enable_libunwind"])
AC_ARG_WITH([tcmalloc-pagesize],
            [AS_HELP_STRING([--with-tcmalloc-pagesize],
                            [Set the tcmalloc internal page size to 4K, 8K, 16K, 32K, 64K, 128K or 256K])],
            [],
            [with_tcmalloc_pagesize=$default_tcmalloc_pagesize])
AC_ARG_WITH([tcmalloc-alignment],
            [AS_HELP_STRING([--with-tcmalloc-alignment],
                            [Set the tcmalloc allocation alignment to 8 or 16 bytes])],
            [],
            [with_tcmalloc_alignment=$default_tcmalloc_alignment])

case "$with_tcmalloc_pagesize" in
  4)
       AC_DEFINE(TCMALLOC_PAGE_SIZE_SHIFT, 12);;
  8)
       #Default tcmalloc page size.
       ;;
  16)
       AC_DEFINE(TCMALLOC_PAGE_SIZE_SHIFT, 14);;
  32)
       AC_DEFINE(TCMALLOC_PAGE_SIZE_SHIFT, 15);;
  64)
       AC_DEFINE(TCMALLOC_PAGE_SIZE_SHIFT, 16);;
  128)
       AC_DEFINE(TCMALLOC_PAGE_SIZE_SHIFT, 17);;
  256)
       AC_DEFINE(TCMALLOC_PAGE_SIZE_SHIFT, 18,
                 [Define internal page size for tcmalloc as number of left bitshift]);;
  *)
       AC_MSG_WARN([${with_tcmalloc_pagesize}K size not supported, using default tcmalloc page size.])
esac
case "$with_tcmalloc_alignment" in
  8)
       AC_DEFINE(TCMALLOC_ALIGN_8BYTES, 1,
                 [Define 8 bytes of allocation alignment for tcmalloc]);;
  16)
       #Default tcmalloc allocation alignment.
       ;;
  *)
       AC_MSG_WARN([${with_tcmalloc_alignment} bytes not supported, using default tcmalloc allocation alignment.])
esac

AS_IF([test "x$enable_heap_checker" = xyes],
      [AS_IF([test "x$heap_checker_supported" = xno],
             [AC_MSG_NOTICE([your system isn't Linux, and I won't build heap checker despite your request])]
              enable_heap_checker=no)])

dnl Heap checker is (and has always been) Linux-only, and also now
dnl depends on clone support in libc.  Most Linux libc's do ship clone()
dnl (includes glibc, musl and even bionic), but not all. So we check.
AS_IF([test "x$enable_heap_checker" = xyes],
      [AC_MSG_CHECKING([clone() support])
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sched.h>

static int fn(void *dummy) { return 0; }

]],[[
  char stk[16];
  return clone(fn, stk, CLONE_VM|CLONE_FS|CLONE_FILES, 0);
]])], [AC_MSG_RESULT([yes])], [ dnl clone not found
  AC_MSG_RESULT([no])
  enable_heap_checker=no
  AS_IF([test "x$heap_checker_is_default" = xyes],
        [AC_MSG_NOTICE([your Linux system won't have heap checker built due to missing clone() support])],
        [AC_MSG_WARN([you requested heap checker built, but your libc doesn't have clone() support])])
])])

# Checks for programs.
AC_PROG_CXX
AC_LANG([C++])
AM_CONDITIONAL(GCC, test "$GCC" = yes)   # let the Makefile know if we're gcc

AX_CXX_COMPILE_STDCXX(11, ext, mandatory)

# Check if we have an objcopy installed that supports -W
AC_CHECK_TOOL([OBJCOPY], [objcopy], [])
AS_IF([test -n "$OBJCOPY"], [dnl
  AC_CACHE_CHECK([if $OBJCOPY supports -W], gpt_cv_objcopy_weaken, [dnl
    AC_LINK_IFELSE([AC_LANG_PROGRAM([void foo() {}])], [dnl
      AS_IF(["$OBJCOPY" -W foo conftest$ac_exeext /dev/null],
      	    [gpt_cv_objcopy_weaken=yes], [gpt_cv_objcopy_weaken=no])],
    [gpt_cv_objcopy_weaken=no])])],
  [gpt_cv_objcopy_weaken=no])
AM_CONDITIONAL(HAVE_OBJCOPY_WEAKEN, test $gpt_cv_objcopy_weaken = yes)

LT_INIT

# Lets try enable frame pointers to enable simpler stacktrace
# capturing methods, but keep performace for critical bits with
# -momit-leaf-frame-pointer. However, we should be conservative so
# that we don't disable leaf frame pointers on whatever architectures
# that have them enabled by default.
AC_CACHE_CHECK(
  [compiler and target supports -fno-omit-frame-pointer -momit-leaf-frame-pointer],
  [ac_cv_frame_pointer_cflags],
  [OLD_CXXFLAGS="$CXXFLAGS"
   CXXFLAGS="$CXXFLAGS -fno-omit-frame-pointer -momit-leaf-frame-pointer"
   AC_COMPILE_IFELSE(
     [AC_LANG_PROGRAM(
#if !(__i386__ || __x86_64__ || __riscv || __aarch64__)
#error unsupported arch
#endif
     )],
     [ac_cv_frame_pointer_cflags=yes],
     [ac_cv_frame_pointer_cflags=no])
   CXXFLAGS="$OLD_CXXFLAGS"])
AM_CONDITIONAL(ENABLE_FP_FLAGS, [test "x$ac_cv_frame_pointer_cflags" = "xyes"])

AX_C___ATTRIBUTE__

AC_MSG_CHECKING(for __attribute__((aligned(N))) on functions)
AC_CACHE_VAL(ac_cv___attribute__aligned_fn, [
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stdlib.h>
     void foo(void) __attribute__((aligned(128)));
     void foo(void) { exit(1); }]], [[]])],[ac_cv___attribute__aligned_fn=yes],[ac_cv___attribute__aligned_fn=no
  ])])
if test "$ac_cv___attribute__aligned_fn" = "yes"; then
  AC_DEFINE(HAVE___ATTRIBUTE__ALIGNED_FN, 1, [define if your compiler supports alignment of functions])
fi
AC_MSG_RESULT($ac_cv___attribute__aligned_fn)


# TODO(csilvers): we could remove a lot when WITH_CPU_PROFILER etc is "no".
AC_CHECK_TYPES([struct mallinfo],,, [#include <malloc.h>])
AC_CHECK_TYPES([struct mallinfo2],,, [#include <malloc.h>])
AC_CHECK_TYPES([Elf32_Versym],,, [#include <elf.h>])   # for vdso_support.h
AC_CHECK_FUNCS(sbrk)            # for tcmalloc to get memory
AC_CHECK_FUNCS(__sbrk)          # for tcmalloc to get memory
AC_CHECK_FUNCS(geteuid)         # for turning off services when run as root
AC_CHECK_FUNCS(fork)            # for the pthread_atfork setup
AC_CHECK_HEADERS(features.h)    # for vdso_support.h, __GLIBC__ macros
AC_CHECK_HEADERS(malloc.h)      # some systems define stuff there, others not
AC_CHECK_HEADERS(glob.h)        # for heap-profile-table (cleaning up profiles)
AC_CHECK_HEADERS(execinfo.h)    # for stacktrace? and heapchecker_unittest
AC_CHECK_HEADERS(unwind.h)      # for stacktrace
AC_CHECK_HEADERS(sched.h)       # for being nice in our spinlock code
AC_CHECK_HEADERS(sys/syscall.h)
AC_CHECK_HEADERS(sys/socket.h)  # optional; for forking out to symbolizer
AC_CHECK_HEADERS(sys/wait.h)    # optional; for forking out to symbolizer
AC_CHECK_HEADERS(poll.h)        # optional; for forking out to symbolizer
AC_CHECK_HEADERS(fcntl.h)       # for tcmalloc_unittest
AC_CHECK_HEADERS(grp.h)         # for heapchecker_unittest
AC_CHECK_HEADERS(pwd.h)         # for heapchecker_unittest
AC_CHECK_HEADERS(sys/resource.h)         # for memalign_unittest.cc
AC_CHECK_HEADERS(sys/cdefs.h)   # Where glibc defines __THROW

AC_CHECK_HEADERS(sys/ucontext.h)
AC_CHECK_HEADERS(ucontext.h)
AC_CHECK_HEADERS(cygwin/signal.h)        # ucontext on cywgin
AC_CHECK_HEADERS(asm/ptrace.h)           # get ptrace macros, e.g. PT_NIP

# check for socketpair, some system, such as QNX, need link in an socket library to use it
AC_SEARCH_LIBS([socketpair], [socket])

# We override a lot of memory allocation routines, not all of which are
# standard.  For those the system doesn't declare, we'll declare ourselves.
AC_CHECK_DECLS([cfree,
                posix_memalign,
                memalign,
                valloc,
                pvalloc],,,
               [#define _XOPEN_SOURCE 600
                #define _QNX_SOURCE 1
                #include <stdlib.h>
                #include <malloc.h>])

if test "$ac_cv_type_struct_mallinfo" = yes; then
  AC_SUBST(ac_cv_have_struct_mallinfo, 1)   # gperftools/tcmalloc.h needs this
else
  AC_SUBST(ac_cv_have_struct_mallinfo, 0)
fi

if test "$ac_cv_type_struct_mallinfo2" = yes; then
  AC_SUBST(ac_cv_have_struct_mallinfo2, 1)   # gperftools/tcmalloc.h needs this
else
  AC_SUBST(ac_cv_have_struct_mallinfo2, 0)
fi

# We hardcode HAVE_MMAP to 1. There are no interesting systems anymore
# without functional mmap. And our windows (except mingw) builds
# aren't using autoconf. So we keep HAVE_MMAP define, but only to
# distingush windows and rest.
case "$host" in
   *-mingw*) default_emergency_malloc=no;;
   *) default_emergency_malloc=yes
      AC_DEFINE(HAVE_MMAP, 1, [Define to 1 if you have a working `mmap' system call.])
esac

# We want to access the "PC" (Program Counter) register from a struct
# ucontext.  Every system has its own way of doing that. But in case
# we're dealing with unknown system, we have to check if GetPC
# actually works. But don't bother if we're not doing cpu-profiling.
if test "$enable_cpu_profiler" = yes; then
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include "src/getpc.h"]], [GetPC({})])],
                    [],
                    [AC_MSG_WARN(Could not find the PC.  Will not try to compile libprofiler...)
                     enable_cpu_profiler=no])
fi

# Some tests test the behavior of .so files, and only make sense for dynamic.
AM_CONDITIONAL(ENABLE_STATIC, test "$enable_static" = yes)

# We want to link in libunwind if it is enabled and exists.
UNWIND_LIBS=
if test "$enable_libunwind" = yes; then
  AC_CHECK_HEADERS([libunwind.h],
                   [AC_CHECK_LIB(unwind, backtrace,
                     [UNWIND_LIBS=-lunwind
                      AC_DEFINE([USE_LIBUNWIND], [1], [libunwind.h was found and is working])
                      will_use_libunwind=yes])])
fi
AC_SUBST(UNWIND_LIBS)

AC_ARG_ENABLE(frame_pointers,
              AS_HELP_STRING([--enable-frame-pointers],
                             [Add -fno-omit-frame-pointer to compile flags]),
	      , enable_frame_pointers=no)
AM_CONDITIONAL(ENABLE_FRAME_POINTERS, test "$enable_frame_pointers" = yes)

# See if the compiler supports -Wno-unused-result.
# Newer ubuntu's turn on -D_FORTIFY_SOURCE=2, enabling
# __attribute__((warn_unused_result)) for things like write(),
# which we don't care about.
AC_CACHE_CHECK([if the compiler supports -Wno-unused-result],
               perftools_cv_w_no_unused_result,
	       [OLD_CXXFLAGS="$CXXFLAGS"
	        CXXFLAGS="$CXXFLAGS -Wno-error -Wunused-result"
		# gcc doesn't warn about unknown flags unless it's
		# also warning for some other purpose, hence the
		# divide-by-0.  (We use -Wno-error to make sure the
		# divide-by-0 doesn't cause this test to fail!)
                #
                # Also gcc is giving only warning for unknown flags of
                # -Wno-XXX form. So in order to detect support we're
                # using -Wunused-result which will cause gcc to give
                # error which we can detect.
	        AC_COMPILE_IFELSE([AC_LANG_PROGRAM(, return 1/0)],
	                          perftools_cv_w_no_unused_result=yes,
                                  perftools_cv_w_no_unused_result=no)
	        CXXFLAGS="$OLD_CXXFLAGS"])
AM_CONDITIONAL(HAVE_W_NO_UNUSED_RESULT,
	       test "$perftools_cv_w_no_unused_result" = yes)

AC_ARG_ENABLE([deprecated-pprof],
              [AS_HELP_STRING([--disable-deprecated-pprof],
                [do not install old deprecated and unmaintained bundled pprof
(see github.com/google/pprof for supported version)])],
              [enable_pprof="$enableval"],
              [enable_pprof=yes])

AM_CONDITIONAL(INSTALL_PPROF,
               [test "x$enable_pprof" = xyes])

AC_ARG_ENABLE([dynamic-sized-delete-support],
              [AS_HELP_STRING([--enable-dynamic-sized-delete-support],
                [try to build run-time switch for sized delete operator])],
              [enable_dyn_sized_delete="$enableval"],
              [enable_dyn_sized_delete=no])

AS_IF([test "x$enable_dyn_sized_delete" = xyes],
      [AC_DEFINE([ENABLE_DYNAMIC_SIZED_DELETE], 1,
                 [Build runtime detection for sized delete])])

AC_ARG_ENABLE([sized-delete],
              [AS_HELP_STRING([--enable-sized-delete],
                              [build sized delete operator])],
              [enable_sized_delete="$enableval"],
              [enable_sized_delete="no"])
AS_IF([test "x$enable_sized_delete" = xyes],
        [AC_DEFINE([ENABLE_SIZED_DELETE], 1, [Build sized deletion operators])
         AC_MSG_NOTICE([Will build sized deallocation operators])],
      [AS_IF([test "x$enable_dyn_sized_delete" = xyes],
             [AC_MSG_NOTICE([Will build dynamically detected sized deallocation operators])],
             [AC_MSG_NOTICE([Will build sized deallocation operators that ignore size])])])

AC_CACHE_CHECK([if C++ compiler supports -fsized-deallocation],
               [perftools_cv_sized_deallocation_result],
               [OLD_CXXFLAGS="$CXXFLAGS"
                CXXFLAGS="$CXXFLAGS -fsized-deallocation"
                AC_LINK_IFELSE([AC_LANG_PROGRAM(
                    [[#include <new>
#include <stddef.h>]],
                    [[static void (* volatile ptr)(void *, size_t) = ::operator delete; (*ptr)(0, 256);]])],
                 perftools_cv_sized_deallocation_result=yes,
                 perftools_cv_sized_deallocation_result=no)
                CXXFLAGS="$OLD_CXXFLAGS"])

AM_CONDITIONAL(HAVE_SIZED_DEALLOCATION,
               test "$perftools_cv_sized_deallocation_result" = yes)

AC_CACHE_CHECK([if C++ compiler supports std::align_val_t without options],
               [perftools_cv_have_align_val_t],
               [AC_LINK_IFELSE([AC_LANG_PROGRAM(
                    [[#include <new>]],
                    [[(::operator delete)((::operator new)(256, std::align_val_t(16)), std::align_val_t(16))]])],
                 perftools_cv_have_align_val_t=yes,
                 perftools_cv_have_align_val_t=no)])

AC_CACHE_CHECK([if C++ compiler supports -faligned-new],
               [perftools_cv_have_f_aligned_new],
               [OLD_CXXFLAGS="$CXXFLAGS"
                CXXFLAGS="$CXXFLAGS -faligned-new"
                AC_LINK_IFELSE([AC_LANG_PROGRAM(
                    [[#include <new>]],
                    [[(::operator delete)((::operator new)(256, std::align_val_t(16)), std::align_val_t(16))]])],
                 perftools_cv_have_f_aligned_new=yes,
                 perftools_cv_have_f_aligned_new=no)
                CXXFLAGS="$OLD_CXXFLAGS"])

AM_CONDITIONAL(HAVE_F_ALIGNED_NEW,
               test "$perftools_cv_have_f_aligned_new" = yes)

AS_IF([test "$perftools_cv_have_align_val_t" = yes || test "$perftools_cv_have_f_aligned_new" = yes],
        [AC_DEFINE([ENABLE_ALIGNED_NEW_DELETE], 1, [Build new/delete operators for overaligned types])
         AC_MSG_NOTICE([Will build new/delete operators for overaligned types])],
         AC_MSG_NOTICE([Will not build new/delete operators for overaligned types]))

if test "$perftools_cv_have_align_val_t" = yes || test "$perftools_cv_have_f_aligned_new" = yes; then
  AC_SUBST(ac_cv_have_std_align_val_t, 1)   # gperftools/tcmalloc.h and windows/gperftools/tcmalloc.h need this
else
  AC_SUBST(ac_cv_have_std_align_val_t, 0)
fi

AC_CACHE_CHECK([if target has _Unwind_Backtrace],
               [perftools_cv_have_unwind_backtrace],
               [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
                    [[#include <unwind.h>
#if defined(__APPLE__) || defined(__FreeBSD__)
#error OSX and FreeBSD _Unwind_Backtrace recurses back to malloc
#endif
]],
                    [[&_Unwind_Backtrace]])],
                 [perftools_cv_have_unwind_backtrace=yes],
                 [perftools_cv_have_unwind_backtrace=no])])
AS_IF([test "x$perftools_cv_have_unwind_backtrace" = xyes],
      [AC_DEFINE(HAVE_UNWIND_BACKTRACE, 1, [Whether <unwind.h> contains _Unwind_Backtrace])])

AS_IF([test "x$will_use_libunwind" = xyes],
      [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(, [return __arm__])],
                         [default_emergency_malloc=yes])])

AC_ARG_ENABLE([emergency-malloc],
              [AS_HELP_STRING([--enable-emergency-malloc],
                              [build emergency malloc feature])],
              [enable_emergency_malloc="$enableval"],
              [enable_emergency_malloc="$default_emergency_malloc"])

AM_CONDITIONAL(BUILD_EMERGENCY_MALLOC, [test "x$enable_emergency_malloc" = xyes])

# Also make sure we get standard PRI... definitions, even with glibc.
# We have to use AH_VERBATIM because we need the #ifdef guard (gcc buglet)
AH_VERBATIM([__STDC_FORMAT_MACROS],
            [/* C99 says: define this to get the PRI... macros from stdint.h */
#ifndef __STDC_FORMAT_MACROS
# define __STDC_FORMAT_MACROS 1
#endif])

# Check if __environ is available (for GetenvBeforeMain)
AC_MSG_CHECKING([for __environ])
AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <unistd.h>],
                                [char **env = __environ])],
               [AC_DEFINE(HAVE___ENVIRON, 1,
                          [Define to 1 if compiler supports __environ])
                AC_MSG_RESULT([yes])],
               [AC_MSG_RESULT([no])])

# If we support __thread, that can speed up tcmalloc a bit.
# Note, however, that our code tickles a bug in gcc < 4.1.2
# involving TLS and -fPIC (which our libraries will use) on x86:
#   http://gcc.gnu.org/ml/gcc-bugs/2006-09/msg02275.html
#
# And mingw also does compile __thread but resultant code actually
# fails to work correctly at least in some not so ancient version:
# http://mingw-users.1079350.n2.nabble.com/gcc-4-4-multi-threaded-exception-handling-amp-thread-specifier-not-working-td3440749.html
#
# Also it was reported that earlier gcc versions for mips compile
# __thread but it doesn't really work
AC_MSG_CHECKING([for __thread])
AC_LINK_IFELSE([AC_LANG_PROGRAM([#if defined(__GNUC__) && ((__GNUC__ < 4) || (__GNUC__ == 4 && __GNUC_MINOR__ < 1) || (__GNUC__ == 4 && __GNUC_MINOR__ == 1 && __GNUC_PATCHLEVEL__ < 2))
#error gcc has this bug: http://gcc.gnu.org/ml/gcc-bugs/2006-09/msg02275.html
#elif defined(__MINGW32__)
#error mingw doesnt really support tls
#elif defined(__APPLE__)
#error OSX __thread support is known to call malloc which makes it unsafe to use from malloc replacement
#elif defined(_AIX)
#error AIX thread support is known to call malloc which makes it unsafe to use from malloc replacement
#elif defined(__QNXNTO__)
#error QNX thread support is known to call malloc which makes it unsafe to use from malloc replacement
#endif
], [static __thread int p = 0])],
               [AC_DEFINE(HAVE_TLS, 1,
                          Define to 1 if compiler supports __thread)
                AC_MSG_RESULT([yes])],
               [AC_MSG_RESULT([no])])

# Nanosleep requires extra libraries on some architectures (solaris).
# This sets NANOSLEEP_LIBS.  nanosleep doesn't exist on mingw, which
# is fine for us because we don't compile libspinlock, which uses it.
if test "$need_nanosleep" = yes; then
  ACX_NANOSLEEP
  AC_SUBST(NANOSLEEP_LIBS)
fi

# Solaris 10 6/06 has a bug where /usr/sfw/lib/libstdc++.la is empty.
# If so, we replace it with our own version.
LIBSTDCXX_LA_LINKER_FLAG=
if test -f /usr/sfw/lib/libstdc++.la && ! test -s /usr/sfw/lib/libstdc++.la
then
  LIBSTDCXX_LA_LINKER_FLAG='-L$(top_srcdir)/src/solaris'
fi
AC_SUBST(LIBSTDCXX_LA_LINKER_FLAG)

# In fact, a lot of the code in this directory depends on pthreads
AX_PTHREAD

# Figure out where libc has program_invocation_name
AC_PROGRAM_INVOCATION_NAME

# Make the install prefix available, to figure out where to look for pprof
AC_INSTALL_PREFIX

dnl only very recent mingw has sleep and nanosleep
case "$host" in
   *-mingw*)
     AC_CHECK_DECLS([sleep], [], [], [#include <unistd.h>])
     AC_CHECK_DECLS([nanosleep], [], [], [#include <time.h>])
   ;;
esac

if test "x$enable_backtrace" = xyes; then
  AC_CHECK_DECLS([backtrace], [], [], [#include <execinfo.h>])
  save_LIBS=$LIBS
  LIBS=$UNWIND_LIBS
  AC_SEARCH_LIBS([backtrace], [execinfo])
  UNWIND_LIBS=$LIBS
  LIBS=$save_LIBS
fi

STACKTRACE_UNITTEST_LIBS=
AS_IF([test "x$ac_cv_header_execinfo_h" = xyes],
  AS_IF([test "x$enable_cpu_profiler" = xyes -o "x$enable_heap_profiler" = xyes -o "x$enable_heap_checker" = xyes],
    [AC_CHECK_DECLS([backtrace_symbols], [], [], [#include <execinfo.h>
                                                  ])
     save_LIBS=$LIBS
     LIBS=
     AC_SEARCH_LIBS([backtrace_symbols], [execinfo])
     STACKTRACE_UNITTEST_LIBS=$LIBS
     LIBS=$save_LIBS]))
AC_SUBST(STACKTRACE_UNITTEST_LIBS)

# For windows, this has a non-trivial value (__declspec(export)), but any
# system that uses configure wants this to be the empty string.
AC_DEFINE(PERFTOOLS_DLL_DECL,,
          [Always the empty-string on non-windows systems.
           On windows, should be "__declspec(dllexport)".
	   This way, when we compile the dll, we export our functions/classes.
	   It's safe to define this here because config.h is only used
	   internally, to compile the DLL, and every DLL source file
	   #includes "config.h" before anything else.])

# In theory, config.h files shouldn't need a header guard, but we do,
# because we (maybe) #include windows/mingw.h from within config.h,
# and it #includes other .h files.  These all have header guards, so
# the end result is if config.h is #included twice, its #undefs get
# evaluated twice, but all the ones in mingw.h/etc only get evaluated
# once, potentially causing trouble.  c.f.
#   https://github.com/gperftools/gperftools/issues/248
AH_TOP([
#ifndef GPERFTOOLS_CONFIG_H_
#define GPERFTOOLS_CONFIG_H_
])

AH_VERBATIM([PTHREADS_CRASHES_IF_RUN_TOO_EARLY],
	    [/* Mark the systems where we know it's bad if pthreads runs too
   early before main (before threads are initialized, presumably).  */
#if defined(__FreeBSD__) || defined(_AIX)
#define PTHREADS_CRASHES_IF_RUN_TOO_EARLY 1
#endif])

# MinGW uses autoconf, but also needs the windows shim routines
# (since it doesn't have its own support for, say, pthreads).
# This requires us to #include a special header file, and also to
# link in some windows versions of .o's instead of the unix versions.
#
# Also, manually mark systems where we have to be careful how early
# we run pthreads.  TODO(csilvers): turn this into an autoconf check.
AH_BOTTOM([
#ifdef __MINGW32__
#include "windows/mingw.h"
#endif

#endif  /* #ifndef GPERFTOOLS_CONFIG_H_ */
])
AM_CONDITIONAL(MINGW, expr $host : '.*-mingw' >/dev/null 2>&1)
AM_CONDITIONAL(OSX, expr $host : '.*-apple-darwin.*' >/dev/null 2>&1)

# Export the --enable flags we set above.  We do this at the end so
# other configure rules can enable or disable targets based on what
# they find.
AM_CONDITIONAL(WITH_CPU_PROFILER, test "$enable_cpu_profiler" = yes)
AM_CONDITIONAL(WITH_HEAP_PROFILER, test "$enable_heap_profiler" = yes)
AM_CONDITIONAL(WITH_HEAP_CHECKER, test "$enable_heap_checker" = yes)
AM_CONDITIONAL(WITH_DEBUGALLOC, test "$enable_debugalloc" = yes)
# We make tcmalloc.so if either heap-profiler or heap-checker is asked for.
AM_CONDITIONAL(WITH_HEAP_PROFILER_OR_CHECKER,
               test "$enable_heap_profiler" = yes -o \
                    "$enable_heap_checker" = yes)
# If we don't use any profilers, we don't need stack traces (or pprof)
AM_CONDITIONAL(WITH_STACK_TRACE, test "$enable_cpu_profiler" = yes -o \
                                      "$enable_heap_profiler" = yes -o \
                                      "$enable_heap_checker" = yes)

have_linux_sigev_thread_id=no
AC_MSG_CHECKING([for Linux SIGEV_THREAD_ID])
AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM([[#include <signal.h>
                           #include <time.h>]],
                         [[return SIGEV_THREAD_ID || CLOCK_THREAD_CPUTIME_ID || __linux;]])],
        [AC_DEFINE(HAVE_LINUX_SIGEV_THREAD_ID, 1,
                  [Define if this is Linux that has SIGEV_THREAD_ID])
         have_linux_sigev_thread_id=yes
         AC_MSG_RESULT([yes])],
        [AC_MSG_RESULT([no])])

# Disable large allocation report by default.
AC_ARG_ENABLE([large-alloc-report],
              [AS_HELP_STRING([--enable-large-alloc-report],
                              [report very large allocations to stderr])],
              [enable_large_alloc_report="$enableval"],
              [enable_large_alloc_report=no])
AS_IF([test "x$enable_large_alloc_report" = xyes],
      [AC_DEFINE([ENABLE_LARGE_ALLOC_REPORT], 1, [report large allocation])])

# Enable aggressive decommit by default
AC_ARG_ENABLE([aggressive-decommit-by-default],
              [AS_HELP_STRING([--enable-aggressive-decommit-by-default],
                              [enable aggressive decommit by default])],
              [enable_aggressive_decommit_by_default="$enableval"],
              [enable_aggressive_decommit_by_default=no])
AS_IF([test "x$enable_aggressive_decommit_by_default" = xyes],
      [AC_DEFINE([ENABLE_AGGRESSIVE_DECOMMIT_BY_DEFAULT],
                 1,
                 [enable aggressive decommit by default])])

# Write generated configuration file
AC_CONFIG_FILES([Makefile
                 src/gperftools/tcmalloc.h src/windows/gperftools/tcmalloc.h])
AC_OUTPUT
