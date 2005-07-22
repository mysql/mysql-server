/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: RpcDbc.java,v 1.13 2004/11/05 01:08:31 mjc Exp $
 */

package com.sleepycat.db.rpcserver;

import com.sleepycat.db.*;
import com.sleepycat.db.internal.DbConstants;
import java.io.*;
import java.util.*;

/**
 * RPC wrapper around a dbc object for the Java RPC server.
 */
public class RpcDbc extends Timer {
    static final byte[] empty = new byte[0];
    RpcDbEnv rdbenv;
    RpcDb rdb;
    Cursor dbc;
    Timer orig_timer;
    boolean isJoin;

    public RpcDbc(RpcDb rdb, Cursor dbc, boolean isJoin) {
        this.rdb = rdb;
        this.rdbenv = rdb.rdbenv;
        this.dbc = dbc;
        this.isJoin = isJoin;
    }

    void dispose() {
        if (dbc != null) {
            try {
                dbc.close();
            } catch (Throwable t) {
                Util.handleException(t);
            }
            dbc = null;
        }
    }

    public  void close(Dispatcher server,
                       __dbc_close_msg args, __dbc_close_reply reply) {
        try {
            dbc.close();
            dbc = null;

            if (isJoin)
                for (LocalIterator i = ((Server)server).cursor_list.iterator(); i.hasNext();) {
                    RpcDbc rdbc = (RpcDbc)i.next();
                    // Unjoin cursors that were joined to create this
                    if (rdbc != null && rdbc.timer == this)
                        rdbc.timer = rdbc.orig_timer;
                }

            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        } finally {
            server.delCursor(this, false);
        }
    }

    public  void count(Dispatcher server,
                       __dbc_count_msg args, __dbc_count_reply reply) {
        try {
            reply.dupcount = dbc.count(/* args.flags == 0 */);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void del(Dispatcher server,
                     __dbc_del_msg args, __dbc_del_reply reply) {
        try {
            reply.status = Util.getStatus(dbc.delete(/* args.flags == 0 */));
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void dup(Dispatcher server,
                     __dbc_dup_msg args, __dbc_dup_reply reply) {
        try {
            Cursor newdbc = dbc.dup(args.flags == DbConstants.DB_POSITION);
            RpcDbc rdbc = new RpcDbc(rdb, newdbc, false);
            /* If this cursor has a parent txn, we need to use it too. */
            if (timer != this)
                rdbc.timer = timer;
            reply.dbcidcl_id = server.addCursor(rdbc);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void get(Dispatcher server,
                     __dbc_get_msg args, __dbc_get_reply reply) {
        try {
            DatabaseEntry key = Util.makeDatabaseEntry(args.keydata, args.keydlen, args.keydoff, args.keyulen, args.keyflags);
            DatabaseEntry data = Util.makeDatabaseEntry(args.datadata,
                args.datadlen, args.datadoff, args.dataulen, args.dataflags,
                args.flags & (DbConstants.DB_MULTIPLE | DbConstants.DB_MULTIPLE_KEY));

            OperationStatus status;
            switch(args.flags & ~Server.DB_MODIFIER_MASK) {
            case DbConstants.DB_CURRENT:
                status = dbc.getCurrent(key, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_FIRST:
                status = dbc.getFirst(key, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_LAST:
                status = dbc.getLast(key, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_NEXT:
                status = dbc.getNext(key, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_NEXT_DUP:
                status = dbc.getNextDup(key, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_NEXT_NODUP:
                status = dbc.getNextNoDup(key, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_PREV:
                status = dbc.getPrev(key, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_PREV_NODUP:
                status = dbc.getPrevNoDup(key, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_GET_RECNO:
                status = dbc.getRecordNumber(data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_SET:
                status = dbc.getSearchKey(key, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_SET_RANGE:
                status = dbc.getSearchKeyRange(key, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_GET_BOTH:
                status = dbc.getSearchBoth(key, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_GET_BOTH_RANGE:
                status = dbc.getSearchBothRange(key, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_SET_RECNO:
                status = dbc.getSearchRecordNumber(key, data, Util.getLockMode(args.flags));
                break;

                /* Join cursors */
            case 0:
                status = ((JoinCursorAdapter)dbc).jc.getNext(key, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_JOIN_ITEM:
                status = ((JoinCursorAdapter)dbc).jc.getNext(key, Util.getLockMode(args.flags));
                break;

            default:
                throw new UnsupportedOperationException("Unknown flag: " + (args.flags & ~Server.DB_MODIFIER_MASK));
            }
            reply.status = Util.getStatus(status);
            reply.keydata = Util.returnDatabaseEntry(key);
            reply.datadata = Util.returnDatabaseEntry(data);
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
            reply.keydata = reply.datadata = empty;
        }
    }

    public  void pget(Dispatcher server,
                      __dbc_pget_msg args, __dbc_pget_reply reply) {
        try {
            DatabaseEntry skey = Util.makeDatabaseEntry(args.skeydata, args.skeydlen, args.skeydoff, args.skeyulen, args.skeyflags);
            DatabaseEntry pkey = Util.makeDatabaseEntry(args.pkeydata, args.pkeydlen, args.pkeydoff, args.pkeyulen, args.pkeyflags);
            DatabaseEntry data = Util.makeDatabaseEntry(args.datadata, args.datadlen, args.datadoff, args.dataulen, args.dataflags);

            OperationStatus status;
            switch(args.flags & ~Server.DB_MODIFIER_MASK) {
            case DbConstants.DB_CURRENT:
                status = ((SecondaryCursor)dbc).getCurrent(skey, pkey, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_FIRST:
                status = ((SecondaryCursor)dbc).getFirst(skey, pkey, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_LAST:
                status = ((SecondaryCursor)dbc).getLast(skey, pkey, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_NEXT:
                status = ((SecondaryCursor)dbc).getNext(skey, pkey, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_NEXT_DUP:
                status = ((SecondaryCursor)dbc).getNextDup(skey, pkey, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_NEXT_NODUP:
                status = ((SecondaryCursor)dbc).getNextNoDup(skey, pkey, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_PREV:
                status = ((SecondaryCursor)dbc).getPrev(skey, pkey, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_PREV_NODUP:
                status = ((SecondaryCursor)dbc).getPrevNoDup(skey, pkey, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_GET_RECNO:
                status = ((SecondaryCursor)dbc).getRecordNumber(pkey, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_SET:
                status = ((SecondaryCursor)dbc).getSearchKey(skey, pkey, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_SET_RANGE:
                status = ((SecondaryCursor)dbc).getSearchKeyRange(skey, pkey, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_GET_BOTH:
                status = ((SecondaryCursor)dbc).getSearchBoth(skey, pkey, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_GET_BOTH_RANGE:
                status = ((SecondaryCursor)dbc).getSearchBothRange(skey, pkey, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_SET_RECNO:
                status = ((SecondaryCursor)dbc).getSearchRecordNumber(skey, pkey, data, Util.getLockMode(args.flags));
                break;

            default:
                throw new UnsupportedOperationException("Unknown flag: " + (args.flags & ~Server.DB_MODIFIER_MASK));
            }
            reply.status = Util.getStatus(status);
            reply.skeydata = Util.returnDatabaseEntry(skey);
            reply.pkeydata = Util.returnDatabaseEntry(pkey);
            reply.datadata = Util.returnDatabaseEntry(data);
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
            reply.skeydata = reply.pkeydata = reply.datadata = empty;
        }
    }

    public  void put(Dispatcher server,
                     __dbc_put_msg args, __dbc_put_reply reply) {
        try {
            DatabaseEntry key = Util.makeDatabaseEntry(args.keydata, args.keydlen, args.keydoff, args.keyulen, args.keyflags);
            DatabaseEntry data = Util.makeDatabaseEntry(args.datadata, args.datadlen, args.datadoff, args.dataulen, args.dataflags);

            OperationStatus status;
            switch(args.flags & ~Server.DB_MODIFIER_MASK) {
            case 0:
                status = dbc.put(key, data);
                break;

            case DbConstants.DB_AFTER:
                status = dbc.putAfter(key, data);
                break;

            case DbConstants.DB_BEFORE:
                status = dbc.putBefore(key, data);
                break;

            case DbConstants.DB_NOOVERWRITE:
                status = dbc.putNoOverwrite(key, data);
                break;

            case DbConstants.DB_KEYFIRST:
                status = dbc.putKeyFirst(key, data);
                break;

            case DbConstants.DB_KEYLAST:
                status = dbc.putKeyLast(key, data);
                break;

            case DbConstants.DB_NODUPDATA:
                status = dbc.putNoDupData(key, data);
                break;

            case DbConstants.DB_CURRENT:
                status = dbc.putCurrent(data);
                break;

            default:
                throw new UnsupportedOperationException("Unknown flag: " + (args.flags & ~Server.DB_MODIFIER_MASK));
            }
            reply.status = Util.getStatus(status);
            reply.keydata = Util.returnDatabaseEntry(key);
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
            reply.keydata = empty;
        }
    }
}
