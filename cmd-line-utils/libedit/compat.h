#ifndef __LIBEDIT_COMPATH_H
#define __LIBEDIT_COMPATH_H

#define  __RCSID(x)
#define  __COPYRIGHT(x)

#include "compat_conf.h"

#ifndef HAVE_VIS_H
/* string visual representation - may want to reimplement */
#define strvis(d,s,m)   strcpy(d,s)
#define strunvis(d,s)   strcpy(d,s)
#endif

#ifndef HAVE_FGETLN
#include "fgetln.h"
#endif

#ifndef HAVE_ISSETUGID
#define issetugid() (getuid()!=geteuid() || getegid()!=getgid())
#endif

#ifndef HAVE_STRLCPY
#include "strlcpy.h"
#endif

#if HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

#ifndef __P
#ifdef __STDC__
#define __P(x)  x
#else
#define __P(x)  ()
#endif
#endif

#if !defined(__attribute__) && (defined(__cplusplus) || !defined(__GNUC__)  || __GNUC__ == 2 && __GNUC_MINOR__ < 8)
#define __attribute__(A)
#endif

#endif
