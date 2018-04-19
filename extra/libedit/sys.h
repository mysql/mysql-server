/*	$NetBSD: sys.h,v 1.17 2011/09/28 14:08:04 christos Exp $	*/

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
 * 3. Neither the name of the University nor the names of its contributors
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

#if !defined(__attribute__) && (defined(__cplusplus) || !defined(__GNUC__)  || __GNUC__ == 2 && __GNUC_MINOR__ < 8)
# define __attribute__(A)
#endif

#ifndef __BEGIN_DECLS
# ifdef  __cplusplus
#  define __BEGIN_DECLS  extern "C" {
#  define __END_DECLS    }
# else
#  define __BEGIN_DECLS
#  define __END_DECLS
# endif
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

#ifndef __arraycount
# define __arraycount(a) (sizeof(a) / sizeof(*(a)))
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

#ifdef __linux__
/* Apparently we need _GNU_SOURCE defined to get access to wcsdup on Linux */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#ifndef __USE_XOPEN
#define __USE_XOPEN
#endif

#include <wchar.h>
#include <wctype.h>

#ifndef HAVE_WCSDUP
wchar_t *wcsdup(const wchar_t *);
#endif

#ifndef _DIAGASSERT
#define _DIAGASSERT(x)
#endif

#ifndef __RCSID
#define __RCSID(x)
#endif

#ifndef HAVE_U_INT32_T
typedef unsigned int	u_int32_t;
#endif

#ifndef SIZE_T_MAX
#define SIZE_T_MAX	((size_t)-1)
#endif

#define	REGEX		/* Use POSIX.2 regular expression functions */
#undef	REGEXP		/* Use UNIX V8 regular expression functions */

#if defined(__sun)
extern int tgetent(char *, const char *);
extern int tgetflag(char *);
extern int tgetnum(char *);
extern int tputs(const char *, int, int (*)(int));
extern char* tgoto(const char*, int, int);
extern char* tgetstr(char*, char**);
#endif

/* XXXMYSQL: Bug#10218 Command line recall rolls into segfault */
#if !HAVE_DECL_TGOTO
/*
 'tgoto' is not declared in the system header files, this causes
 problems on 64-bit systems. The function returns a 64 bit pointer
 but caller see it as "int" and it's thus truncated to 32-bit
*/
extern char* tgoto(const char*, int, int);
#endif

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
extern void *   memcpy(void *, const void *, size_t);
extern void *   memset(void *, int, size_t);
# endif
extern char    *fgetline(FILE *, int *);
#endif

#endif /* _h_sys */
