/* ==== syscall.S ============================================================
 * Copyright (c) 1993 by John F. Carr, jfc@mit.edu
 *
 *  1.00 93/09/xx proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif
 
/* DO NOT USE COLONS IN THIS FILE, GCC CAN'T HANDLE THEM */
/* #include <copyright.h> */

	.globl	.oVncs
	.text
	.align	2

/* ==========================================================================
 * machdep_sys_write()
 */
	.globl	_machdep_sys_write
	.globl	_.machdep_sys_write

	.text	2
_machdep_sys_write:
	.long	_.machdep_sys_write

	.text
_.machdep_sys_write:
	lcs	r0,0(r3)
	svc	4(r0)
	bntbr	r15
	store	r2,_errno,r5
	brx	r15
	cal	r2,-1(r0)


/* ==========================================================================
 * machdep_sys_read()
 */
	.globl	_machdep_sys_read
	.globl	_.machdep_sys_read

	.text	2
_machdep_sys_read:
	.long	_.machdep_sys_read

	.text
_.machdep_sys_read:
	lcs	r0,0(r3)
	svc	3(r0)
	bntbr	r15
	store	r2,_errno,r5
	brx	r15
	cal	r2,-1(r0)


/* ==========================================================================
 * machdep_sys_open()
 */
	.globl	_machdep_sys_open
	.globl	_.machdep_sys_open

	.text	2
_machdep_sys_open:
	.long	_.machdep_sys_open

	.text
_.machdep_sys_open:
	lcs	r0,0(r2)
	svc	5(r0)
	bntbr	r15
	store	r2,_errno,r5
	brx	r15
	cal	r2,-1(r0)


/* ==========================================================================
 * machdep_sys_bind()
 */
	.globl	_machdep_sys_bind
	.globl	_.machdep_sys_bind

	.text	2
_machdep_sys_bind:
	.long	_.machdep_sys_bind

	.text
_.machdep_sys_bind:
	ls	r0,0(r3)
	svc	104(r0)
	bntbr	r15
	store	r2,_errno,r5
	brx	r15
	cal	r2,-1(r0)


/* ==========================================================================
 * machdep_sys_socket()
 */
	.globl	_machdep_sys_socket
	.globl	_.machdep_sys_socket

	.text	2
_machdep_sys_socket:
	.long	_.machdep_sys_socket

	.text
	.align	2
_.machdep_sys_socket:
	svc	97(r0)
	bntbr	r15
	store	r2,_errno,r5
	brx	r15
	cal	r2,-1(r0)


/* ==========================================================================
 * machdep_sys_close()
 */
	.globl	_machdep_sys_close
	.globl	_.machdep_sys_close

	.text	2
_machdep_sys_close:
	.long	_.machdep_sys_close

	.text
	.align	2
_.machdep_sys_close:
	svc	6(r0)
	bntbr	r15
	store	r2,_errno,r5
	brx	r15
	cal	r2,-1(r0)


/* ==========================================================================
 * machdep_sys_connect()
 */
	.globl	_machdep_sys_connect
	.globl	_.machdep_sys_connect

	.text	2
_machdep_sys_connect:
	.long	_.machdep_sys_connect

	.text
_.machdep_sys_connect:
	lcs	r0,0(r3)
	svc	98(r0)
	bntbr	r15
	store	r2,_errno,r5
	brx	r15
	cal	r2,-1(r0)

/* ==========================================================================
 * machdep_sys_accept()
 */
	.globl	_machdep_sys_accept
	.globl	_.machdep_sys_accept

	.text	2
_machdep_sys_accept:
	.long	_.machdep_sys_accept

	.text
_.machdep_sys_accept:
	lcs	r0,0(r3)
	svc	99(r0)
	bntbr	r15
	store	r2,_errno,r5
	cal	r2,-1(r0)

/* ==========================================================================
 * machdep_sys_listen()
 */
	.globl	_machdep_sys_listen
	.globl	_.machdep_sys_listen

	.text	2
	.align	2
_machdep_sys_listen:
	.long	_.machdep_sys_listen

	.text
	.align	2
_.machdep_sys_listen:
	svc	106(r0)
	bntbr	r15
	brx	r15
	twoc	r2,r2

/* ==========================================================================
 * machdep_sys_fcntl()
 */
	.globl	_.machdep_sys_fcntl
	.globl	_machdep_sys_fcntl

	.text	2
_machdep_sys_fcntl:
	.long	_.machdep_sys_fcntl
	.text
_.machdep_sys_fcntl:
	lcs	r0,0(sp)
	svc	92(r0)
	bntbr	r15
	store	r2,_errno,r5
	brx	r15
	cal	r2,-1(r0)

/* ==========================================================================
 * machdep_sys_getpeername()
 */
    .globl  _machdep_sys_getpeername
    .globl  _.machdep_sys_getpeername

    .text   2
_machdep_sys_getpeername:
    .long   _.machdep_sys_getpeername

    .text
_.machdep_sys_getpeername:
    ls  r0,0(sp)
    svc 141(r0)
    bntbr   r15
    brx r15
    twoc    r2,r2


/* ==========================================================================
 * machdep_sys_getsockopt()
 */
    .globl  _machdep_sys_getsockopt
    .globl  _.machdep_sys_getsockopt

    .text   2
_machdep_sys_getsockopt:
    .long   _.machdep_sys_getsockopt

    .text
_.machdep_sys_getsockopt:
    ls  r0,0(sp)
    svc 118(r0)
    bntbr   r15
    brx r15
    twoc    r2,r2


/* ==========================================================================
 * machdep_sys_select()
 */

	.globl	_.machdep_sys_select
	.globl	_machdep_sys_select

	.text	2
_machdep_sys_select:
	.long	_.machdep_sys_select
	.text
_.machdep_sys_select:
	svc	93(r0)
	bntbr	r15
	brx	r15
	twoc	r2,r2

/* ==========================================================================
 * __tsh()
 */
	.globl	_._tsh
	.globl	__tsh
	.text	2
__tsh:	.long	_._tsh
	.text
	.align	2
_._tsh:
	brx	r15
	tsh	r2,0(r2)


/* ==========================================================================
 * __pthread_save()
 */
	.globl	__pthread_save
	.globl	_._pthread_save
	.text	2
__pthread_save:
	.long	_._pthread_save
	.text
	.align	2
_._pthread_save:
	ail	r5,r4,0
	jne	1f
	mr	r4,sp
	lis	r5,0
	stm	r4,0(r2)
	brx	r15
	lis	r2,0
1:
	mr	r0,r15		# save old return address
	ls	r15,0(r4)	# new return address
	mr	r5,r4		# r0 to restore
	mr	r4,r3		# sp to restore
	stm	r4,0(r2)
	brx	r0
	lis	r2,0


/* ==========================================================================
 * __pthread_restore()
 */
	.globl	__pthread_restore
	.globl	_._pthread_restore
	.text	2
__pthread_restore:
	.long	_._pthread_restore
	.text
	.align	2
_._pthread_restore:
	lm	r4,0(r2)
	mr	r0,r5
	lis	r2,1
	brx	r15
	mr	sp,r4


