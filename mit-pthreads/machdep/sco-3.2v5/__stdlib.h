/*
 *   Copyright (C) 1984-1995 The Santa Cruz Operation, Inc.
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

#ifndef ___STDLIB_H
#define ___STDLIB_H

#pragma comment(exestr, "posix @(#) stdlib.h 20.1 94/12/04 ")

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(4)

#ifndef _DIV_T
#define _DIV_T
typedef	struct
{
	int	quot;
	int	rem;
} div_t;
#endif

#ifndef _LDIV_T
#define _LDIV_T
typedef struct
{
	long	quot;
	long	rem;
} ldiv_t;
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int	size_t;
#endif

#if !defined(_SSIZE_T)
#define _SSIZE_T
typedef int	ssize_t;
#endif

#ifndef _WCHAR_T
#define _WCHAR_T
typedef long	wchar_t;
#endif

#ifndef NULL
#define NULL	0
#endif /* NULL */

#define EXIT_FAILURE	1
#define EXIT_SUCCESS	0
#define RAND_MAX	077777



extern unsigned char	__ctype[];

#define MB_CUR_MAX	((int)__ctype[520])

extern double	atof(const char *);
extern int	atoi(const char *);
extern long	atol(const char *);
extern double	strtod(const char *, char **);
extern float	strtof(const char *, char **);
extern long	strtol(const char *, char **, int);
extern unsigned long	strtoul(const char *, char **, int);

extern int	rand(void);
extern void	srand(unsigned int);

extern void	*calloc(size_t, size_t);
extern void	free(void *);
extern void	*malloc(size_t);
extern void	*realloc(void *, size_t);

extern void	abort(void);
extern void	exit(int);
extern char	*getenv(const char *);
extern int	system(const char *);

extern void	*bsearch(const void *, const void *, size_t, size_t,
			int (*)(const void *, const void *));
extern void	qsort(void *, size_t, size_t,
			int (*)(const void *, const void *));

#ifdef __cplusplus
#ifndef _ABS_INL
#define _ABS_INL
inline int (abs)(int i) {return (i > 0) ? i : -i;}
#endif
#else
extern int	(abs)(int);	/* Protect from macro definitions */
#endif

extern div_t	div(int, int);
extern long	labs(long);
extern ldiv_t	ldiv(long, long);

extern int	mbtowc(wchar_t *, const char *, size_t);
extern int	mblen(const char *, size_t);
extern int	wctomb(char *, wchar_t);

extern size_t	mbstowcs(wchar_t *, const char *, size_t);
extern size_t	wcstombs(char *, const wchar_t *, size_t);




#define mblen(s, n)	mbtowc((wchar_t *)0, s, n)

#ifdef __cplusplus
}
#endif

#pragma pack()

#endif /* ___STDLIB_H */
