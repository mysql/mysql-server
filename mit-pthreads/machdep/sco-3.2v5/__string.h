/* Copyright 1994-1995 The Santa Cruz Operation, Inc. All Rights Reserved. */


#if defined(_NO_PROTOTYPE)	/* Old, crufty environment */
#include <oldstyle/string.h>
#elif defined(_XOPEN_SOURCE) || defined(_XPG4_VERS)	/* Xpg4 environment */
#include <xpg4/string.h>
#elif defined(_POSIX_SOURCE) || defined(_POSIX_C_SOURCE) /* Posix environment */
#include <posix/string.h>
#elif _STRICT_ANSI 	/* Pure Ansi/ISO environment */
#include <ansi/string.h>
#elif defined(_SCO_ODS_30) /* Old, Tbird compatible environment */
#include <ods_30_compat/string.h>
#else 	/* Normal, default environment */
/*
 *   Portions Copyright (C) 1983-1995 The Santa Cruz Operation, Inc.
 *		All Rights Reserved.
 *
 *	The information in this file is provided for the exclusive use of
 *	the licensees of The Santa Cruz Operation, Inc.  Such users have the
 *	right to use, modify, and incorporate this code into other products
 *	for purposes authorized by the license agreement provided they include
 *	this notice and the associated copyright notice with any such product.
 *	The information in this file is provided "AS IS" without warranty.
 */

/*	Portions Copyright (c) 1990, 1991, 1992, 1993 UNIX System Laboratories, Inc. */
/*	Portions Copyright (c) 1979 - 1990 AT&T   */
/*	  All Rights Reserved   */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF          */
/*	UNIX System Laboratories, Inc.                          */
/*	The copyright notice above does not evidence any        */
/*	actual or intended publication of such source code.     */

#ifndef ___STRING_H
#define ___STRING_H

#pragma comment(exestr, "xpg4plus @(#) string.h 20.1 94/12/04 ")

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int	size_t;
#endif

#ifndef NULL
#define NULL	0
#endif /* NULL */



extern void	*memchr(const void *, int, size_t);
extern void	*memcpy(void *, const void *, size_t);
extern void	*memccpy(void *, const void *, int, size_t);
extern void	*memmove(void *, const void *, size_t);
extern void	*memset(void *, int, size_t);

extern char	*strchr(const char *, int);
extern char	*strcpy(char *, const char *);
extern char	*strncpy(char *, const char *, size_t);
extern char	*strcat(char *, const char *);
extern char	*strncat(char *, const char *, size_t);
extern char	*strpbrk(const char *, const char *);
extern char	*strrchr(const char *, int);
extern char	*strstr(const char *, const char *);
extern char	*strtok(char *, const char *);
extern char	*strtok_r(char *, const char *, char **);
extern char	*strerror(int);
extern char	*strlist(char *, const char *, ...);

extern int	memcmp(const void *, const void *, size_t);
extern int	strcmp(const char *, const char *);
extern int	strcoll(const char *, const char *);
extern int	strncmp(const char *, const char *, size_t);

extern void perror(const char *);
extern char *strdup(const char *);
extern int     strncoll(const char *, const char *, int);
extern size_t  strnxfrm(char *, const char *, size_t , int);

extern size_t	strxfrm(char *, const char *, size_t);
extern size_t	strcspn(const char *, const char *);
extern size_t	strspn(const char *, const char *);
extern size_t	strlen(const char *);

#ifdef __USLC__
#pragma int_to_unsigned strcspn
#pragma int_to_unsigned strspn
#pragma int_to_unsigned strlen
#endif

#if !defined(__cplusplus) && defined(__USLC__)
/* Use intrinsic ??? */
#ifndef strlen
#define strlen	__std_hdr_strlen
#endif
#ifndef strcpy
#define strcpy	__std_hdr_strcpy
#endif
#ifndef strncpy
#define strncpy	__std_hdr_strncpy
#endif
#endif


extern int	ffs(int);
/*
 * The following two functions were withdrawn in XPG3,
 * but are provided for backwards compatibility.
 */
extern int nl_strcmp(char *, char *);
extern int nl_strncmp(char *, char *, int n);



#ifdef __cplusplus
}
#endif

#endif /* ___STRING_H */
#endif
