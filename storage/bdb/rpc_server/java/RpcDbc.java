/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: RpcDbc.java,v 1.3 2002/08/09 01:56:10 bostic Exp $
 */

package com.sleepycat.db.rpcserver;

import com.sleepycat.db.*;
import java.io.IOException;
import java.io.*;
import java.util.*;

/**
 * RPC wrapper around a dbc object for the Java RPC server.
 */
public class RpcDbc extends Timer
{
	static final byte[] empty = new byte[0];
	RpcDbEnv rdbenv;
	RpcDb rdb;
	Dbc dbc;
	Timer orig_timer;
	boolean isJoin;

	public RpcDbc(RpcDb rdb, Dbc dbc, boolean isJoin)
	{
		this.rdb = rdb;
		this.rdbenv = rdb.rdbenv;
		this.dbc = dbc;
		this.isJoin = isJoin;
	}

	void dispose()
	{
		if (dbc != null) {
			try {
				dbc.close();
			} catch(DbException e) {
				e.printStackTrace(DbServer.err);
			}
			dbc = null;
		}
	}

	public  void close(DbDispatcher server,
		__dbc_close_msg args, __dbc_close_reply reply)
	{
		try {
			dbc.close();
			dbc = null;

			if (isJoin)
				for(LocalIterator i = ((DbServer)server).cursor_list.iterator(); i.hasNext(); ) {
					RpcDbc rdbc = (RpcDbc)i.next();
					// Unjoin cursors that were joined to create this
					if (rdbc != null && rdbc.timer == this)
						rdbc.timer = rdbc.orig_timer;
				}

			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.get_errno();
		} finally {
			server.delCursor(this);
		}
	}

	public  void count(DbDispatcher server,
		__dbc_count_msg args, __dbc_count_reply reply)
	{
		try {
			reply.dupcount = dbc.count(args.flags);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.get_errno();
		}
	}

	public  void del(DbDispatcher server,
		__dbc_del_msg args, __dbc_del_reply reply)
	{
		try {
			reply.status = dbc.del(args.flags);
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.get_errno();
		}
	}

	public  void dup(DbDispatcher server,
		__dbc_dup_msg args, __dbc_dup_reply reply)
	{
		try {
			Dbc newdbc = dbc.dup(args.flags);
			RpcDbc rdbc = new RpcDbc(rdb, newdbc, false);
			/* If this cursor has a parent txn, we need to use it too. */
			if (timer != this)
				rdbc.timer = timer;
			reply.dbcidcl_id = server.addCursor(rdbc);
			reply.status = 0;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.get_errno();
		}
	}

	public  void get(DbDispatcher server,
		__dbc_get_msg args, __dbc_get_reply reply)
	{
		try {
			Dbt key = new Dbt(args.keydata);
			key.set_dlen(args.keydlen);
			key.set_ulen(args.keyulen);
			key.set_doff(args.keydoff);
			key.set_flags(Db.DB_DBT_MALLOC |
			    (args.keyflags & Db.DB_DBT_PARTIAL));

			Dbt data = new Dbt(args.datadata);
			data.set_dlen(args.datadlen);
			data.set_ulen(args.dataulen);
			data.set_doff(args.datadoff);
			if ((args.flags & Db.DB_MULTIPLE) != 0 ||
			    (args.flags & Db.DB_MULTIPLE_KEY) != 0) {
				if (data.get_data().length == 0)
					data.set_data(new byte[data.get_ulen()]);
				data.set_flags(Db.DB_DBT_USERMEM |
				    (args.dataflags & Db.DB_DBT_PARTIAL));
			} else
				data.set_flags(Db.DB_DBT_MALLOC |
				    (args.dataflags & Db.DB_DBT_PARTIAL));

			reply.status = dbc.get(key, data, args.flags);

			if (key.get_data() == args.keydata) {
				reply.keydata = new byte[key.get_size()];
				System.arraycopy(key.get_data(), 0, reply.keydata, 0, key.get_size());
			} else
				reply.keydata = key.get_data();

			if (data.get_data() == args.datadata) {
				reply.datadata = new byte[data.get_size()];
				System.arraycopy(data.get_data(), 0, reply.datadata, 0, data.get_size());
			} else
				reply.datadata = data.get_data();
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.get_errno();
			reply.keydata = reply.datadata = empty;
		}
	}

	public  void pget(DbDispatcher server,
		__dbc_pget_msg args, __dbc_pget_reply reply)
	{
		try {
			Dbt skey = new Dbt(args.skeydata);
			skey.set_dlen(args.skeydlen);
			skey.set_doff(args.skeydoff);
			skey.set_ulen(args.skeyulen);
			skey.set_flags(Db.DB_DBT_MALLOC |
			    (args.skeyflags & Db.DB_DBT_PARTIAL));

			Dbt pkey = new Dbt(args.pkeydata);
			pkey.set_dlen(args.pkeydlen);
			pkey.set_doff(args.pkeydoff);
			pkey.set_ulen(args.pkeyulen);
			pkey.set_flags(Db.DB_DBT_MALLOC |
			    (args.pkeyflags & Db.DB_DBT_PARTIAL));

			Dbt data = new Dbt(args.datadata);
			data.set_dlen(args.datadlen);
			data.set_doff(args.datadoff);
			data.set_ulen(args.dataulen);
			data.set_flags(Db.DB_DBT_MALLOC |
			    (args.dataflags & Db.DB_DBT_PARTIAL));

			reply.status = dbc.pget(skey, pkey, data, args.flags);

			if (skey.get_data() == args.skeydata) {
				reply.skeydata = new byte[skey.get_size()];
				System.arraycopy(skey.get_data(), 0, reply.skeydata, 0, skey.get_size());
			} else
				reply.skeydata = skey.get_data();

			if (pkey.get_data() == args.pkeydata) {
				reply.pkeydata = new byte[pkey.get_size()];
				System.arraycopy(pkey.get_data(), 0, reply.pkeydata, 0, pkey.get_size());
			} else
				reply.pkeydata = pkey.get_data();

			if (data.get_data() == args.datadata) {
				reply.datadata = new byte[data.get_size()];
				System.arraycopy(data.get_data(), 0, reply.datadata, 0, data.get_size());
			} else
				reply.datadata = data.get_data();
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.get_errno();
		}
	}

	public  void put(DbDispatcher server,
		__dbc_put_msg args, __dbc_put_reply reply)
	{
		try {
			Dbt key = new Dbt(args.keydata);
			key.set_dlen(args.keydlen);
			key.set_ulen(args.keyulen);
			key.set_doff(args.keydoff);
			key.set_flags(args.keyflags & Db.DB_DBT_PARTIAL);

			Dbt data = new Dbt(args.datadata);
			data.set_dlen(args.datadlen);
			data.set_ulen(args.dataulen);
			data.set_doff(args.datadoff);
			data.set_flags(args.dataflags);

			reply.status = dbc.put(key, data, args.flags);

			if (reply.status == 0 &&
			    (args.flags == Db.DB_AFTER || args.flags == Db.DB_BEFORE) &&
			    rdb.db.get_type() == Db.DB_RECNO)
				reply.keydata = key.get_data();
			else
				reply.keydata = empty;
		} catch(DbException e) {
			e.printStackTrace(DbServer.err);
			reply.status = e.get_errno();
			reply.keydata = empty;
		}
	}
}
