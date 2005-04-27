/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: RpcDbTxn.java,v 1.2 2002/08/09 01:56:10 bostic Exp $
 */

package com.sleepycat.db.rpcserver;

import com.sleepycat.db.*;
import java.io.IOException;
import java.io.*;
import java.util.*;

/**
 * RPC wrapper around a txn object for the Java RPC server.
 */
public class RpcDbTxn extends Timer
{
	RpcDbEnv rdbenv;
	DbTxn txn;

	public RpcDbTxn(RpcDbEnv rdbenv, DbTxn txn)
	{
		this.rdbenv = rdbenv;
		this.txn = txn;
	}

	void dispose()
	{
		if (txn != null) {
			try {
				txn.abort();
			} catch(DbException e) {
				e.printStackTrace(DbServer.err);
			}
			txn = null;
		}
	}

	public  void abort(DbDispatcher server,
		__txn_abort_msg args, __txn_abort_reply reply)
	{
		try {
			txn.abort();
			txn = null;
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.get_errno();
		} finally {
			server.delTxn(this);
		}
	}

	public  void begin(DbDispatcher server,
		__txn_begin_msg args, __txn_begin_reply reply)
	{
		try {
			if (rdbenv == null) {
				reply.status = Db.DB_NOSERVER_ID;
				return;
			}
			DbEnv dbenv = rdbenv.dbenv;
			RpcDbTxn rparent = server.getTxn(args.parentcl_id);
			DbTxn parent = (rparent != null) ? rparent.txn : null;

			txn = dbenv.txn_begin(parent, args.flags);

			if (rparent != null)
				timer = rparent.timer;
			reply.txnidcl_id = server.addTxn(this);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.get_errno();
		}
	}

	public  void commit(DbDispatcher server,
		__txn_commit_msg args, __txn_commit_reply reply)
	{
		try {
			txn.commit(args.flags);
			txn = null;
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.get_errno();
		} finally {
			server.delTxn(this);
		}
	}

	public  void discard(DbDispatcher server,
		__txn_discard_msg args, __txn_discard_reply reply)
	{
		try {
			txn.discard(args.flags);
			txn = null;
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.get_errno();
		} finally {
			server.delTxn(this);
		}
	}

	public  void prepare(DbDispatcher server,
		__txn_prepare_msg args, __txn_prepare_reply reply)
	{
		try {
			txn.prepare(args.gid);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.get_errno();
		}
	}
}
