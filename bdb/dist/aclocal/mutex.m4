dnl $Id: mutex.m4,v 11.20 2000/12/20 22:16:56 bostic Exp $

dnl Figure out mutexes for this compiler/architecture.
AC_DEFUN(AM_DEFINE_MUTEXES, [

AC_CACHE_CHECK([for mutexes], db_cv_mutex, [dnl
db_cv_mutex=no

orig_libs=$LIBS

dnl User-specified POSIX mutexes.
dnl
dnl Assume that -lpthread exists when the user specifies POSIX mutexes.  (I
dnl only expect this option to be used on Solaris, which has -lpthread.)
if test "$db_cv_posixmutexes" = yes; then
	db_cv_mutex="posix_only"
fi

dnl User-specified UI mutexes.
dnl
dnl Assume that -lthread exists when the user specifies UI mutexes.  (I only
dnl expect this option to be used on Solaris, which has -lthread.)
if test "$db_cv_uimutexes" = yes; then
	db_cv_mutex="ui_only"
fi

dnl LWP threads: _lwp_XXX
dnl
dnl Test for LWP threads before testing for UI/POSIX threads, we prefer them
dnl on Solaris.  There are two reasons: the Solaris C library has UI/POSIX
dnl interface stubs, but they're broken, configuring them for inter-process
dnl mutexes doesn't return an error, but it doesn't work either.  Second,
dnl there's a bug in SunOS 5.7 where applications get pwrite, not pwrite64,
dnl if they load the C library before the appropriate threads library, e.g.,
dnl tclsh using dlopen to load the DB library.  Anyway, by using LWP threads
dnl we avoid answering lots of user questions, not to mention the bugs.
if test "$db_cv_mutex" = no; then
AC_TRY_RUN([
#include <synch.h>
main(){
	static lwp_mutex_t mi = SHAREDMUTEX;
	static lwp_cond_t ci = SHAREDCV;
	lwp_mutex_t mutex = mi;
	lwp_cond_t cond = ci;
	exit (
	_lwp_mutex_lock(&mutex) ||
	_lwp_mutex_unlock(&mutex));
}], [db_cv_mutex="Solaris/lwp"])
fi

dnl UI threads: thr_XXX
dnl
dnl Try with and without the -lthread library.
if test "$db_cv_mutex" = no -o "$db_cv_mutex" = "ui_only"; then
LIBS="-lthread $LIBS"
AC_TRY_RUN([
#include <thread.h>
#include <synch.h>
main(){
	mutex_t mutex;
	cond_t cond;
	int type = USYNC_PROCESS;
	exit (
	mutex_init(&mutex, type, NULL) ||
	cond_init(&cond, type, NULL) ||
	mutex_lock(&mutex) ||
	mutex_unlock(&mutex));
}], [db_cv_mutex="UI/threads/library"])
LIBS="$orig_libs"
fi
if test "$db_cv_mutex" = no -o "$db_cv_mutex" = "ui_only"; then
AC_TRY_RUN([
#include <thread.h>
#include <synch.h>
main(){
	mutex_t mutex;
	cond_t cond;
	int type = USYNC_PROCESS;
	exit (
	mutex_init(&mutex, type, NULL) ||
	cond_init(&cond, type, NULL) ||
	mutex_lock(&mutex) ||
	mutex_unlock(&mutex));
}], [db_cv_mutex="UI/threads"])
fi
if test "$db_cv_mutex" = "ui_only"; then
	AC_MSG_ERROR([unable to find UI mutex interfaces])
fi


dnl POSIX.1 pthreads: pthread_XXX
dnl
dnl Try with and without the -lpthread library.
if test "$db_cv_mutex" = no -o "$db_cv_mutex" = "posix_only"; then
AC_TRY_RUN([
#include <pthread.h>
main(){
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	pthread_condattr_t condattr;
	pthread_mutexattr_t mutexattr;
	exit (
	pthread_condattr_init(&condattr) ||
	pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED) ||
	pthread_mutexattr_init(&mutexattr) ||
	pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED) ||
	pthread_cond_init(&cond, &condattr) ||
	pthread_mutex_init(&mutex, &mutexattr) ||
	pthread_mutex_lock(&mutex) ||
	pthread_mutex_unlock(&mutex) ||
	pthread_mutex_destroy(&mutex) ||
	pthread_cond_destroy(&cond) ||
	pthread_condattr_destroy(&condattr) ||
	pthread_mutexattr_destroy(&mutexattr));
}], [db_cv_mutex="POSIX/pthreads"])
fi
if test "$db_cv_mutex" = no -o "$db_cv_mutex" = "posix_only"; then
LIBS="-lpthread $LIBS"
AC_TRY_RUN([
#include <pthread.h>
main(){
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	pthread_condattr_t condattr;
	pthread_mutexattr_t mutexattr;
	exit (
	pthread_condattr_init(&condattr) ||
	pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED) ||
	pthread_mutexattr_init(&mutexattr) ||
	pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED) ||
	pthread_cond_init(&cond, &condattr) ||
	pthread_mutex_init(&mutex, &mutexattr) ||
	pthread_mutex_lock(&mutex) ||
	pthread_mutex_unlock(&mutex) ||
	pthread_mutex_destroy(&mutex) ||
	pthread_cond_destroy(&cond) ||
	pthread_condattr_destroy(&condattr) ||
	pthread_mutexattr_destroy(&mutexattr));
}], [db_cv_mutex="POSIX/pthreads/library"])
LIBS="$orig_libs"
fi
if test "$db_cv_mutex" = "posix_only"; then
	AC_MSG_ERROR([unable to find POSIX mutex interfaces])
fi

dnl msemaphore: HPPA only
dnl Try HPPA before general msem test, it needs special alignment.
if test "$db_cv_mutex" = no; then
AC_TRY_RUN([
#include <sys/mman.h>
main(){
#if defined(__hppa)
	typedef msemaphore tsl_t;
	msemaphore x;
	msem_init(&x, 0);
	msem_lock(&x, 0);
	msem_unlock(&x, 0);
	exit(0);
#else
	exit(1);
#endif
}], [db_cv_mutex="HP/msem_init"])
fi

dnl msemaphore: AIX, OSF/1
if test "$db_cv_mutex" = no; then
AC_TRY_RUN([
#include <sys/types.h>
#include <sys/mman.h>;
main(){
	typedef msemaphore tsl_t;
	msemaphore x;
	msem_init(&x, 0);
	msem_lock(&x, 0);
	msem_unlock(&x, 0);
	exit(0);
}], [db_cv_mutex="UNIX/msem_init"])
fi

dnl ReliantUNIX
if test "$db_cv_mutex" = no; then
LIBS="$LIBS -lmproc"
AC_TRY_LINK([#include <ulocks.h>],
[typedef spinlock_t tsl_t;
spinlock_t x; initspin(&x, 1); cspinlock(&x); spinunlock(&x);],
[db_cv_mutex="ReliantUNIX/initspin"])
LIBS="$orig_libs"
fi

dnl SCO: UnixWare has threads in libthread, but OpenServer doesn't.
if test "$db_cv_mutex" = no; then
AC_TRY_RUN([
main(){
#if defined(__USLC__)
	exit(0);
#endif
	exit(1);
}], [db_cv_mutex="SCO/x86/cc-assembly"])
fi

dnl abilock_t: SGI
if test "$db_cv_mutex" = no; then
AC_TRY_LINK([#include <abi_mutex.h>],
[typedef abilock_t tsl_t;
abilock_t x; init_lock(&x); acquire_lock(&x); release_lock(&x);],
[db_cv_mutex="SGI/init_lock"])
fi

dnl sema_t: Solaris
dnl The sema_XXX calls do not work on Solaris 5.5.  I see no reason to ever
dnl turn this test on, unless we find some other platform that uses the old
dnl POSIX.1 interfaces.  (I plan to move directly to pthreads on Solaris.)
if test "$db_cv_mutex" = DOESNT_WORK; then
AC_TRY_LINK([#include <synch.h>],
[typedef sema_t tsl_t;
 sema_t x;
 sema_init(&x, 1, USYNC_PROCESS, NULL); sema_wait(&x); sema_post(&x);],
[db_cv_mutex="UNIX/sema_init"])
fi

dnl _lock_try/_lock_clear: Solaris
dnl On Solaris systems without Pthread or UI mutex interfaces, DB uses the
dnl undocumented _lock_try _lock_clear function calls instead of either the
dnl sema_trywait(3T) or sema_wait(3T) function calls.  This is because of
dnl problems in those interfaces in some releases of the Solaris C library.
if test "$db_cv_mutex" = no; then
AC_TRY_LINK([#include <sys/machlock.h>],
[typedef lock_t tsl_t;
 lock_t x;
 _lock_try(&x); _lock_clear(&x);],
[db_cv_mutex="Solaris/_lock_try"])
fi

dnl _check_lock/_clear_lock: AIX
if test "$db_cv_mutex" = no; then
AC_TRY_LINK([#include <sys/atomic_op.h>],
[int x; _check_lock(&x,0,1); _clear_lock(&x,0);],
[db_cv_mutex="AIX/_check_lock"])
fi

dnl Alpha/gcc: OSF/1
dnl The alpha/gcc code doesn't work as far as I know.  There are
dnl two versions, both have problems.  See Support Request #1583.
if test "$db_cv_mutex" = DOESNT_WORK; then
AC_TRY_RUN([main(){
#if defined(__alpha)
#if defined(__GNUC__)
exit(0);
#endif
#endif
exit(1);}],
[db_cv_mutex="ALPHA/gcc-assembly"])
fi

dnl PaRisc/gcc: HP/UX
if test "$db_cv_mutex" = no; then
AC_TRY_RUN([main(){
#if defined(__hppa)
#if defined(__GNUC__)
exit(0);
#endif
#endif
exit(1);}],
[db_cv_mutex="HPPA/gcc-assembly"])
fi

dnl PPC/gcc:
if test "$db_cv_mutex" = no; then
AC_TRY_RUN([main(){
#if defined(__powerpc__)
#if defined(__GNUC__)
exit(0);
#endif
#endif
exit(1);}],
[db_cv_mutex="PPC/gcc-assembly"])
fi

dnl Sparc/gcc: SunOS, Solaris
dnl The sparc/gcc code doesn't always work, specifically, I've seen assembler
dnl failures from the stbar instruction on SunOS 4.1.4/sun4c and gcc 2.7.2.2.
if test "$db_cv_mutex" = DOESNT_WORK; then
AC_TRY_RUN([main(){
#if defined(__sparc__)
#if defined(__GNUC__)
	exit(0);
#endif
#endif
	exit(1);
}], [db_cv_mutex="Sparc/gcc-assembly"])
fi

dnl 68K/gcc: SunOS
if test "$db_cv_mutex" = no; then
AC_TRY_RUN([main(){
#if (defined(mc68020) || defined(sun3))
#if defined(__GNUC__)
	exit(0);
#endif
#endif
	exit(1);
}], [db_cv_mutex="68K/gcc-assembly"])
fi

dnl x86/gcc: FreeBSD, NetBSD, BSD/OS, Linux
if test "$db_cv_mutex" = no; then
AC_TRY_RUN([main(){
#if defined(i386) || defined(__i386__)
#if defined(__GNUC__)
	exit(0);
#endif
#endif
	exit(1);
}], [db_cv_mutex="x86/gcc-assembly"])
fi

dnl ia86/gcc: Linux
if test "$db_cv_mutex" = no; then
AC_TRY_RUN([main(){
#if defined(__ia64)
#if defined(__GNUC__)
	exit(0);
#endif
#endif
	exit(1);
}], [db_cv_mutex="ia64/gcc-assembly"])
fi

dnl: uts/cc: UTS
if test "$db_cv_mutex" = no; then
AC_TRY_RUN([main(){
#if defined(_UTS)
	exit(0);
#endif
	exit(1);
}], [db_cv_mutex="UTS/cc-assembly"])
fi
])

if test "$db_cv_mutex" = no; then
	AC_MSG_WARN(
	    [THREAD MUTEXES NOT AVAILABLE FOR THIS COMPILER/ARCHITECTURE.])
	ADDITIONAL_OBJS="mut_fcntl${o} $ADDITIONAL_OBJS"
else
	AC_DEFINE(HAVE_MUTEX_THREADS)
fi

case "$db_cv_mutex" in
68K/gcc-assembly)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_68K_GCC_ASSEMBLY);;
AIX/_check_lock)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_AIX_CHECK_LOCK);;
ALPHA/gcc-assembly)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_ALPHA_GCC_ASSEMBLY);;
HP/msem_init)		ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_HPPA_MSEM_INIT);;
HPPA/gcc-assembly)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_HPPA_GCC_ASSEMBLY);;
ia64/gcc-assembly)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_IA64_GCC_ASSEMBLY);;
POSIX/pthreads)		ADDITIONAL_OBJS="mut_pthread${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_PTHREADS);;
POSIX/pthreads/library)	LIBS="-lpthread $LIBS"
			ADDITIONAL_OBJS="mut_pthread${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_PTHREADS);;
PPC/gcc-assembly)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_PPC_GCC_ASSEMBLY);;
ReliantUNIX/initspin)	LIBS="$LIBS -lmproc"
			ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_RELIANTUNIX_INITSPIN);;
SCO/x86/cc-assembly)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_SCO_X86_CC_ASSEMBLY);;
SGI/init_lock)		ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_SGI_INIT_LOCK);;
Solaris/_lock_try)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_SOLARIS_LOCK_TRY);;
Solaris/lwp)		ADDITIONAL_OBJS="mut_pthread${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_SOLARIS_LWP);;
Sparc/gcc-assembly)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_SPARC_GCC_ASSEMBLY);;
UI/threads)		ADDITIONAL_OBJS="mut_pthread${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_UI_THREADS);;
UI/threads/library)	LIBS="-lthread $LIBS"
			ADDITIONAL_OBJS="mut_pthread${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_UI_THREADS);;
UNIX/msem_init)		ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_MSEM_INIT);;
UNIX/sema_init)		ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_SEMA_INIT);;
UTS/cc-assembly)	ADDITIONAL_OBJS="$ADDITIONAL_OBJS uts4.cc${o}"
			AC_DEFINE(HAVE_MUTEX_UTS_CC_ASSEMBLY);;
x86/gcc-assembly)	ADDITIONAL_OBJS="mut_tas${o} $ADDITIONAL_OBJS"
			AC_DEFINE(HAVE_MUTEX_X86_GCC_ASSEMBLY);;
esac
])dnl
