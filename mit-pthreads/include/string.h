/*	$NetBSD: string.h,v 1.6 1994/10/26 00:56:30 cgd Exp $	*/

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
 *	@(#)string.h	5.10 (Berkeley) 3/9/91
 */

#ifndef _STRING_H_
#define	_STRING_H_
#include <sys/cdefs.h>
#include <pthread/types.h>
#include <pthread/posix.h>
#include <sys/__string.h>

#ifndef	NULL
#define	NULL	0
#endif

__BEGIN_DECLS
void	*memchr __P_((const void *, int, size_t));
int	 memcmp __P_((const void *, const void *, size_t));
void	*memcpy __P_((void *, const void *, size_t));
void	*memmove __P_((void *, const void *, size_t));
void	*memset __P_((void *, int, size_t));
char	*strcat __P_((char *, const char *));
char	*strchr __P_((const char *, int));
int	 strcmp __P_((const char *, const char *));
int	 strcoll __P_((const char *, const char *));
char	*strcpy __P_((char *, const char *));
size_t	 strcspn __P_((const char *, const char *));
char	*strerror __P_((int));
size_t	 strlen __P_((const char *));
char	*strncat __P_((char *, const char *, size_t));
int	 strncmp __P_((const char *, const char *, size_t));
char	*strncpy __P_((char *, const char *, size_t));
char	*strpbrk __P_((const char *, const char *));
char	*strrchr __P_((const char *, int));
size_t	 strspn __P_((const char *, const char *));
char	*strstr __P_((const char *, const char *));
char	*strtok __P_((char *, const char *));
char	*strtok_r __P_((char *, const char *, char **));
size_t	 strxfrm __P_((char *, const char *, size_t));

/* Nonstandard routines common to all pthreads supported platforms */
#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
int	 ffs __P_((int));
void	*memccpy __P_((void *, const void *, int, size_t));
int	 strcasecmp __P_((const char *, const char *));
int	 strncasecmp __P_((const char *, const char *, size_t));
char	*strsignal __P_((int));
void	 swab __P_((const void *, void *, size_t));
#endif 
__END_DECLS

#endif /* _STRING_H_ */
