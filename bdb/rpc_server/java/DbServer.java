/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbServer.java,v 1.5 2002/08/09 01:56:09 bostic Exp $
 */

package com.sleepycat.db.rpcserver;

import com.sleepycat.db.*;
import java.io.*;
import java.util.*;
import org.acplt.oncrpc.OncRpcException;
import org.acplt.oncrpc.server.OncRpcCallInformation;

/**
 * Main entry point for the Java version of the Berkeley DB RPC server
 */
public class DbServer extends DbDispatcher
{
	public static long idleto = 10 * 60 * 1000;	// 5 minutes
	public static long defto = 5 * 60 * 1000;	// 5 minutes
	public static long maxto = 60 * 60 * 1000;	// 1 hour
	public static String passwd = null;
	public static PrintWriter err;

	long now, hint; // updated each operation
	FreeList env_list = new FreeList();
	FreeList db_list = new FreeList();
	FreeList txn_list = new FreeList();
	FreeList cursor_list = new FreeList();

	public DbServer() throws IOException, OncRpcException
	{
		super();
		init_lists();
	}

	public void dispatchOncRpcCall(OncRpcCallInformation call, int program,
		int version, int procedure) throws OncRpcException, IOException
	{
		long newnow = System.currentTimeMillis();
		// DbServer.err.println("Dispatching RPC call " + procedure + " after delay of " + (newnow - now));
		now = newnow;
		// DbServer.err.flush();
		super.dispatchOncRpcCall(call, program, version, procedure);

		try {
			doTimeouts();
		} catch(Throwable t) {
			System.err.println("Caught " + t + " during doTimeouts()");
			t.printStackTrace(System.err);
		}
	}

	// Internal methods to track context
	private void init_lists()
	{
		// We do this so that getEnv/Db/etc(0) == null
		env_list.add(null);
		db_list.add(null);
		txn_list.add(null);
		cursor_list.add(null);
	}

	int addEnv(RpcDbEnv rdbenv)
	{
		rdbenv.timer.last_access = now;
		int id = env_list.add(rdbenv);
		return id;
	}

	int addDb(RpcDb rdb)
	{
		int id = db_list.add(rdb);
		return id;
	}

	int addTxn(RpcDbTxn rtxn)
	{
		rtxn.timer.last_access = now;
		int id = txn_list.add(rtxn);
		return id;
	}

	int addCursor(RpcDbc rdbc)
	{
		rdbc.timer.last_access = now;
		int id = cursor_list.add(rdbc);
		return id;
	}

	void delEnv(RpcDbEnv rdbenv)
	{
		// cursors and transactions will already have been cleaned up
		for(LocalIterator i = db_list.iterator(); i.hasNext(); ) {
			RpcDb rdb = (RpcDb)i.next();
			if (rdb != null && rdb.rdbenv == rdbenv)
				delDb(rdb);
		}

		env_list.del(rdbenv);
		rdbenv.dispose();
	}

	void delDb(RpcDb rdb)
	{
		db_list.del(rdb);
		rdb.dispose();

		for(LocalIterator i = cursor_list.iterator(); i.hasNext(); ) {
			RpcDbc rdbc = (RpcDbc)i.next();
			if (rdbc != null && rdbc.timer == rdb)
				i.remove();
		}
	}

	void delTxn(RpcDbTxn rtxn)
	{
		txn_list.del(rtxn);
		rtxn.dispose();

		for(LocalIterator i = cursor_list.iterator(); i.hasNext(); ) {
			RpcDbc rdbc = (RpcDbc)i.next();
			if (rdbc != null && rdbc.timer == rtxn)
				i.remove();
		}

		for(LocalIterator i = txn_list.iterator(); i.hasNext(); ) {
			RpcDbTxn rtxn_child = (RpcDbTxn)i.next();
			if (rtxn_child != null && rtxn_child.timer == rtxn)
				i.remove();
		}
	}

	void delCursor(RpcDbc rdbc)
	{
		cursor_list.del(rdbc);
		rdbc.dispose();
	}

	RpcDbEnv getEnv(int envid)
	{
		RpcDbEnv rdbenv = (RpcDbEnv)env_list.get(envid);
		if (rdbenv != null)
			rdbenv.timer.last_access = now;
		return rdbenv;
	}

	RpcDb getDb(int dbid)
	{
		RpcDb rdb = (RpcDb)db_list.get(dbid);
		if (rdb != null)
			rdb.rdbenv.timer.last_access = now;
		return rdb;
	}

	RpcDbTxn getTxn(int txnid)
	{
		RpcDbTxn rtxn = (RpcDbTxn)txn_list.get(txnid);
		if (rtxn != null)
			rtxn.timer.last_access = rtxn.rdbenv.timer.last_access = now;
		return rtxn;
	}

	RpcDbc getCursor(int dbcid)
	{
		RpcDbc rdbc = (RpcDbc)cursor_list.get(dbcid);
		if (rdbc != null)
			rdbc.last_access = rdbc.timer.last_access = rdbc.rdbenv.timer.last_access = now;
		return rdbc;
	}

	void doTimeouts()
	{
		if (now < hint) {
			// DbServer.err.println("Skipping cleaner sweep - now = " + now + ", hint = " + hint);
			return;
		}

		// DbServer.err.println("Starting a cleaner sweep");
		hint = now + DbServer.maxto;

		for(LocalIterator i = cursor_list.iterator(); i.hasNext(); ) {
			RpcDbc rdbc = (RpcDbc)i.next();
			if (rdbc == null)
				continue;

			long end_time = rdbc.timer.last_access + rdbc.rdbenv.timeout;
			// DbServer.err.println("Examining " + rdbc + ", time left = " + (end_time - now));
			if (end_time < now) {
				DbServer.err.println("Cleaning up " + rdbc);
				delCursor(rdbc);
			} else if (end_time < hint)
				hint = end_time;
		}

		for(LocalIterator i = txn_list.iterator(); i.hasNext(); ) {
			RpcDbTxn rtxn = (RpcDbTxn)i.next();
			if (rtxn == null)
				continue;

			long end_time = rtxn.timer.last_access + rtxn.rdbenv.timeout;
			// DbServer.err.println("Examining " + rtxn + ", time left = " + (end_time - now));
			if (end_time < now) {
				DbServer.err.println("Cleaning up " + rtxn);
				delTxn(rtxn);
			} else if (end_time < hint)
				hint = end_time;
		}

		for(LocalIterator i = env_list.iterator(); i.hasNext(); ) {
			RpcDbEnv rdbenv = (RpcDbEnv)i.next();
			if (rdbenv == null)
				continue;

			long end_time = rdbenv.timer.last_access + rdbenv.idletime;
			// DbServer.err.println("Examining " + rdbenv + ", time left = " + (end_time - now));
			if (end_time < now) {
				DbServer.err.println("Cleaning up " + rdbenv);
				delEnv(rdbenv);
			}
		}

		 // if we didn't find anything, reset the hint
		if (hint == now + DbServer.maxto)
			hint = 0;

		// DbServer.err.println("Finishing a cleaner sweep");
	}

	// Some constants that aren't available elsewhere
	static final int DB_SERVER_FLAGMASK = Db.DB_LOCKDOWN |
	    Db.DB_PRIVATE | Db.DB_RECOVER | Db.DB_RECOVER_FATAL |
	    Db.DB_SYSTEM_MEM | Db.DB_USE_ENVIRON |
	    Db.DB_USE_ENVIRON_ROOT;
	static final int DB_SERVER_ENVFLAGS = Db.DB_INIT_CDB |
	    Db.DB_INIT_LOCK | Db.DB_INIT_LOG | Db.DB_INIT_MPOOL |
	    Db.DB_INIT_TXN | Db.DB_JOINENV;
	static final int DB_SERVER_DBFLAGS = Db.DB_DIRTY_READ |
	    Db.DB_NOMMAP | Db.DB_RDONLY;
	static final int DB_SERVER_DBNOSHARE = Db.DB_EXCL | Db.DB_TRUNCATE;

	public static void main(String[] args)
	{
		System.out.println("Starting DbServer...");
		for (int i = 0; i < args.length; i++) {
			if (args[i].charAt(0) != '-')
				usage();

			switch (args[i].charAt(1)) {
			case 'h':
				++i; // add_home(args[++i]);
				break;
			case 'I':
				idleto = Long.parseLong(args[++i]) * 1000L;
				break;
			case 'P':
				passwd = args[++i];
				break;
			case 't':
				defto = Long.parseLong(args[++i]) * 1000L;
				break;
			case 'T':
				maxto = Long.parseLong(args[++i]) * 1000L;
				break;
			case 'V':
				// version;
				break;
			case 'v':
				// verbose
				break;
			default:
				usage();
			}
		}

		try {
			DbServer.err = new PrintWriter(new FileOutputStream("JavaRPCServer.trace", true));
			DbServer server = new DbServer();
			server.run();
		} catch (Throwable e) {
			System.out.println("DbServer exception:");
			e.printStackTrace(DbServer.err);
		} finally {
			if (DbServer.err != null)
				DbServer.err.close();
		}

		System.out.println("DbServer stopped.");
	}

	static void usage()
	{
		System.err.println("usage: java com.sleepycat.db.rpcserver.DbServer \\");
		System.err.println("[-Vv] [-h home] [-P passwd] [-I idletimeout] [-L logfile] [-t def_timeout] [-T maxtimeout]");
		System.exit(1);
	}
}
