/* Copyright 1994-1995 The Santa Cruz Operation, Inc. All Rights Reserved. */


#if defined(_NO_PROTOTYPE)	/* Old, crufty environment */
#include <oldstyle/dirent.h>
#elif defined(_XOPEN_SOURCE) || defined(_XPG4_VERS)	/* Xpg4 environment */
#include <xpg4/dirent.h>
#elif defined(_POSIX_SOURCE) || defined(_POSIX_C_SOURCE) /* Posix environment */
#include <posix/dirent.h>
#elif defined(_SCO_ODS_30) /* Old, Tbird compatible environment */
#include <ods_30_compat/dirent.h>
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

#ifndef _DIRENT_H
#define _DIRENT_H

#pragma comment(exestr, "xpg4plus @(#) dirent.h 20.1 94/12/04 ")

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(4)

#define MAXNAMLEN	512	/* maximum filename length  */
#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif
#undef DIRBLKSIZ
#define DIRBLKSIZ	1048	/* buffer size for fs-indep. dirs  */

#ifndef _SYS_TYPES_H
#include <sys/types.h>
#endif

#ifndef _SYS_DIRENT_H
#define _SYS_DIRENT_H
#ifdef __STDC__
#pragma comment(exestr, "@(#) dirent.h 25.8 94/09/22 ")
#else
#ident "@(#) dirent.h 25.8 94/09/22 "
#endif
/*
 *	Copyright (C) 1988-1994 The Santa Cruz Operation, Inc.
 *		All Rights Reserved.
 *	The information in this file is provided for the exclusive use of
 *	the licensees of The Santa Cruz Operation, Inc.  Such users have the
 *	right to use, modify, and incorporate this code into other products
 *	for purposes authorized by the license agreement provided they include
 *	this notice and the associated copyright notice with any such product.
 *	The information in this file is provided "AS IS" without warranty.
 */
/*	Copyright (c) 1984, 1986, 1987, 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@)#(head.sys:dirent.h	1.3" */

/*
 * The following structure defines the file
 * system independent directory entry.
 *
 */

#include <sys/types.h>

#ifdef  _M_I386
#pragma pack(4)
#else
#pragma pack(2)
#endif

#ifdef _INKERNEL
/*
 * dirent used by the kernel
 */
struct dirent {
	ino32_t	d_ino;		/* inode number of entry */
	off_t	d_off;		/* offset of disk directory entry */
	unsigned short	d_reclen;	/* length of this record */
	char	d_name[MAXNAMLEN+1];	/* name of file */
};

#else /* !_INKERNEL */
/*
 * dirent as used by application code
 * For now leave the declaration as is. When the new development system
 * is implemented, ino_t may be ushort or ulong. If ino_t is ulong, there
 * will be no d_pad field.
 */
struct dirent				/* data from readdir() */
	{
#if defined(_IBCS2)
	long		d_ino;
#else /* !_IBCS2 */
	ino_t		d_ino;		/* inode number of entry */
#if defined(_INO_16_T)
	short		d_pad;		/* because ino_t is ushort */
#endif /* defined(_INO_16_T) */
#endif /* defined(_IBCS2) */
	off_t		d_off;		/* offset of disk directory entry */
	unsigned short	d_reclen;	/* length of this record */
	char		d_name[MAXNAMLEN+1];	/* name of file */
	};
#endif /* _INKERNEL */

typedef struct dirent	dirent_t;

#pragma pack()
#endif	/* _SYS_DIRENT_H */

#define d_fileno	d_ino
#define	d_namlen	d_reclen

#ifdef __cplusplus
}
#endif

#pragma pack()

#endif /* _DIRENT_H */
#endif
