/*	$NetBSD: sys.h,v 1.6 2003/03/10 00:57:38 christos Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Christos Zoulas of Cornell University.
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
 *	@(#)sys.h	8.1 (Berkeley) 6/4/93
 */

/*
 * sys.h: Put all the stupid compiler and system dependencies here...
 */
#ifndef _h_sys
#define	_h_sys

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

#ifndef public
# define public		/* Externally visible functions/variables */
#endif

#ifndef private
# define private	static	/* Always hidden internals */
#endif

#ifndef protected
# define protected	/* Redefined from elsewhere to "static" */
			/* When we want to hide everything	*/
#endif

#ifndef _PTR_T
# define _PTR_T
typedef void	*ptr_t;
#endif

#ifndef _IOCTL_T
# define _IOCTL_T
typedef void	*ioctl_t;
#endif

#include <stdio.h>

#ifndef HAVE_STRLCAT
#define	strlcat libedit_strlcat
size_t	strlcat(char *dst, const char *src, size_t size);
#endif

#ifndef HAVE_STRLCPY
#define	strlcpy libedit_strlcpy
size_t	strlcpy(char *dst, const char *src, size_t size);
#endif

#ifndef HAVE_FGETLN
#define	fgetln libedit_fgetln
char	*fgetln(FILE *fp, size_t *len);
#endif

#define	REGEX		/* Use POSIX.2 regular expression functions */
#undef	REGEXP		/* Use UNIX V8 regular expression functions */

#ifdef notdef
# undef REGEX
# undef REGEXP
# include <malloc.h>
# ifdef __GNUC__
/*
 * Broken hdrs.
 */
extern int	tgetent(const char *bp, char *name);
extern int	tgetflag(const char *id);
extern int	tgetnum(const char *id);
extern char    *tgetstr(const char *id, char **area);
extern char    *tgoto(const char *cap, int col, int row);
extern int	tputs(const char *str, int affcnt, int (*putc)(int));
extern char    *getenv(const char *);
extern int	fprintf(FILE *, const char *, ...);
extern int	sigsetmask(int);
extern int	sigblock(int);
extern int	fputc(int, FILE *);
extern int	fgetc(FILE *);
extern int	fflush(FILE *);
extern int	tolower(int);
extern int	toupper(int);
extern int	errno, sys_nerr;
extern char	*sys_errlist[];
extern void	perror(const char *);
#  include <string.h>
#  define strerror(e)	sys_errlist[e]
# endif
# ifdef SABER
extern ptr_t    memcpy(ptr_t, const ptr_t, size_t);
extern ptr_t    memset(ptr_t, int, size_t);
# endif
extern char    *fgetline(FILE *, int *);
#endif

#endif /* _h_sys */
