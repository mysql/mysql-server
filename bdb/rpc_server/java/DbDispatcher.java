/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: DbDispatcher.java,v 1.5 2002/08/09 01:56:08 bostic Exp $
 */

package com.sleepycat.db.rpcserver;

import com.sleepycat.db.*;
import java.io.IOException;
import org.acplt.oncrpc.OncRpcException;

/**
 * Dispatcher for RPC messages for the Java RPC server.
 * These are hooks that translate between RPC msg/reply structures and
 * DB calls, which keeps the real implementation code in Rpc* classes cleaner.
 */
public abstract class DbDispatcher extends DbServerStub
{
	abstract int addEnv(RpcDbEnv rdbenv);
	abstract int addDb(RpcDb rdb);
	abstract int addTxn(RpcDbTxn rtxn);
	abstract int addCursor(RpcDbc rdbc);
	abstract void delEnv(RpcDbEnv rdbenv);
	abstract void delDb(RpcDb rdb);
	abstract void delTxn(RpcDbTxn rtxn);
	abstract void delCursor(RpcDbc rdbc);
	abstract RpcDbEnv getEnv(int envid);
	abstract RpcDb getDb(int dbid);
	abstract RpcDbTxn getTxn(int txnbid);
	abstract RpcDbc getCursor(int dbcid);

	public DbDispatcher() throws IOException, OncRpcException
	{
		super();
	}

	//// Db methods

	public  __db_associate_reply __DB_db_associate_4001(__db_associate_msg args)
	{
		__db_associate_reply reply = new __db_associate_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.associate(this, args, reply);
		return reply;
	}

	public  __db_bt_maxkey_reply __DB_db_bt_maxkey_4001(__db_bt_maxkey_msg args)
	{
		__db_bt_maxkey_reply reply = new __db_bt_maxkey_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.set_bt_maxkey(this, args, reply);
		return reply;
	}

	public  __db_bt_minkey_reply __DB_db_bt_minkey_4001(__db_bt_minkey_msg args)
	{
		__db_bt_minkey_reply reply = new __db_bt_minkey_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.set_bt_minkey(this, args, reply);
		return reply;
	}

	public  __db_close_reply __DB_db_close_4001(__db_close_msg args)
	{
		__db_close_reply reply = new __db_close_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.close(this, args, reply);
		return reply;
	}

	public  __db_create_reply __DB_db_create_4001(__db_create_msg args)
	{
		__db_create_reply reply = new __db_create_reply();
		RpcDb rdb = new RpcDb(getEnv(args.dbenvcl_id));
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.create(this, args, reply);
		return reply;
	}

	public  __db_cursor_reply __DB_db_cursor_4001(__db_cursor_msg args)
	{
		__db_cursor_reply reply = new __db_cursor_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.cursor(this, args, reply);
		return reply;
	}

	public  __db_del_reply __DB_db_del_4001(__db_del_msg args)
	{
		__db_del_reply reply = new __db_del_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.del(this, args, reply);
		return reply;
	}

	public  __db_encrypt_reply __DB_db_encrypt_4001(__db_encrypt_msg args)
	{
		__db_encrypt_reply reply = new __db_encrypt_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.set_encrypt(this, args, reply);
		return reply;
	}

	public  __db_extentsize_reply __DB_db_extentsize_4001(__db_extentsize_msg args)
	{
		__db_extentsize_reply reply = new __db_extentsize_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.set_q_extentsize(this, args, reply);
		return reply;
	}

	public  __db_flags_reply __DB_db_flags_4001(__db_flags_msg args)
	{
		__db_flags_reply reply = new __db_flags_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.set_flags(this, args, reply);
		return reply;
	}

	public  __db_get_reply __DB_db_get_4001(__db_get_msg args)
	{
		__db_get_reply reply = new __db_get_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.get(this, args, reply);
		return reply;
	}

	public  __db_h_ffactor_reply __DB_db_h_ffactor_4001(__db_h_ffactor_msg args)
	{
		__db_h_ffactor_reply reply = new __db_h_ffactor_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.set_h_ffactor(this, args, reply);
		return reply;
	}

	public  __db_h_nelem_reply __DB_db_h_nelem_4001(__db_h_nelem_msg args)
	{
		__db_h_nelem_reply reply = new __db_h_nelem_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.set_h_nelem(this, args, reply);
		return reply;
	}

	public  __db_join_reply __DB_db_join_4001(__db_join_msg args)
	{
		__db_join_reply reply = new __db_join_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.join(this, args, reply);
		return reply;
	}

	public  __db_key_range_reply __DB_db_key_range_4001(__db_key_range_msg args)
	{
		__db_key_range_reply reply = new __db_key_range_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.key_range(this, args, reply);
		return reply;
	}

	public  __db_lorder_reply __DB_db_lorder_4001(__db_lorder_msg args)
	{
		__db_lorder_reply reply = new __db_lorder_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.set_lorder(this, args, reply);
		return reply;
	}

	public  __db_open_reply __DB_db_open_4001(__db_open_msg args)
	{
		__db_open_reply reply = new __db_open_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.open(this, args, reply);
		return reply;
	}

	public  __db_pagesize_reply __DB_db_pagesize_4001(__db_pagesize_msg args)
	{
		__db_pagesize_reply reply = new __db_pagesize_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.set_pagesize(this, args, reply);
		return reply;
	}

	public  __db_pget_reply __DB_db_pget_4001(__db_pget_msg args)
	{
		__db_pget_reply reply = new __db_pget_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.pget(this, args, reply);
		return reply;
	}

	public  __db_put_reply __DB_db_put_4001(__db_put_msg args)
	{
		__db_put_reply reply = new __db_put_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.put(this, args, reply);
		return reply;
	}

	public  __db_remove_reply __DB_db_remove_4001(__db_remove_msg args)
	{
		__db_remove_reply reply = new __db_remove_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.remove(this, args, reply);
		return reply;
	}

	public  __db_rename_reply __DB_db_rename_4001(__db_rename_msg args)
	{
		__db_rename_reply reply = new __db_rename_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.rename(this, args, reply);
		return reply;
	}

	public  __db_re_delim_reply __DB_db_re_delim_4001(__db_re_delim_msg args)
	{
		__db_re_delim_reply reply = new __db_re_delim_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.set_re_delim(this, args, reply);
		return reply;
	}

	public  __db_re_len_reply __DB_db_re_len_4001(__db_re_len_msg args)
	{
		__db_re_len_reply reply = new __db_re_len_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.set_re_len(this, args, reply);
		return reply;
	}

	public  __db_re_pad_reply __DB_db_re_pad_4001(__db_re_pad_msg args)
	{
		__db_re_pad_reply reply = new __db_re_pad_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.set_re_pad(this, args, reply);
		return reply;
	}

	public  __db_stat_reply __DB_db_stat_4001(__db_stat_msg args)
	{
		__db_stat_reply reply = new __db_stat_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.stat(this, args, reply);
		return reply;
	}

	public  __db_sync_reply __DB_db_sync_4001(__db_sync_msg args)
	{
		__db_sync_reply reply = new __db_sync_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.sync(this, args, reply);
		return reply;
	}

	public  __db_truncate_reply __DB_db_truncate_4001(__db_truncate_msg args)
	{
		__db_truncate_reply reply = new __db_truncate_reply();
		RpcDb rdb = getDb(args.dbpcl_id);
		if (rdb == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdb.truncate(this, args, reply);
		return reply;
	}

	//// Cursor methods

	public  __dbc_close_reply __DB_dbc_close_4001(__dbc_close_msg args)
	{
		__dbc_close_reply reply = new __dbc_close_reply();
		RpcDbc rdbc = getCursor(args.dbccl_id);
		if (rdbc == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbc.close(this, args, reply);
		return reply;
	}

	public  __dbc_count_reply __DB_dbc_count_4001(__dbc_count_msg args)
	{
		__dbc_count_reply reply = new __dbc_count_reply();
		RpcDbc rdbc = getCursor(args.dbccl_id);
		if (rdbc == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbc.count(this, args, reply);
		return reply;
	}

	public  __dbc_del_reply __DB_dbc_del_4001(__dbc_del_msg args)
	{
		__dbc_del_reply reply = new __dbc_del_reply();
		RpcDbc rdbc = getCursor(args.dbccl_id);
		if (rdbc == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbc.del(this, args, reply);
		return reply;
	}

	public  __dbc_dup_reply __DB_dbc_dup_4001(__dbc_dup_msg args)
	{
		__dbc_dup_reply reply = new __dbc_dup_reply();
		RpcDbc rdbc = getCursor(args.dbccl_id);
		if (rdbc == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbc.dup(this, args, reply);
		return reply;
	}

	public  __dbc_get_reply __DB_dbc_get_4001(__dbc_get_msg args)
	{
		__dbc_get_reply reply = new __dbc_get_reply();
		RpcDbc rdbc = getCursor(args.dbccl_id);
		if (rdbc == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbc.get(this, args, reply);
		return reply;
	}

	public  __dbc_pget_reply __DB_dbc_pget_4001(__dbc_pget_msg args) {
		__dbc_pget_reply reply = new __dbc_pget_reply();
		RpcDbc rdbc = getCursor(args.dbccl_id);
		if (rdbc == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbc.pget(this, args, reply);
		return reply;
	}

	public  __dbc_put_reply __DB_dbc_put_4001(__dbc_put_msg args) {
		__dbc_put_reply reply = new __dbc_put_reply();
		RpcDbc rdbc = getCursor(args.dbccl_id);
		if (rdbc == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbc.put(this, args, reply);
		return reply;
	}

	//// Environment methods

	public  __env_cachesize_reply __DB_env_cachesize_4001(__env_cachesize_msg args)
	{
		__env_cachesize_reply reply = new __env_cachesize_reply();
		RpcDbEnv rdbenv = getEnv(args.dbenvcl_id);
		if (rdbenv == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbenv.set_cachesize(this, args, reply);
		return reply;
	}

	public  __env_close_reply __DB_env_close_4001(__env_close_msg args)
	{
		__env_close_reply reply = new __env_close_reply();
		RpcDbEnv rdbenv = getEnv(args.dbenvcl_id);
		if (rdbenv == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbenv.close(this, args, reply);
		return reply;
	}

	public  __env_create_reply __DB_env_create_4001(__env_create_msg args)
	{
		__env_create_reply reply = new __env_create_reply();
		RpcDbEnv rdbenv = new RpcDbEnv();
		rdbenv.create(this, args, reply);
		return reply;
	}

	public  __env_dbremove_reply __DB_env_dbremove_4001(__env_dbremove_msg args)
	{
		__env_dbremove_reply reply = new __env_dbremove_reply();
		RpcDbEnv rdbenv = getEnv(args.dbenvcl_id);
		if (rdbenv == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbenv.dbremove(this, args, reply);
		return reply;
	}

	public  __env_dbrename_reply __DB_env_dbrename_4001(__env_dbrename_msg args)
	{
		__env_dbrename_reply reply = new __env_dbrename_reply();
		RpcDbEnv rdbenv = getEnv(args.dbenvcl_id);
		if (rdbenv == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbenv.dbrename(this, args, reply);
		return reply;
	}

	public  __env_encrypt_reply __DB_env_encrypt_4001(__env_encrypt_msg args)
	{
		__env_encrypt_reply reply = new __env_encrypt_reply();
		RpcDbEnv rdbenv = getEnv(args.dbenvcl_id);
		if (rdbenv == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbenv.set_encrypt(this, args, reply);
		return reply;
	}

	public  __env_flags_reply __DB_env_flags_4001(__env_flags_msg args)
	{
		__env_flags_reply reply = new __env_flags_reply();
		RpcDbEnv rdbenv = getEnv(args.dbenvcl_id);
		if (rdbenv == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbenv.set_flags(this, args, reply);
		return reply;
	}

	public  __env_open_reply __DB_env_open_4001(__env_open_msg args)
	{
		__env_open_reply reply = new __env_open_reply();
		RpcDbEnv rdbenv = getEnv(args.dbenvcl_id);
		if (rdbenv == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbenv.open(this, args, reply);
		return reply;
	}

	public  __env_remove_reply __DB_env_remove_4001(__env_remove_msg args)
	{
		__env_remove_reply reply = new __env_remove_reply();
		RpcDbEnv rdbenv = getEnv(args.dbenvcl_id);
		if (rdbenv == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbenv.remove(this, args, reply);
		return reply;
	}

	//// Transaction methods

	public  __txn_abort_reply __DB_txn_abort_4001(__txn_abort_msg args)
	{
		__txn_abort_reply reply = new __txn_abort_reply();
		RpcDbTxn rdbtxn = getTxn(args.txnpcl_id);
		if (rdbtxn == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbtxn.abort(this, args, reply);
		return reply;
	}

	public  __txn_begin_reply __DB_txn_begin_4001(__txn_begin_msg args)
	{
		__txn_begin_reply reply = new __txn_begin_reply();
		RpcDbTxn rdbtxn = new RpcDbTxn(getEnv(args.dbenvcl_id), null);
		rdbtxn.begin(this, args, reply);
		return reply;
	}

	public  __txn_commit_reply __DB_txn_commit_4001(__txn_commit_msg args)
	{
		__txn_commit_reply reply = new __txn_commit_reply();
		RpcDbTxn rdbtxn = getTxn(args.txnpcl_id);
		if (rdbtxn == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbtxn.commit(this, args, reply);
		return reply;
	}

	public  __txn_discard_reply __DB_txn_discard_4001(__txn_discard_msg args)
	{
		__txn_discard_reply reply = new __txn_discard_reply();
		RpcDbTxn rdbtxn = getTxn(args.txnpcl_id);
		if (rdbtxn == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbtxn.discard(this, args, reply);
		return reply;
	}

	public  __txn_prepare_reply __DB_txn_prepare_4001(__txn_prepare_msg args)
	{
		__txn_prepare_reply reply = new __txn_prepare_reply();
		RpcDbTxn rdbtxn = getTxn(args.txnpcl_id);
		if (rdbtxn == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbtxn.prepare(this, args, reply);
		return reply;
	}

	public  __txn_recover_reply __DB_txn_recover_4001(__txn_recover_msg args)
	{
		__txn_recover_reply reply = new __txn_recover_reply();
		RpcDbEnv rdbenv = getEnv(args.dbenvcl_id);
		if (rdbenv == null)
			reply.status = Db.DB_NOSERVER_ID;
		else
			rdbenv.txn_recover(this, args, reply);
		return reply;
	}
}
