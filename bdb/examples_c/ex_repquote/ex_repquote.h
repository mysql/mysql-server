/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: ex_repquote.h,v 1.27 2002/04/23 04:27:50 krinsky Exp $
 */

#ifndef _EX_REPQUOTE_H_
#define	_EX_REPQUOTE_H_

#define	SELF_EID	1

typedef struct {
	char *host;		/* Host name. */
	u_int32_t port;		/* Port on which to connect to this site. */
} repsite_t;

/* Globals */
extern int master_eid;
extern char *myaddr;

struct __member;	typedef struct __member member_t;
struct __machtab;	typedef struct __machtab machtab_t;

/* Arguments for the connect_all thread. */
typedef struct {
	DB_ENV *dbenv;
	const char *progname;
	const char *home;
	machtab_t *machtab;
	repsite_t *sites;
	int nsites;
} all_args;

/* Arguments for the connect_loop thread. */
typedef struct {
	DB_ENV *dbenv;
	const char * home;
	const char * progname;
	machtab_t *machtab;
	int port;
} connect_args;

#define	CACHESIZE	(10 * 1024 * 1024)
#define	DATABASE	"quote.db"
#define	SLEEPTIME	3

void *connect_all __P((void *args));
void *connect_thread __P((void *args));
int doclient __P((DB_ENV *, const char *, machtab_t *));
int domaster __P((DB_ENV *, const char *));
int get_accepted_socket __P((const char *, int));
int get_connected_socket __P((machtab_t *, const char *, const char *, int, int *, int *));
int get_next_message __P((int, DBT *, DBT *));
int listen_socket_init __P((const char *, int));
int listen_socket_accept __P((machtab_t *, const char *, int, int *));
int machtab_getinfo __P((machtab_t *, int, u_int32_t *, int *));
int machtab_init __P((machtab_t **, int, int));
void machtab_parm __P((machtab_t *, int *, int *, u_int32_t *));
int machtab_rem __P((machtab_t *, int, int));
int quote_send __P((DB_ENV *, const DBT *, const DBT *, int, u_int32_t));

#ifndef COMPQUIET
#define	COMPQUIET(x,y)	x = (y)
#endif

#endif /* !_EX_REPQUOTE_H_ */
