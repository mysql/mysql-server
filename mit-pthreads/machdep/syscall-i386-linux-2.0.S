/* ==== syscall.S ============================================================
 * Copyright (c) 1994 Chris Provenzano, proven@mit.edu
 * All rights reserved.
 *
 * Description : Machine dependent syscalls for i386/i486/i586
 *
 *  1.00 93/08/26 proven
 *      -Started coding this file.
 *
 *	1.01 93/11/13 proven
 *		-The functions readv() and writev() added.
 *
 *	1.16 94/01/30 proven
 *		-This file now requires gas version 2.0 or greater.
 */

#ifndef lint
	.text
	.asciz "$Id$";
#endif

#include <sys/syscall.h>
#include <config.h>

#ifdef __ELF__

#define NAME(X) 	machdep_sys_##X
#define END(X)		1: ; .type NAME(X),@function ; .size NAME(X),1b - NAME(X)

#else

#define NAME(X)		_machdep_sys_##X
#define END(X)

#endif

#define SYSCALL0(x)					\
	.globl NAME(x) ;				\
									\
NAME(x):							\
									\
	push %ebx;						\
	lea	SYS_##x, %eax;				\
	int $0x80;						\
	pop %ebx;						\
	ret;							\
									\
	END(x)

#define SYSCALL1(x)					\
	.globl NAME(x) ;				\
									\
NAME(x):							\
									\
	push %ebx;						\
	mov 8(%esp), %ebx;				\
	lea	SYS_##x, %eax;				\
	int $0x80;						\
	pop %ebx;						\
	ret;							\
									\
    END(x)

#define SYSCALL2(x)					\
	.globl NAME(x) ;				\
									\
NAME(x):							\
									\
	push %ebx;						\
	mov 8(%esp), %ebx;				\
	mov 12(%esp), %ecx;				\
	lea	SYS_##x, %eax;				\
	int $0x80;						\
	pop %ebx;						\
	ret;							\
									\
    END(x)

#define SYSCALL3(x)					\
	.globl NAME(x) ;				\
									\
NAME(x):							\
									\
	push %ebx;						\
	mov 8(%esp), %ebx;				\
	mov 12(%esp), %ecx;				\
	mov 16(%esp), %edx;				\
	lea	SYS_##x, %eax;				\
	int $0x80;						\
	pop %ebx;						\
	ret; 							\
	END(x)


#define SYSCALL4(x)					\
	.globl NAME(x) ;				\
									\
NAME(x):							\
									\
	push %ebx;						\
	push %esi;						\
	mov 12(%esp), %ebx;				\
	mov 16(%esp), %ecx;				\
	mov 20(%esp), %edx;				\
	mov 24(%esp), %esi;				\
	lea	SYS_##x, %eax;				\
	int $0x80;						\
	pop %esi;						\
	pop %ebx;						\
	ret; 							\
	END(x)

#define SYSCALL5(x)					\
	.globl NAME(x) ;				\
									\
NAME(x):							\
									\
	push %ebx;						\
	push %esi;						\
	push %edi;						\
	mov 16(%esp), %ebx;				\
	mov 20(%esp), %ecx;				\
	mov 24(%esp), %edx;				\
	mov 28(%esp), %esi;				\
	mov 32(%esp), %edi;				\
	lea	SYS_##x, %eax;				\
	int $0x80;						\
	pop %edi;						\
	pop %esi;						\
	pop %ebx;						\
	ret; 							\
	END(x)

/* =========================================================================
 *		exit		 1		select		 82
 *		fork		 2		fstatfs		100
 *		read		 3		socketcall	102
 *		write		 4		flock		143
 *		open		 5		readv		145		
 *		creat		 8		writev		146
 *		link		 9
 *		unlink		10
 *		execve		11
 *		chdir		12
 *		chmod		15
 *		chown		16
 *		lseek		19
 *		rename		38
 *		dup		41
 *		pipe		42
 *		ioctl		54
 *		fcntl		55
 *		dup2		63
 *		readdir		89
 *		ftruncate	93
 *		fchmod		94
 *		fchown		95
 */

/*
 * Initial asm stuff for all functions.
 */
	.text
	.align	2


/* ==========================================================================
 * machdep_sys_read()
 */
SYSCALL3(read)

/* ==========================================================================
 * machdep_sys_write()
 */
SYSCALL3(write)

/* ==========================================================================
 * machdep_sys_open()
 */
SYSCALL3(open)

/* ==========================================================================
 * machdep_sys_close()
 */
SYSCALL1(close)

/* ==========================================================================
 * machdep_sys_creat()
 */
SYSCALL2(creat)

/* ==========================================================================
 * machdep_sys_dup2()
 */
SYSCALL2(dup2)

/* ==========================================================================
 * machdep_sys_fcntl()
 */
SYSCALL3(fcntl)

/* ==========================================================================
 * machdep_sys_fchown()
 */
SYSCALL3(fchown)

/* ==========================================================================
 * machdep_sys_fchmod()
 */
SYSCALL2(fchmod)

/* ==========================================================================
 * machdep_sys_ioctl()
 */
SYSCALL3(ioctl)

/* ==========================================================================
 * machdep_sys_chown()
 */
SYSCALL3(chown)

/* ==========================================================================
 * machdep_sys_chmod()
 */
SYSCALL2(chmod)

/* ==========================================================================
 * machdep_sys_chdir()
 */
SYSCALL1(chdir)

/* ==========================================================================
 * machdep_sys_unlink()
 */
SYSCALL1(unlink)

/* ==========================================================================
 * machdep_sys_link()
 */
SYSCALL2(link)

/* ==========================================================================
 * machdep_sys_rename()
 */
SYSCALL2(rename)

/* ==========================================================================
 * machdep_sys_lseek()
 */
SYSCALL3(lseek)

/* ==========================================================================
 * machdep_sys_pipe()
 */
SYSCALL1(pipe)

/* ==========================================================================
 * machdep_sys_dup()
 */
SYSCALL1(dup)

/* ==========================================================================
 * machdep_sys_fork()
 */
SYSCALL0(fork)

/* ==========================================================================
 * machdep_sys_execve()
 */
SYSCALL3(execve)

/* ==========================================================================
 * machdep_sys_stat()
 */
SYSCALL2(stat)

/* ==========================================================================
 * machdep_sys_fstat()
 */
SYSCALL2(fstat)

/* ==========================================================================
 * machdep_sys_lstat()
 */
SYSCALL2(lstat)

/* ==========================================================================
 * machdep_sys_fstatfs()
 */
SYSCALL2(fstatfs)

/* ==========================================================================
 * machdep_sys_ftruncate()
 */
SYSCALL2(ftruncate)

/* ==========================================================================
 * machdep_sys_chroot()
 */
SYSCALL1(chroot)

/* ==========================================================================
 * machdep_sys_uname()
 */
SYSCALL1(uname)

/* ==========================================================================
 * Nonstandard calls used to make the system work
 *
 * ==========================================================================
 * machdep_sys_select()
 */

	.globl NAME(select)

NAME(select):

	push %ebx
	lea 8(%esp), %ebx
	lea	SYS_select, %eax
	int $0x80
	pop %ebx
	ret
	END(select)

/* ==========================================================================
 * machdep_sys_wait4()
 */
SYSCALL4(wait4)

/* ==========================================================================
 * machdep_sys_readdir()
 */
SYSCALL3(readdir)

/* ==========================================================================
 * machdep_sys_socketcall()
 */
SYSCALL2(socketcall)


SYSCALL1(exit)
SYSCALL3(sigprocmask)

/* ==========================================================================
 * machdep_sys_sigsuspend()
 */

	.globl NAME(sigsuspend)

NAME(sigsuspend):

    push %ebp
	mov %esp,%ebp
	push %edi
	push %esi
	push %ebx
	mov 8(%ebp), %edx
	mov (%edx), %edx
	lea	SYS_sigsuspend, %eax
	int $0x80
	pop %ebx
	pop %esi
	pop %edi
	mov %ebp,%esp
    pop %ebp
	ret
	END(sigsuspend)

/* ==========================================================================
 * machdep_sys_readv()
 */
#ifdef HAVE_SYSCALL_READV
SYSCALL3(readv)
#endif

/* ==========================================================================
 * machdep_sys_writev()
 */
#ifdef HAVE_SYSCALL_WRITEV 
SYSCALL3(writev)
#endif

/* ==========================================================================
 * machdep_sys_flock()
 */
#ifdef HAVE_SYSCALL_FLOCK
SYSCALL2(flock)
#endif
