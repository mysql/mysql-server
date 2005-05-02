/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_rw.c,v 11.24 2002/07/12 18:56:52 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"

/*
 * __os_io --
 *	Do an I/O.
 *
 * PUBLIC: int __os_io __P((DB_ENV *, DB_IO *, int, size_t *));
 */
int
__os_io(dbenv, db_iop, op, niop)
	DB_ENV *dbenv;
	DB_IO *db_iop;
	int op;
	size_t *niop;
{
	int ret;

#if defined(HAVE_PREAD) && defined(HAVE_PWRITE)
	switch (op) {
	case DB_IO_READ:
		if (DB_GLOBAL(j_read) != NULL)
			goto slow;
		*niop = pread(db_iop->fhp->fd, db_iop->buf,
		    db_iop->bytes, (off_t)db_iop->pgno * db_iop->pagesize);
		break;
	case DB_IO_WRITE:
		if (DB_GLOBAL(j_write) != NULL)
			goto slow;
		*niop = pwrite(db_iop->fhp->fd, db_iop->buf,
		    db_iop->bytes, (off_t)db_iop->pgno * db_iop->pagesize);
		break;
	}
	if (*niop == (size_t)db_iop->bytes)
		return (0);
slow:
#endif
	MUTEX_THREAD_LOCK(dbenv, db_iop->mutexp);

	if ((ret = __os_seek(dbenv, db_iop->fhp,
	    db_iop->pagesize, db_iop->pgno, 0, 0, DB_OS_SEEK_SET)) != 0)
		goto err;
	switch (op) {
	case DB_IO_READ:
		ret = __os_read(dbenv,
		    db_iop->fhp, db_iop->buf, db_iop->bytes, niop);
		break;
	case DB_IO_WRITE:
		ret = __os_write(dbenv,
		    db_iop->fhp, db_iop->buf, db_iop->bytes, niop);
		break;
	}

err:	MUTEX_THREAD_UNLOCK(dbenv, db_iop->mutexp);

	return (ret);

}

/*
 * __os_read --
 *	Read from a file handle.
 *
 * PUBLIC: int __os_read __P((DB_ENV *, DB_FH *, void *, size_t, size_t *));
 */
int
__os_read(dbenv, fhp, addr, len, nrp)
	DB_ENV *dbenv;
	DB_FH *fhp;
	void *addr;
	size_t len;
	size_t *nrp;
{
	size_t offset;
	ssize_t nr;
	int ret;
	u_int8_t *taddr;

	for (taddr = addr,
	    offset = 0; offset < len; taddr += nr, offset += nr) {
retry:		if ((nr = DB_GLOBAL(j_read) != NULL ?
		    DB_GLOBAL(j_read)(fhp->fd, taddr, len - offset) :
		    read(fhp->fd, taddr, len - offset)) < 0) {
			if ((ret = __os_get_errno()) == EINTR)
				goto retry;
			__db_err(dbenv, "read: 0x%x, %lu: %s", taddr,
			    (u_long)len-offset, strerror(ret));
			return (ret);
		}
		if (nr == 0)
			break;
	}
	*nrp = taddr - (u_int8_t *)addr;
	return (0);
}

/*
 * __os_write --
 *	Write to a file handle.
 *
 * PUBLIC: int __os_write __P((DB_ENV *, DB_FH *, void *, size_t, size_t *));
 */
int
__os_write(dbenv, fhp, addr, len, nwp)
	DB_ENV *dbenv;
	DB_FH *fhp;
	void *addr;
	size_t len;
	size_t *nwp;
{
	size_t offset;
	ssize_t nw;
	int ret;
	u_int8_t *taddr;

	for (taddr = addr,
	    offset = 0; offset < len; taddr += nw, offset += nw)
retry:		if ((nw = DB_GLOBAL(j_write) != NULL ?
		    DB_GLOBAL(j_write)(fhp->fd, taddr, len - offset) :
		    write(fhp->fd, taddr, len - offset)) < 0) {
			if ((ret = __os_get_errno()) == EINTR)
				goto retry;
			__db_err(dbenv, "write: 0x%x, %lu: %s", taddr,
			    (u_long)len-offset, strerror(ret));
			return (ret);
		}
	*nwp = len;
	return (0);
}
