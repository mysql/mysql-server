/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: RpcDbTxn.java,v 1.9 2004/05/04 13:45:33 sue Exp $
 */

package com.sleepycat.db.rpcserver;

import com.sleepycat.db.*;
import com.sleepycat.db.internal.DbConstants;
import java.io.*;
import java.util.*;

/**
 * RPC wrapper around a txn object for the Java RPC server.
 */
public class RpcDbTxn extends Timer {
    RpcDbEnv rdbenv;
    Transaction txn;

    public RpcDbTxn(RpcDbEnv rdbenv, Transaction txn) {
        this.rdbenv = rdbenv;
        this.txn = txn;
    }

    void dispose() {
        if (txn != null) {
            try {
                txn.abort();
            } catch (DatabaseException e) {
                e.printStackTrace(Server.err);
            }
            txn = null;
        }
    }

    public  void abort(Dispatcher server,
                       __txn_abort_msg args, __txn_abort_reply reply) {
        try {
            txn.abort();
            txn = null;
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        } finally {
            server.delTxn(this, false);
        }
    }

    public  void begin(Dispatcher server,
                       __txn_begin_msg args, __txn_begin_reply reply) {
        try {
            if (rdbenv == null) {
                reply.status = DbConstants.DB_NOSERVER_ID;
                return;
            }
            Environment dbenv = rdbenv.dbenv;
            RpcDbTxn rparent = server.getTxn(args.parentcl_id);
            Transaction parent = (rparent != null) ? rparent.txn : null;

            TransactionConfig config = new TransactionConfig();
            config.setDegree2((args.flags & DbConstants.DB_DEGREE_2) != 0);
            config.setDirtyRead((args.flags & DbConstants.DB_DIRTY_READ) != 0);
            config.setNoSync((args.flags & DbConstants.DB_TXN_NOSYNC) != 0);
            config.setNoWait(true);
            config.setSync((args.flags & DbConstants.DB_TXN_SYNC) != 0);

            txn = dbenv.beginTransaction(parent, config);

            if (rparent != null)
                timer = rparent.timer;
            reply.txnidcl_id = server.addTxn(this);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void commit(Dispatcher server,
                        __txn_commit_msg args, __txn_commit_reply reply) {
        try {
            switch(args.flags) {
            case 0:
                txn.commit();
                break;

            case DbConstants.DB_TXN_SYNC:
                txn.commitSync();
                break;

            case DbConstants.DB_TXN_NOSYNC:
                txn.commitSync();
                break;

            default:
                throw new UnsupportedOperationException("Unknown flag: " + (args.flags & ~Server.DB_MODIFIER_MASK));
            }
            txn = null;
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        } finally {
            server.delTxn(this, false);
        }
    }

    public  void discard(Dispatcher server,
                         __txn_discard_msg args, __txn_discard_reply reply) {
        try {
            txn.discard(/* args.flags == 0 */);
            txn = null;
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        } finally {
            server.delTxn(this, false);
        }
    }

    public  void prepare(Dispatcher server,
                         __txn_prepare_msg args, __txn_prepare_reply reply) {
        try {
            txn.prepare(args.gid);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }
}
