/* Copyright 1994-1995 The Santa Cruz Operation, Inc. All Rights Reserved. */


#if defined(_NO_PROTOTYPE)	/* Old, crufty environment */
#include <oldstyle/syscall.h>
#elif defined(_SCO_ODS_30) /* Old, Tbird compatible environment */
#include <ods_30_compat/syscall.h>
#else 	/* Normal, default environment */
/*
/    Portions Copyright (C) 1983-1995 The Santa Cruz Operation, Inc.
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

#ident	"xpg4plus @(#) sys.i386 20.1 94/12/04 "
/* #ident	"xpg4plus @(#)head:sys.i386	1.2" */

/*
/*	Definitions of Kernel Entry Call Gates
*/

#ifndef _SYSCALL_H_
#define _SYSCALL_H_

/*#define	SYSCALL	$0x7,$0*/
/*#define SIGCALL $0xF,$0*/

/*
/*	Definitions of System Call Entry Point Numbers
*/

#define	SYS_access	33
#define	SYS_acct	51
#define	SYS_advfs	70
#define	SYS_alarm	27
#define	SYS_break	17
#define	SYS_brk	17
#define	SYS_chdir	12
#define	SYS_chmod	15
#define	SYS_chown	16
#define	SYS_chroot	61
#define	SYS_close	6
#define	SYS_creat	8
#define	SYS_dup	41
#define	SYS_exec	11
#define	SYS_execve	59
#define	SYS_exit	1
#define	SYS_fcntl	62
#define	SYS_fork	2
#define	SYS_fstat	28
#define	SYS_fstatfs	38
#define SYS_fxstat	125
#define	SYS_getdents	81
#define	SYS_getgid	47
#define	SYS_getmsg	85
#define	SYS_getpid	20
#define	SYS_gettimeofday	171
#define	SYS_getuid	24
#define	SYS_gtty	32
#define	SYS_ioctl	54
#define	SYS_kill	37
#define	SYS_link	9
#define	SYS_lock	45
#define	SYS_lseek	19
#define	SYS_lstat	91
#define SYS_lxstat	124
#define	SYS_mkdir	80
#define	SYS_mknod	14
#define	SYS_mount	21
#define	SYS_msgsys	49
#define	SYS_nice	34
#define	SYS_open	5
#define	SYS_pause	29
#define	SYS_pipe	42
#define	SYS_plock	45
#define	SYS_poll	87
#define	SYS_prof	44
#define	SYS_ptrace	26
#define	SYS_putmsg	86
#define	SYS_rdebug	76
#define	SYS_read	3
#define	SYS_readlink	92
#define	SYS_readv	121
#define	SYS_rfstart	74
#define	SYS_rfstop	77
#define	SYS_rfsys	78
#define	SYS_rmdir	79
#define	SYS_rmount	72
#define	SYS_rumount	73
#define	SYS_seek	19
#define	SYS_semsys	53
#define	SYS_setgid	46
#define	SYS_setpgrp	39
#define	SYS_settimeofday	172
#define	SYS_setuid	23
#define	SYS_shmsys	52
#define	SYS_signal	48
#define	SYS_stat	18
#define	SYS_statfs	35
#define	SYS_stime	25
#define	SYS_stty	31
#define	SYS_symlink	90
#define	SYS_sync	36
#define	SYS_sys3b	50
#define SYS_sysi86  50
#define	SYS_sysacct	51
#define	SYS_sysfs	84
#define	SYS_time	13
#define	SYS_times	43
#define	SYS_uadmin	55
#define	SYS_ulimit	63
#define	SYS_umask	60
#define	SYS_umount	22
#define	SYS_unadvfs	71
#define	SYS_unlink	10
#define	SYS_utime	30
#define	SYS_utssys	57
#define	SYS_wait	7
#define	SYS_write	4
#define	SYS_writev	122
#define SYS_xstat	123
#define SYS_ftruncate	192

/* cxenix numbers are created by the formula
 * (table index << 8) + CXENIX
 */

#define CXENIX	0x28			/* Decimal 40 */

#define XLOCKING	0x0128
#define CREATSEM	0x0228
#define OPENSEM		0x0328
#define SIGSEM		0x0428
#define WAITSEM		0x0528
#define NBWAITSEM	0x0628
#define RDCHK		0x0728
#define CHSIZE		0x0a28
#define SYS_ftime	0x0b28
#define NAP		0x0c28
#define SDGET		0x0d28
#define SDFREE		0x0e28
#define SDENTER		0x0f28
#define SDLEAVE		0x1028
#define SDGETV		0x1128
#define SDWAITV		0x1228
#define PROCTL		0x2028
#define EXECSEG		0x2128
#define UNEXECSEG	0x2228
#define SYS_select	0x2428
#define SYS_eaccess	0x2528
#define SYS_paccess	0x2628
#define SYS_sigaction	0x2728
#define SYS_sigprocmask	0x2828
#define SYS_sigpending	0x2928
#define SYS_sigsuspend	0x2a28
#define SYS_getgroups	0x2b28
#define SYS_setgroups	0x2c28
#define SYS_sysconf	0x2d28
#define SYS_pathconf	0x2e28
#define SYS_fpathconf	0x2f28
#define SYS_rename	0x3028
#define	SYS_setitimer	0x3828

#define CLOCAL		127
#endif
#endif
