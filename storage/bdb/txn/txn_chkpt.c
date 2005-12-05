/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2005
 *	Sleepycat Software.  All rights reserved.
 */
/*
 * Copyright (c) 1995, 1996
 *	The President and Fellows of Harvard University.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: txn_chkpt.c,v 12.19 2005/10/20 18:57:13 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <stdlib.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"

/*
 * __txn_checkpoint_pp --
 *	DB_ENV->txn_checkpoint pre/post processing.
 *
 * PUBLIC: int __txn_checkpoint_pp
 * PUBLIC:     __P((DB_ENV *, u_int32_t, u_int32_t, u_int32_t));
 */
int
__txn_checkpoint_pp(dbenv, kbytes, minutes, flags)
	DB_ENV *dbenv;
	u_int32_t kbytes, minutes, flags;
{
	DB_THREAD_INFO *ip;
	int ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->tx_handle, "txn_checkpoint", DB_INIT_TXN);

	/*
	 * On a replication client, all transactions are read-only; therefore,
	 * a checkpoint is a null-op.
	 *
	 * We permit txn_checkpoint, instead of just rendering it illegal,
	 * so that an application can just let a checkpoint thread continue
	 * to operate as it gets promoted or demoted between being a
	 * master and a client.
	 */
	if (IS_REP_CLIENT(dbenv))
		return (0);

	ENV_ENTER(dbenv, ip);
	REPLICATION_WRAP(dbenv,
	    (__txn_checkpoint(dbenv, kbytes, minutes, flags)), ret);
	ENV_LEAVE(dbenv, ip);
	return (ret);
}

/*
 * __txn_checkpoint --
 *	DB_ENV->txn_checkpoint.
 *
 * PUBLIC: int __txn_checkpoint
 * PUBLIC:	__P((DB_ENV *, u_int32_t, u_int32_t, u_int32_t));
 */
int
__txn_checkpoint(dbenv, kbytes, minutes, flags)
	DB_ENV *dbenv;
	u_int32_t kbytes, minutes, flags;
{
	DB_LSN ckp_lsn, last_ckp;
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	REGENV *renv;
	REGINFO *infop;
	time_t last_ckp_time, now;
	u_int32_t bytes, gen, id, logflags, mbytes;
	int ret;

	ret = gen = 0;
	/*
	 * A client will only call through here during recovery,
	 * so just sync the Mpool and go home.
	 */
	if (IS_REP_CLIENT(dbenv)) {
		if (MPOOL_ON(dbenv) && (ret = __memp_sync(dbenv, NULL)) != 0) {
			__db_err(dbenv,
		    "txn_checkpoint: failed to flush the buffer cache %s",
			    db_strerror(ret));
			return (ret);
		} else
			return (0);
	}

	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;
	infop = dbenv->reginfo;
	renv = infop->primary;
	/*
	 * No mutex is needed as envid is read-only once it is set.
	 */
	id = renv->envid;

	/*
	 * The checkpoint LSN is an LSN such that all transactions begun before
	 * it are complete.  Our first guess (corrected below based on the list
	 * of active transactions) is the last-written LSN.
	 */
	if ((ret = __log_current_lsn(dbenv, &ckp_lsn, &mbytes, &bytes)) != 0)
		return (ret);

	if (!LF_ISSET(DB_FORCE)) {
		/* Don't checkpoint a quiescent database. */
		if (bytes == 0 && mbytes == 0)
			return (0);

		/*
		 * If either kbytes or minutes is non-zero, then only take the
		 * checkpoint if more than "minutes" minutes have passed or if
		 * more than "kbytes" of log data have been written since the
		 * last checkpoint.
		 */
		if (kbytes != 0 &&
		    mbytes * 1024 + bytes / 1024 >= (u_int32_t)kbytes)
			goto do_ckp;

		if (minutes != 0) {
			(void)time(&now);

			TXN_SYSTEM_LOCK(dbenv);
			last_ckp_time = region->time_ckp;
			TXN_SYSTEM_UNLOCK(dbenv);

			if (now - last_ckp_time >= (time_t)(minutes * 60))
				goto do_ckp;
		}

		/*
		 * If we checked time and data and didn't go to checkpoint,
		 * we're done.
		 */
		if (minutes != 0 || kbytes != 0)
			return (0);
	}

	/*
	 * We must single thread checkpoints otherwise the chk_lsn may get out
	 * of order.  We need to capture the start of the earliest currently
	 * active transaction (chk_lsn) and then flush all buffers.  While
	 * doing this we we could then be overtaken by another checkpoint that
	 * sees a later chk_lsn but competes first.  An archive process could
	 * then remove a log this checkpoint depends on.
	 */
do_ckp:	MUTEX_LOCK(dbenv, region->mtx_ckp);
	if ((ret = __txn_getactive(dbenv, &ckp_lsn)) != 0)
		goto err;

	if (MPOOL_ON(dbenv) && (ret = __memp_sync(dbenv, NULL)) != 0) {
		__db_err(dbenv,
		    "txn_checkpoint: failed to flush the buffer cache %s",
		    db_strerror(ret));
		goto err;
	}

	/*
	 * Because we can't be a replication client here, and because
	 * recovery (somewhat unusually) calls txn_checkpoint and expects
	 * it to write a log message, LOGGING_ON is the correct macro here.
	 */
	if (LOGGING_ON(dbenv)) {
		TXN_SYSTEM_LOCK(dbenv);
		last_ckp = region->last_ckp;
		TXN_SYSTEM_UNLOCK(dbenv);
		if (REP_ON(dbenv) && (ret = __rep_get_gen(dbenv, &gen)) != 0)
			goto err;

		/*
		 * Put out records for the open files before we log
		 * the checkpoint.  The records are certain to be at
		 * or after ckp_lsn, but before the checkpoint record
		 * itself, so they're sure to be included if we start
		 * recovery from the ckp_lsn contained in this
		 * checkpoint.
		 */
		logflags = DB_LOG_PERM | DB_LOG_CHKPNT;
		if (!IS_RECOVERING(dbenv))
			logflags |= DB_FLUSH;
		if ((ret = __dbreg_log_files(dbenv)) != 0 ||
		    (ret = __txn_ckp_log(dbenv, NULL, &ckp_lsn, logflags,
		    &ckp_lsn, &last_ckp, (int32_t)time(NULL), id, gen)) != 0) {
			__db_err(dbenv,
			    "txn_checkpoint: log failed at LSN [%ld %ld] %s",
			    (long)ckp_lsn.file, (long)ckp_lsn.offset,
			    db_strerror(ret));
			goto err;
		}

		if ((ret = __txn_updateckp(dbenv, &ckp_lsn)) != 0)
			goto err;
	}

err:	MUTEX_UNLOCK(dbenv, region->mtx_ckp);
	return (ret);
}

/*
 * __txn_getactive --
 *	 Find the oldest active transaction and figure out its "begin" LSN.
 *	 This is the lowest LSN we can checkpoint, since any record written
 *	 after it may be involved in a transaction and may therefore need
 *	 to be undone in the case of an abort.
 *
 *	 We check both the file and offset for 0 since the lsn may be in
 *	 transition.  If it is then we don't care about this txn because it
 *	 must be starting after we set the initial value of lsnp in the caller.
 *	 All txns must initalize their begin_lsn before writing to the log.
 *
 * PUBLIC: int __txn_getactive __P((DB_ENV *, DB_LSN *));
 */
int
__txn_getactive(dbenv, lsnp)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
{
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	TXN_DETAIL *td;

	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;

	TXN_SYSTEM_LOCK(dbenv);
	for (td = SH_TAILQ_FIRST(&region->active_txn, __txn_detail);
	    td != NULL;
	    td = SH_TAILQ_NEXT(td, links, __txn_detail))
		if (td->begin_lsn.file != 0 &&
		    td->begin_lsn.offset != 0 &&
		    log_compare(&td->begin_lsn, lsnp) < 0)
			*lsnp = td->begin_lsn;
	TXN_SYSTEM_UNLOCK(dbenv);

	return (0);
}

/*
 * __txn_getckp --
 *	Get the LSN of the last transaction checkpoint.
 *
 * PUBLIC: int __txn_getckp __P((DB_ENV *, DB_LSN *));
 */
int
__txn_getckp(dbenv, lsnp)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
{
	DB_LSN lsn;
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;

	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;

	TXN_SYSTEM_LOCK(dbenv);
	lsn = region->last_ckp;
	TXN_SYSTEM_UNLOCK(dbenv);

	if (IS_ZERO_LSN(lsn))
		return (DB_NOTFOUND);

	*lsnp = lsn;
	return (0);
}

/*
 * __txn_updateckp --
 *	Update the last_ckp field in the transaction region.  This happens
 * at the end of a normal checkpoint and also when a replication client
 * receives a checkpoint record.
 *
 * PUBLIC: int __txn_updateckp __P((DB_ENV *, DB_LSN *));
 */
int
__txn_updateckp(dbenv, lsnp)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
{
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;

	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;

	/*
	 * We want to make sure last_ckp only moves forward;  since we drop
	 * locks above and in log_put, it's possible for two calls to
	 * __txn_ckp_log to finish in a different order from how they were
	 * called.
	 */
	TXN_SYSTEM_LOCK(dbenv);
	if (log_compare(&region->last_ckp, lsnp) < 0) {
		region->last_ckp = *lsnp;
		(void)time(&region->time_ckp);
	}
	TXN_SYSTEM_UNLOCK(dbenv);

	return (0);
}
