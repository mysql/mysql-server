/* Copyright 1994-1995 The Santa Cruz Operation, Inc. All Rights Reserved. */


#if defined(_NO_PROTOTYPE)	/* Old, crufty environment */
#include <oldstyle/__stdio.h>
#elif defined(_XOPEN_SOURCE) || defined(_XPG4_VERS)	/* Xpg4 environment */
#include <xpg4/__stdio.h>
#elif defined(_POSIX_SOURCE) || defined(_POSIX_C_SOURCE) /* Posix environment */
#include <posix/__stdio.h>
#elif _STRICT_ANSI 	/* Pure Ansi/ISO environment */
#include <ansi/__stdio.h>
#elif defined(_SCO_ODS_30) /* Old, Tbird compatible environment */
#include <ods_30_compat/__stdio.h>
#else 	/* Normal, default environment */
/*
 *   Portions Copyright (C) 1984-1995 The Santa Cruz Operation, Inc.
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

#ifndef ___STDIO_H
#define ___STDIO_H

#pragma comment(exestr, "xpg4plus @(#) stdio.h 20.1 94/12/04 ")

#pragma pack(4)

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int	size_t;
#endif

#ifndef _FPOS_T
#define _FPOS_T
typedef long	fpos_t;
#endif

#ifndef _WCHAR_T
#define _WCHAR_T
typedef long	wchar_t;
#endif

#ifndef _WINT_T
#define _WINT_T
typedef long	wint_t;
#endif

#ifndef NULL
#define NULL	0
#endif /* NULL */

#ifndef EOF
#define EOF	(-1)
#endif

#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2

#ifndef TMP_MAX
#define TMP_MAX		17576	/* 26 * 26 * 26  */
#endif

#define BUFSIZ		1024	/* default buffer size  */


#define _IOEOF		0020	/* EOF reached on read  */
#define _IOERR		0040	/* I/O error from system  */

#define _IOREAD		0001	/* currently reading  */
#define _IOWRT		0002	/* currently writing  */
#define _IORW		0200	/* opened for reading and writing  */
#define _IOMYBUF	0010	/* stdio malloc()'d buffer  */

#define _SBFSIZ		8

#define L_cuserid	9

/* Non name space polluting version of above */
#define _P_tmpdir "/usr/tmp/"

#ifndef _VA_LIST
#define _VA_LIST char *
#endif


#ifdef __cplusplus
}
#endif

#pragma pack()

#endif /* ___STDIO_H */
#endif
