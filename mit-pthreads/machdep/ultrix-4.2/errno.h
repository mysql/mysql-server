/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)errno.h	7.13 (Berkeley) 2/19/91
 *	errno.h,v 1.3 1993/05/20 16:22:09 cgd Exp
 */

#ifndef _SYS_ERRNO_H_
#define _SYS_ERRNO_H_

#define	EPERM			1		/* Operation not permitted */
#define	ENOENT			2		/* No such file or directory */
#define	ESRCH			3		/* No such process */
#define	EINTR			4		/* Interrupted system call */
#define	EIO				5		/* Input/output error */
#define	ENXIO			6		/* Device not configured */
#define	E2BIG			7		/* Argument list too long */
#define	ENOEXEC			8		/* Exec format error */
#define	EBADF			9		/* Bad file descriptor */
#define	ECHILD			10		/* No child processes */
#define EAGAIN			11		/* No more processes */
#define	ENOMEM			12		/* Cannot allocate memory */
#define	EACCES			13		/* Permission denied */
#define	EFAULT			14		/* Bad address */
/*						15		   Non POSIX */
/*						16		   Non POSIX */
#define	EEXIST			17		/* File exists */
#define	EXDEV			18		/* Cross-device link */
#define	ENODEV			19		/* Operation not supported by device */
#define	ENOTDIR			20		/* Not a directory */
#define	EISDIR			21		/* Is a directory */
#define	EINVAL			22		/* Invalid argument */
#define	ENFILE			23		/* Too many open files in system */
#define	EMFILE			24		/* Too many open files */
#define	ENOTTY			25		/* Inappropriate ioctl for device */
/*						26		   Non POSIX */
#define	EFBIG			27		/* File too large */
#define	ENOSPC			28		/* No space left on device */
#define	ESPIPE			29		/* Illegal seek */
#define	EROFS			30		/* Read-only file system */
#define	EMLINK			31		/* Too many links */
#define	EPIPE			32		/* Broken pipe */

/* math software */
#define	EDOM			33		/* Numerical argument out of domain */
#define	ERANGE			34		/* Result too large */
/*						35		   Non POSIX */
/*						36		   Non POSIX */
/*						37		   Non POSIX */
/*						38		   Non POSIX */
/*						39		   Non POSIX */
/*						40		   Non POSIX */
/*						41		   Non POSIX */
/*						42		   Non POSIX */
/*						43		   Non POSIX */
/*						44		   Non POSIX */
/*						45		   Non POSIX */
/*						46		   Non POSIX */
/*						47		   Non POSIX */
/*						48		   Non POSIX */
/*						49		   Non POSIX */
/*						50		   Non POSIX */
/*						51		   Non POSIX */
/*						52		   Non POSIX */
/*						53		   Non POSIX */
/*						54		   Non POSIX */
/*						55		   Non POSIX */
/*						56		   Non POSIX */
/*						57		   Non POSIX */
/*						58		   Non POSIX */
/*						59		   Non POSIX */
/*						60		   Non POSIX */
/*						61		   Non POSIX */
/*						62		   Non POSIX */
#define	ENAMETOOLONG	63		/* File name too long */
/*						64		   Non POSIX */
/*						65		   Non POSIX */
#define	ENOTEMPTY		66		/* Directory not empty */
/*						67		   Non POSIX */
/*						68		   Non POSIX */
/*						69		   Non POSIX */
/*						70		   Non POSIX */
/*						71		   Non POSIX */
/*						72		   Non POSIX */
/*						73		   Non POSIX */
/*						74		   Non POSIX */
#define	ENOLCK			75		/* No locks available */
#define	ENOSYS			76		/* Function not implemented */

#ifndef _POSIX_SOURCE
#define	ENOTBLK			15		/* Block device required */
#define	EBUSY			16		/* Device busy */
#define	ETXTBSY			26		/* Text file busy */

/* non-blocking and interrupt i/o */
#define	EWOULDBLOCK		35	/* Operation would block */
#define	EDEADLK			EWOULDBLOCK		/* Resource deadlock avoided */
#define	EINPROGRESS		36		/* Operation now in progress */
#define	EALREADY		37		/* Operation already in progress */

/* ipc/network software -- argument errors */
#define	ENOTSOCK		38		/* Socket operation on non-socket */
#define	EDESTADDRREQ	39		/* Destination address required */
#define	EMSGSIZE		40		/* Message too long */
#define	EPROTOTYPE		41		/* Protocol wrong type for socket */
#define	ENOPROTOOPT		42		/* Protocol not available */
#define	EPROTONOSUPPORT	43		/* Protocol not supported */
#define	ESOCKTNOSUPPORT	44		/* Socket type not supported */
#define	EOPNOTSUPP		45		/* Operation not supported on socket */
#define	EPFNOSUPPORT	46		/* Protocol family not supported */
#define	EAFNOSUPPORT	47		/* Address family not supported by protocol family */
#define	EADDRINUSE		48		/* Address already in use */
#define	EADDRNOTAVAIL	49		/* Can't assign requested address */

/* ipc/network software -- operational errors */
#define	ENETDOWN		50		/* Network is down */
#define	ENETUNREACH		51		/* Network is unreachable */
#define	ENETRESET		52		/* Network dropped connection on reset */
#define	ECONNABORTED	53		/* Software caused connection abort */
#define	ECONNRESET		54		/* Connection reset by peer */
#define	ENOBUFS			55		/* No buffer space available */
#define	EISCONN			56		/* Socket is already connected */
#define	ENOTCONN		57		/* Socket is not connected */
#define	ESHUTDOWN		58		/* Can't send after socket shutdown */
#define	ETOOMANYREFS	59		/* Too many references: can't splice */
#define	ETIMEDOUT		60		/* Connection timed out */
#define	ECONNREFUSED	61		/* Connection refused */

#define	ELOOP			62		/* Too many levels of symbolic links */

#define	EHOSTDOWN		64		/* Host is down */
#define	EHOSTUNREACH	65		/* No route to host */

/* quotas & mush */
#define	EPROCLIM		67		/* Too many processes */
#define	EUSERS			68		/* Too many users */
#define	EDQUOT			69		/* Disc quota exceeded */

/* Network File System */
#define	ESTALE			70		/* Stale NFS file handle */
#define	EREMOTE			71		/* Too many levels of remote in path */

/* IPC errors */
#define	ENOMSG			72		/* RPC struct is bad */
#define	EIDRM			73		/* RPC version wrong */

/* Alignment error of some type (i.e., cluster, page, block ...) */
#define	EALIGN			74		/* RPC prog. not avail */
#endif /* _POSIX_SOURCE */

#endif /* _SYS_ERRNO_H_ */
