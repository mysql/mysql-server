/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	from: @(#)time.h	5.12 (Berkeley) 3/9/91
 *	$Id$
 */

#ifndef _STDLIB_H_
#define	_STDLIB_H_

#include <sys/__stdlib.h>

/* Returned by `div'.  */
typedef struct
  {
    int quot;   /* Quotient.  */
    int rem;    /* Remainder.  */
  } div_t;

/* Returned by `ldiv'.  */
typedef struct
  {
    long quot;  /* Quotient.  */
    long rem;   /* Remainder.  */
  } ldiv_t;

#ifndef RAND_MAX
#define RAND_MAX			2147483647
#endif

#define EXIT_FAILURE    	1       /* Failing exit status.  */
#define EXIT_SUCCESS    	0       /* Successful exit status.  */

#include <sys/cdefs.h>

__BEGIN_DECLS

double			atof 		__P_((const char *));
int				atoi 		__P_((const char *));
long			atol 		__P_((const char *));
double			strtod 		__P_((const char *, char **));
long			strtol 		__P_((const char *, char **, int));
unsigned long	strtoul 	__P_((const char *, char **, int));

int				rand		__P_((void));
void			srand		__P_((unsigned int));

long			random		__P_((void));
void			srandom		__P_((unsigned int));
char 		  * initstate	__P_((unsigned int, char *, int));
char 		  * setstate	__P_((char *));

void		  * malloc		__P_((size_t));
void 		  * realloc		__P_((void *, size_t));
void		  * calloc		__P_((size_t, size_t));
void			free		__P_((void *));

__NORETURN void	abort		__P_((void));
int				atexit		__P_((void (* __func)() ));
__NORETURN void	exit		__P_((int));
int				system		__P_((const char *));

extern char	 **	environ;

char		  * getenv		__P_((const char *));
int 			putenv		__P_((const char *));
int				setenv		__P_((const char *, const char *, int));
void			unsetenv	__P_((const char *));

void		  * bsearch		__P_((const void *, const void *, size_t, size_t,
							int (* __func)__P_((const void *, const void *)) ));
void		   	qsort		__P_((void *, size_t, size_t, 
							int (* __func)__P_((const void *, const void *)) ));

int				abs			__P_((int));
long			labs		__P_((long));
div_t			div			__P_((int, int));
ldiv_t			ldiv		__P_((long, long));

void		  *	memchr		__P_((const void *, int, size_t));

/* Stuff to do */
int				mblen		__P_((const char *, size_t));
int				mbtowc		__P_((wchar_t *, const char *, size_t));
int				wctomb		__P_((char *, wchar_t));
size_t			mbstowcs	__P_((wchar_t *, const char *, size_t));
size_t			wcstombs	__P_((char *, const wchar_t *, size_t));


__END_DECLS

#endif /* !_STDLIB_H_ */
