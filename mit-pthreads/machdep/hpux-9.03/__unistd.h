/* /afs/sipb.mit.edu/project/pthreads/src/CVS/pthreads/machdep/hpux-9.03/__unist
d.h,v 1.2 1995/03/10 03:59:53 snl Exp */

#ifndef _SYS___UNISTD_H_
#define _SYS___UNISTD_H_

#include <sys/stdsyms.h>
#include <sys/types.h>
#include <utime.h>

#ifndef NULL
#define NULL    0
#endif

#ifndef _GID_T
#define _GID_T
typedef long gid_t;
#endif

#ifndef _UID_T
#define _UID_T
typedef long uid_t;
#endif

#ifndef _PID_T
#define _PID_T
typedef long pid_t;
#endif

#ifndef _OFF_T
#define _OFF_T
typedef long off_t;
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

#ifndef _SSIZE_T
#define _SSIZE_T
typedef int ssize_t;
#endif

/* Symbolic constants for sysconf() variables defined by POSIX.1-1988: 0-7 */

#define _SC_ARG_MAX         0  /* ARG_MAX: Max length of argument to exec()
                                    including environment data */
#define _SC_CHILD_MAX       1  /* CHILD_MAX: Max of processes per userid */
#define _SC_CLK_TCK         2  /* Number of clock ticks per second */
#define _SC_NGROUPS_MAX     3  /* NGROUPS_MAX: Max of simultaneous
                                    supplementary group IDs per process */
#define _SC_OPEN_MAX        4  /* OPEN_MAX: Max of files that one process
                                    can have open at any one time */
#define _SC_JOB_CONTROL     5  /* _POSIX_JOB_CONTROL: 1 iff supported */
#define _SC_SAVED_IDS       6  /* _POSIX_SAVED_IDS: 1 iff supported */
#define _SC_1_VERSION_88    7  /* _POSIX_VERSION: Date of POSIX.1-1988 */

/* Symbolic constants for sysconf() variables added by POSIX.1-1990: 100-199 */

#define _SC_STREAM_MAX     100 /* STREAM_MAX: Max of open stdio FILEs */
#define _SC_TZNAME_MAX     101 /* TZNAME_MAX: Max length of timezone name */
#define _SC_1_VERSION_90   102 /* _POSIX_VERSION: Date of POSIX.1-1990 */

#endif /* _SYS___UNISTD_H_ */

