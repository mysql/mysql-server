/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: ex_tpcb.h,v 11.4 2000/05/17 19:21:02 bostic Exp $
 */

#ifndef _TPCB_H_
#define	_TPCB_H_

typedef enum { ACCOUNT, BRANCH, TELLER } FTYPE;

#define	TELLERS_PER_BRANCH	100
#define	ACCOUNTS_PER_TELLER	1000

#define	ACCOUNTS 1000000
#define	BRANCHES 10
#define	TELLERS 1000
#define	HISTORY	1000000
#define	HISTORY_LEN 100
#define	RECLEN 100
#define	BEGID	1000000

typedef struct _defrec {
	u_int32_t	id;
	u_int32_t	balance;
	u_int8_t	pad[RECLEN - sizeof(u_int32_t) - sizeof(u_int32_t)];
} defrec;

typedef struct _histrec {
	u_int32_t	aid;
	u_int32_t	bid;
	u_int32_t	tid;
	u_int32_t	amount;
	u_int8_t	pad[RECLEN - 4 * sizeof(u_int32_t)];
} histrec;
#endif /* _TPCB_H_ */
