/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	from: @(#)stdlib.h	5.13 (Berkeley) 6/4/91
 *	$Id$
 */

#ifndef _SYS___UNISTD_H_
#define _SYS___UNISTD_H_

#include <sys/cdefs.h>
#include <sys/types.h>

#define _SC_ARG_MAX     1   /* space for argv & envp */
#define _SC_CHILD_MAX       2   /* maximum children per process??? */
#define _SC_CLK_TCK     3   /* clock ticks/sec */
#define _SC_NGROUPS_MAX     4   /* number of groups if multple supp. */
#define _SC_OPEN_MAX        5   /* max open files per process */
#define _SC_JOB_CONTROL     6   /* do we have job control */
#define _SC_SAVED_IDS       7   /* do we have saved uid/gids */
#define _SC_VERSION     8   /* POSIX version supported */

#define _POSIX_JOB_CONTROL  1
#define _POSIX_SAVED_IDS    1
#define _POSIX_VERSION      198808

#define _PC_LINK_MAX        1   /* max links to file/dir */
#define _PC_MAX_CANON       2   /* max line length */
#define _PC_MAX_INPUT       3   /* max "packet" to a tty device */
#define _PC_NAME_MAX        4   /* max pathname component length */
#define _PC_PATH_MAX        5   /* max pathname length */
#define _PC_PIPE_BUF        6   /* size of a pipe */
#define _PC_CHOWN_RESTRICTED    7   /* can we give away files */
#define _PC_NO_TRUNC        8   /* trunc or error on >NAME_MAX */
#define _PC_VDISABLE        9   /* best char to shut off tty c_cc */
#define _PC_LAST        9   /* highest value of any _PC_* */

#ifndef NULL
#define	NULL		0	/* null pointer constant */
#endif

typedef int ssize_t;

#endif 
