/*	$NetBSD: vis.c,v 1.22 2002/03/23 17:38:27 christos Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#include "config.h"

#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: vis.c,v 1.22 2002/03/23 17:38:27 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <assert.h>
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <stdlib.h>

#include "np/vis.h"

#ifdef __weak_alias
__weak_alias(strsvis,_strsvis)
__weak_alias(strsvisx,_strsvisx)
__weak_alias(strvis,_strvis)
__weak_alias(strvisx,_strvisx)
__weak_alias(svis,_svis)
__weak_alias(vis,_vis)
#endif

#if !HAVE_VIS_H
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#undef BELL
#if defined(__STDC__)
#define BELL '\a'
#else
#define BELL '\007'
#endif

#define isoctal(c)	(((unsigned char)(c)) >= '0' && ((unsigned char)(c)) <= '7')
#define iswhite(c)	(c == ' ' || c == '\t' || c == '\n')
#define issafe(c)	(c == '\b' || c == BELL || c == '\r')
#define xtoa(c)		"0123456789abcdef"[c]

#define MAXEXTRAS       5


char *MAKEEXTRALIST(uint flag, const char *orig)
{								
  const char *o = orig;
  char *e, *extra; 
  while (*o++)
    continue; 
  extra = (char*) malloc((size_t)((o - orig) + MAXEXTRAS));
  assert(extra);
  for (o = orig, e = extra; (*e++ = *o++) != '\0';)
    continue;
  e--;
  if (flag & VIS_SP) *e++ = ' ';
  if (flag & VIS_TAB) *e++ = '\t';
  if (flag & VIS_NL) *e++ = '\n';
  if ((flag & VIS_NOSLASH) == 0) *e++ = '\\';
  *e = '\0';
  return extra;
}


/*
 * This is HVIS, the macro of vis used to HTTP style (RFC 1808)
 */
#define HVIS(dst, c, flag, nextc, extra)				      \
do 									      \
	if (!isascii(c) || !isalnum(c) || strchr("$-_.+!*'(),", c) != NULL) { \
		*dst++ = '%';						      \
		*dst++ = xtoa(((unsigned int)c >> 4) & 0xf);		      \
		*dst++ = xtoa((unsigned int)c & 0xf);			      \
	} else {							      \
		SVIS(dst, c, flag, nextc, extra);			      \
	}								      \
while (/*CONSTCOND*/0)
	
/*
 * This is SVIS, the central macro of vis.
 * dst:	      Pointer to the destination buffer
 * c:	      Character to encode
 * flag:      Flag word
 * nextc:     The character following 'c'
 * extra:     Pointer to the list of extra characters to be
 *	      backslash-protected.
 */
#define SVIS(dst, c, flag, nextc, extra)				      \
do {									      \
	int isextra, isc;						      \
	isextra = strchr(extra, c) != NULL;				      \
	if (!isextra && isascii(c) && (isgraph(c) || iswhite(c) ||	      \
	    ((flag & VIS_SAFE) && issafe(c)))) {			      \
		*dst++ = c;						      \
		break;							      \
	}								      \
	isc = 0;							      \
	if (flag & VIS_CSTYLE) {					      \
		switch (c) {						      \
		case '\n':						      \
			isc = 1; *dst++ = '\\'; *dst++ = 'n';		      \
			break;						      \
		case '\r':						      \
			isc = 1; *dst++ = '\\'; *dst++ = 'r';		      \
			break;						      \
		case '\b':						      \
			isc = 1; *dst++ = '\\'; *dst++ = 'b';		      \
			break;						      \
		case BELL:						      \
			isc = 1; *dst++ = '\\'; *dst++ = 'a';		      \
			break;						      \
		case '\v':						      \
			isc = 1; *dst++ = '\\'; *dst++ = 'v';		      \
			break;						      \
		case '\t':						      \
			isc = 1; *dst++ = '\\'; *dst++ = 't';		      \
			break;						      \
		case '\f':						      \
			isc = 1; *dst++ = '\\'; *dst++ = 'f';		      \
			break;						      \
		case ' ':						      \
			isc = 1; *dst++ = '\\'; *dst++ = 's';		      \
			break;						      \
		case '\0':						      \
			isc = 1; *dst++ = '\\'; *dst++ = '0';		      \
			if (isoctal(nextc)) {				      \
				*dst++ = '0';				      \
				*dst++ = '0';				      \
			}						      \
		}							      \
	}								      \
	if (isc) break;							      \
	if (isextra || ((c & 0177) == ' ') || (flag & VIS_OCTAL)) {	      \
		*dst++ = '\\';						      \
		*dst++ = (unsigned char)(((unsigned int)(unsigned char)c >> 6) & 03) + '0';    \
		*dst++ = (unsigned char)(((unsigned int)(unsigned char)c >> 3) & 07) + '0';    \
		*dst++ =			     (c	      & 07) + '0';    \
	} else {							      \
		if ((flag & VIS_NOSLASH) == 0) *dst++ = '\\';		      \
		if (c & 0200) {						      \
			c &= 0177; *dst++ = 'M';			      \
		}							      \
		if (iscntrl(c)) {					      \
			*dst++ = '^';					      \
			if (c == 0177)					      \
				*dst++ = '?';				      \
			else						      \
				*dst++ = c + '@';			      \
		} else {						      \
			*dst++ = '-'; *dst++ = c;			      \
		}							      \
	}								      \
} while (/*CONSTCOND*/0)


/*
 * svis - visually encode characters, also encoding the characters
 * 	  pointed to by `extra'
 */
char *
svis(dst, c, flag, nextc, extra)
	char *dst;
	int c, flag, nextc;
	const char *extra;
{
	char *nextra, *to_be_freed;
	_DIAGASSERT(dst != NULL);
	_DIAGASSERT(extra != NULL);
	nextra= to_be_freed= MAKEEXTRALIST(flag, extra);
	if (flag & VIS_HTTPSTYLE)
		HVIS(dst, c, flag, nextc, nextra);
	else
		SVIS(dst, c, flag, nextc, nextra);
	*dst = '\0';
	free(to_be_freed);
	return(dst);
}


/*
 * strsvis, strsvisx - visually encode characters from src into dst
 *
 *	Extra is a pointer to a \0-terminated list of characters to
 *	be encoded, too. These functions are useful e. g. to
 *	encode strings in such a way so that they are not interpreted
 *	by a shell.
 *	
 *	Dst must be 4 times the size of src to account for possible
 *	expansion.  The length of dst, not including the trailing NULL,
 *	is returned. 
 *
 *	Strsvisx encodes exactly len bytes from src into dst.
 *	This is useful for encoding a block of data.
 */
int
strsvis(dst, src, flag, extra)
	char *dst;
	const char *src;
	int flag;
	const char *extra;
{
	char c;
	char *start;
	char *nextra, *to_be_freed;

	_DIAGASSERT(dst != NULL);
	_DIAGASSERT(src != NULL);
	_DIAGASSERT(extra != NULL);
	nextra= to_be_freed= MAKEEXTRALIST(flag, extra);
	if (flag & VIS_HTTPSTYLE) {
		for (start = dst; (c = *src++) != '\0'; /* empty */)
			HVIS(dst, c, flag, *src, nextra);
	} else {
		for (start = dst; (c = *src++) != '\0'; /* empty */)
			SVIS(dst, c, flag, *src, nextra);
	}
	*dst = '\0';
	free(to_be_freed);
	return (dst - start);
}


int
strsvisx(dst, src, len, flag, extra)
	char *dst;
	const char *src;
	size_t len;
	int flag;
	const char *extra;
{
	char c;
	char *start;
	char *nextra, *to_be_freed;

	_DIAGASSERT(dst != NULL);
	_DIAGASSERT(src != NULL);
	_DIAGASSERT(extra != NULL);
	nextra= to_be_freed= MAKEEXTRALIST(flag, extra);

	if (flag & VIS_HTTPSTYLE) {
		for (start = dst; len > 0; len--) {
			c = *src++;
			HVIS(dst, c, flag, len ? *src : '\0', nextra);
		}
	} else {
		for (start = dst; len > 0; len--) {
			c = *src++;
			SVIS(dst, c, flag, len ? *src : '\0', nextra);
		}
	}
	*dst = '\0';
	free(to_be_freed);
	return (dst - start);
}


/*
 * vis - visually encode characters
 */
char *
vis(dst, c, flag, nextc)
	char *dst;
	int c, flag, nextc;
	
{
	char *extra, *to_be_freed;

	_DIAGASSERT(dst != NULL);

	extra= to_be_freed= MAKEEXTRALIST(flag, "");

	if (flag & VIS_HTTPSTYLE)
	    HVIS(dst, c, flag, nextc, extra);
	else
	    SVIS(dst, c, flag, nextc, extra);
	*dst = '\0';
	free(to_be_freed);
	return (dst);
}


/*
 * strvis, strvisx - visually encode characters from src into dst
 *	
 *	Dst must be 4 times the size of src to account for possible
 *	expansion.  The length of dst, not including the trailing NULL,
 *	is returned. 
 *
 *	Strvisx encodes exactly len bytes from src into dst.
 *	This is useful for encoding a block of data.
 */
int
strvis(dst, src, flag)
	char *dst;
	const char *src;
	int flag;
{
	char *extra;
	int tmp;

	extra= MAKEEXTRALIST(flag, "");
	tmp= strsvis(dst, src, flag, extra);
	free(extra);
	return tmp;
}


int
strvisx(dst, src, len, flag)
	char *dst;
	const char *src;
	size_t len;
	int flag;
{
	char *extra;
	int tmp;

	extra= MAKEEXTRALIST(flag, "");
	tmp= strsvisx(dst, src, len, flag, extra);
	free(extra);
	return tmp;
}
#endif
