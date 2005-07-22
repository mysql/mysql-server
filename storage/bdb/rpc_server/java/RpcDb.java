/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: RpcDb.java,v 1.24 2004/11/05 00:42:40 mjc Exp $
 */

package com.sleepycat.db.rpcserver;

import com.sleepycat.db.*;
import com.sleepycat.db.internal.DbConstants;
import java.io.*;
import java.util.*;

/**
 * RPC wrapper around a db object for the Java RPC server.
 */
public class RpcDb extends Timer {
    static final byte[] empty = new byte[0];
    DatabaseConfig config;
    Database db;
    RpcDbEnv rdbenv;
    int refcount = 0;
    String dbname, subdbname;
    int type, setflags, openflags;

    public RpcDb(RpcDbEnv rdbenv) {
        this.rdbenv = rdbenv;
    }

    void dispose() {
        if (db != null) {
            try {
                db.close();
            } catch (Throwable t) {
                Util.handleException(t);
            }
            db = null;
        }
    }

    public  void associate(Dispatcher server,
                           __db_associate_msg args, __db_associate_reply reply) {
        try {
            // The semantics of the new API are a little different.
            // The secondary database will already be open, here, so we first
            // have to close it and then call openSecondaryDatabase.
            RpcDb secondary = server.getDatabase(args.sdbpcl_id);
            try {
                secondary.db.close();
            } finally {
                secondary.db = null;
            }

            RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
            Transaction txn = (rtxn != null) ? rtxn.txn : null;

            args.flags &= ~AssociateCallbacks.DB_RPC2ND_MASK;
            SecondaryConfig secondaryConfig = new SecondaryConfig();
            // The secondary has already been opened once, so we don't
            // need all of the settings here, only a few:
            secondaryConfig.setReadOnly(secondary.config.getReadOnly());
            secondaryConfig.setTransactional(secondary.config.getTransactional());
            secondaryConfig.setKeyCreator(AssociateCallbacks.getCallback(args.flags));
            secondaryConfig.setAllowPopulate((args.flags & DbConstants.DB_CREATE) != 0);
            secondary.db = rdbenv.dbenv.openSecondaryDatabase(txn, secondary.dbname, secondary.subdbname, db, secondaryConfig);
            secondary.config = secondary.db.getConfig();
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public void close(Dispatcher server,
                       __db_close_msg args, __db_close_reply reply) {
        if (refcount == 0 || --refcount > 0) {
            reply.status = 0;
            return;
        }

        try {
            server.delDatabase(this, false);
            if (db != null)
                db.close((args.flags & DbConstants.DB_NOSYNC) != 0);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        } finally {
            db = null;
        }
    }

    public  void create(Dispatcher server,
                        __db_create_msg args, __db_create_reply reply) {
        try {
            config = new DatabaseConfig();
            config.setXACreate((args.flags & DbConstants.DB_XA_CREATE) != 0);
            reply.dbcl_id = server.addDatabase(this);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void cursor(Dispatcher server,
                        __db_cursor_msg args, __db_cursor_reply reply) {
        try {
            RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
            Transaction txn = (rtxn != null) ? rtxn.txn : null;

            CursorConfig config = new CursorConfig();
            config.setDirtyRead((args.flags & DbConstants.DB_DIRTY_READ) != 0);
            config.setDegree2((args.flags & DbConstants.DB_DEGREE_2) != 0);
            config.setWriteCursor((args.flags & DbConstants.DB_WRITECURSOR) != 0);

            Cursor dbc = db.openCursor(txn, config);
            RpcDbc rdbc = new RpcDbc(this, dbc, false);
            rdbc.timer = (rtxn != null) ? rtxn.timer : this;
            reply.dbcidcl_id = server.addCursor(rdbc);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void del(Dispatcher server,
                     __db_del_msg args, __db_del_reply reply) {
        try {
            RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
            Transaction txn = (rtxn != null) ? rtxn.txn : null;
            DatabaseEntry key = Util.makeDatabaseEntry(args.keydata, args.keydlen, args.keydoff, args.keyulen, args.keyflags);

            db.delete(txn, key /* args.flags == 0 */);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void get(Dispatcher server,
                     __db_get_msg args, __db_get_reply reply) {
        try {
            RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
            Transaction txn = (rtxn != null) ? rtxn.txn : null;
            DatabaseEntry key = Util.makeDatabaseEntry(args.keydata, args.keydlen, args.keydoff, args.keyulen, args.keyflags);
            DatabaseEntry data = Util.makeDatabaseEntry(args.datadata,
                args.datadlen, args.datadoff, args.dataulen, args.dataflags,
                args.flags & DbConstants.DB_MULTIPLE);

            OperationStatus status;
            switch(args.flags & ~Server.DB_MODIFIER_MASK) {
            case 0:
                status = db.get(txn, key, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_CONSUME:
                status = db.consume(txn, key, data, false);
                break;

            case DbConstants.DB_CONSUME_WAIT:
                status = db.consume(txn, key, data, true);
                break;

            case DbConstants.DB_GET_BOTH:
                status = db.getSearchBoth(txn, key, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_SET_RECNO:
                status = db.getSearchRecordNumber(txn, key, data, Util.getLockMode(args.flags));
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

    public  void join(Dispatcher server,
                      __db_join_msg args, __db_join_reply reply) {
        try {
            Cursor[] cursors = new Cursor[args.curs.length + 1];
            for (int i = 0; i < args.curs.length; i++) {
                RpcDbc rdbc = server.getCursor(args.curs[i]);
                if (rdbc == null) {
                    reply.status = DbConstants.DB_NOSERVER_ID;
                    return;
                }
                cursors[i] = rdbc.dbc;
            }
            cursors[args.curs.length] = null;

            JoinConfig config = new JoinConfig();
            config.setNoSort(args.flags == DbConstants.DB_JOIN_NOSORT);
            JoinCursor jdbc = db.join(cursors, config);

            RpcDbc rjdbc = new RpcDbc(this, new JoinCursorAdapter(db, jdbc), true);
            /*
             * If our curslist has a parent txn, we need to use it too
             * for the activity timeout.  All cursors must be part of
             * the same transaction, so just check the first.
             */
            RpcDbc rdbc0 = server.getCursor(args.curs[0]);
            if (rdbc0.timer != rdbc0)
                rjdbc.timer = rdbc0.timer;

            /*
             * All of the curslist cursors must point to the join
             * cursor's timeout so that we do not timeout any of the
             * curlist cursors while the join cursor is active.
             */
            for (int i = 0; i < args.curs.length; i++) {
                RpcDbc rdbc = server.getCursor(args.curs[i]);
                rdbc.orig_timer = rdbc.timer;
                rdbc.timer = rjdbc;
            }
            reply.dbcidcl_id = server.addCursor(rjdbc);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void key_range(Dispatcher server,
                           __db_key_range_msg args, __db_key_range_reply reply) {
        try {
            RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
            Transaction txn = (rtxn != null) ? rtxn.txn : null;
            DatabaseEntry key = Util.makeDatabaseEntry(args.keydata, args.keydlen, args.keydoff, args.keyulen, args.keyflags);

            KeyRange range = db.getKeyRange(txn, key /*, args.flags == 0 */);
            reply.status = 0;
            reply.less = range.less;
            reply.equal = range.equal;
            reply.greater = range.greater;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    private boolean findSharedDatabase(Dispatcher server, __db_open_reply reply)
        throws DatabaseException {
        RpcDb rdb = null;
        boolean matchFound = false;
        LocalIterator i = ((Server)server).db_list.iterator();

        while (!matchFound && i.hasNext()) {
            rdb = (RpcDb)i.next();
            if (rdb != null && rdb != this && rdb.rdbenv == rdbenv &&
                (type == DbConstants.DB_UNKNOWN || rdb.type == type) &&
                openflags == rdb.openflags &&
                setflags == rdb.setflags &&
                dbname != null && rdb.dbname != null &&
                dbname.equals(rdb.dbname) &&
                (subdbname == rdb.subdbname ||
                 (subdbname != null && rdb.subdbname != null &&
                  subdbname.equals(rdb.subdbname))))
                matchFound = true;
        }

        if (matchFound) {
            ++rdb.refcount;
            reply.dbcl_id = ((FreeList.FreeListIterator)i).current;
            reply.type = Util.fromDatabaseType(rdb.config.getType());
            reply.lorder = rdb.config.getByteOrder();
            reply.status = 0;

            // Server.err.println("Sharing Database: " + reply.dbcl_id);
        }

        return matchFound;
    }

    public  void get_name(Dispatcher server,
                          __db_get_name_msg args, __db_get_name_reply reply) {
        reply.filename = dbname;
        reply.dbname = subdbname;
        reply.status = 0;
    }

    public  void get_open_flags(Dispatcher server,
                                __db_get_open_flags_msg args, __db_get_open_flags_reply reply) {
        try {
            reply.flags = 0;
            if (config.getAllowCreate()) reply.flags |= DbConstants.DB_CREATE;
            if (config.getExclusiveCreate()) reply.flags |= DbConstants.DB_EXCL;
            if (config.getReadOnly()) reply.flags |= DbConstants.DB_RDONLY;
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void open(Dispatcher server,
                      __db_open_msg args, __db_open_reply reply) {
        try {
            dbname = (args.name.length() > 0) ? args.name : null;
            subdbname = (args.subdb.length() > 0) ? args.subdb : null;
            type = args.type;
            openflags = args.flags & Server.DB_SERVER_DBFLAGS;

            if (findSharedDatabase(server, reply)) {
                server.delDatabase(this, true);
            } else {
                RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
                Transaction txn = (rtxn != null) ? rtxn.txn : null;

                // Server.err.println("Calling db.open(" + null + ", " + dbname + ", " + subdbname + ", " + args.type + ", " + Integer.toHexString(args.flags) + ", " + args.mode + ")");

                config.setAllowCreate((args.flags & DbConstants.DB_CREATE) != 0);
                config.setExclusiveCreate((args.flags & DbConstants.DB_EXCL) != 0);
                config.setReadOnly((args.flags & DbConstants.DB_RDONLY) != 0);
                config.setTransactional(txn != null || (args.flags & DbConstants.DB_AUTO_COMMIT) != 0);
                config.setTruncate((args.flags & DbConstants.DB_TRUNCATE) != 0);
                config.setType(Util.toDatabaseType(args.type));
                config.setMode(args.mode);

                db = rdbenv.dbenv.openDatabase(txn, dbname, subdbname, config);
                ++refcount;

                // Refresh config in case we didn't know the full story before opening
                config = db.getConfig();

                reply.dbcl_id = args.dbpcl_id;
                type = reply.type = Util.fromDatabaseType(config.getType());
                reply.lorder = config.getByteOrder();
                reply.status = 0;
            }
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }

        // System.err.println("Database.open: reply.status = " + reply.status + ", reply.dbcl_id = " + reply.dbcl_id);
    }

    public  void pget(Dispatcher server,
                      __db_pget_msg args, __db_pget_reply reply) {
        try {
            RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
            Transaction txn = (rtxn != null) ? rtxn.txn : null;
            DatabaseEntry skey = Util.makeDatabaseEntry(args.skeydata, args.skeydlen, args.skeydoff, args.skeyulen, args.skeyflags);
            DatabaseEntry pkey = Util.makeDatabaseEntry(args.pkeydata, args.pkeydlen, args.pkeydoff, args.pkeyulen, args.pkeyflags);
            DatabaseEntry data = Util.makeDatabaseEntry(args.datadata, args.datadlen, args.datadoff, args.dataulen, args.dataflags);

            OperationStatus status;
            switch(args.flags & ~Server.DB_MODIFIER_MASK) {
            case 0:
                status = ((SecondaryDatabase)db).get(txn, skey, pkey, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_GET_BOTH:
                status = ((SecondaryDatabase)db).getSearchBoth(txn, skey, pkey, data, Util.getLockMode(args.flags));
                break;

            case DbConstants.DB_SET_RECNO:
                status = ((SecondaryDatabase)db).getSearchRecordNumber(txn, skey, pkey, data, Util.getLockMode(args.flags));
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
                     __db_put_msg args, __db_put_reply reply) {
        try {
            RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
            Transaction txn = (rtxn != null) ? rtxn.txn : null;

            DatabaseEntry key = Util.makeDatabaseEntry(args.keydata, args.keydlen, args.keydoff, args.keyulen, args.keyflags);
            DatabaseEntry data = Util.makeDatabaseEntry(args.datadata, args.datadlen, args.datadoff, args.dataulen, args.dataflags);

            reply.keydata = empty;
            OperationStatus status;
            switch(args.flags & ~Server.DB_MODIFIER_MASK) {
            case 0:
                status = db.put(txn, key, data);
                break;

            case DbConstants.DB_APPEND:
                status = db.append(txn, key, data);
                reply.keydata = Util.returnDatabaseEntry(key);
                break;

            case DbConstants.DB_NODUPDATA:
                status = db.putNoDupData(txn, key, data);
                break;

            case DbConstants.DB_NOOVERWRITE:
                status = db.putNoOverwrite(txn, key, data);
                break;

            default:
                throw new UnsupportedOperationException("Unknown flag: " + (args.flags & ~Server.DB_MODIFIER_MASK));
            }
            reply.status = Util.getStatus(status);
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
            reply.keydata = empty;
        }
    }

    public  void remove(Dispatcher server,
                        __db_remove_msg args, __db_remove_reply reply) {
        try {
            args.name = (args.name.length() > 0) ? args.name : null;
            args.subdb = (args.subdb.length() > 0) ? args.subdb : null;
            Database.remove(args.name, args.subdb, config);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        } finally {
            server.delDatabase(this, false);
        }
    }

    public  void rename(Dispatcher server,
                        __db_rename_msg args, __db_rename_reply reply) {
        try {
            args.name = (args.name.length() > 0) ? args.name : null;
            args.subdb = (args.subdb.length() > 0) ? args.subdb : null;
            args.newname = (args.newname.length() > 0) ? args.newname : null;
            Database.rename(args.name, args.subdb, args.newname, config);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        } finally {
            server.delDatabase(this, false);
        }
    }

    public  void set_bt_maxkey(Dispatcher server,
                               __db_bt_maxkey_msg args, __db_bt_maxkey_reply reply) {
        try {
            // XXX: check what to do about: config.setBtreeMaxKey(args.maxkey);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void get_bt_minkey(Dispatcher server,
                               __db_get_bt_minkey_msg args, __db_get_bt_minkey_reply reply) {
        try {
            reply.minkey = config.getBtreeMinKey();
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void set_bt_minkey(Dispatcher server,
                               __db_bt_minkey_msg args, __db_bt_minkey_reply reply) {
        try {
            config.setBtreeMinKey(args.minkey);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void get_encrypt_flags(Dispatcher server,
                                   __db_get_encrypt_flags_msg args, __db_get_encrypt_flags_reply reply) {
        try {
            reply.flags = config.getEncrypted() ? DbConstants.DB_ENCRYPT_AES : 0;
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void set_encrypt(Dispatcher server,
                             __db_encrypt_msg args, __db_encrypt_reply reply) {
        try {
            config.setEncrypted(args.passwd /*, args.flags == 0 */);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void get_flags(Dispatcher server,
                           __db_get_flags_msg args, __db_get_flags_reply reply) {
        try {
            reply.flags = 0;
            if (config.getChecksum()) reply.flags |= DbConstants.DB_CHKSUM;
            if (config.getEncrypted()) reply.flags |= DbConstants.DB_ENCRYPT;
            if (config.getBtreeRecordNumbers()) reply.flags |= DbConstants.DB_RECNUM;
            if (config.getRenumbering()) reply.flags |= DbConstants.DB_RENUMBER;
            if (config.getReverseSplitOff()) reply.flags |= DbConstants.DB_REVSPLITOFF;
            if (config.getSortedDuplicates()) reply.flags |= DbConstants.DB_DUPSORT;
            if (config.getSnapshot()) reply.flags |= DbConstants.DB_SNAPSHOT;
            if (config.getUnsortedDuplicates()) reply.flags |= DbConstants.DB_DUP;
            if (config.getTransactionNotDurable()) reply.flags |= DbConstants.DB_TXN_NOT_DURABLE;
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void set_flags(Dispatcher server,
                           __db_flags_msg args, __db_flags_reply reply) {
        try {
            // Server.err.println("Calling db.setflags(" + Integer.toHexString(args.flags) + ")");
            config.setChecksum((args.flags & DbConstants.DB_CHKSUM) != 0);
            config.setBtreeRecordNumbers((args.flags & DbConstants.DB_RECNUM) != 0);
            config.setRenumbering((args.flags & DbConstants.DB_RENUMBER) != 0);
            config.setReverseSplitOff((args.flags & DbConstants.DB_REVSPLITOFF) != 0);
            config.setSortedDuplicates((args.flags & DbConstants.DB_DUPSORT) != 0);
            config.setSnapshot((args.flags & DbConstants.DB_SNAPSHOT) != 0);
            config.setUnsortedDuplicates((args.flags & DbConstants.DB_DUP) != 0);
            config.setTransactionNotDurable((args.flags & DbConstants.DB_TXN_NOT_DURABLE) != 0);

            setflags |= args.flags;
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void get_h_ffactor(Dispatcher server,
                               __db_get_h_ffactor_msg args, __db_get_h_ffactor_reply reply) {
        try {
            reply.ffactor = config.getHashFillFactor();
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void set_h_ffactor(Dispatcher server,
                               __db_h_ffactor_msg args, __db_h_ffactor_reply reply) {
        try {
            config.setHashFillFactor(args.ffactor);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void get_h_nelem(Dispatcher server,
                             __db_get_h_nelem_msg args, __db_get_h_nelem_reply reply) {
        try {
            reply.nelem = config.getHashNumElements();
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void set_h_nelem(Dispatcher server,
                             __db_h_nelem_msg args, __db_h_nelem_reply reply) {
        try {
            config.setHashNumElements(args.nelem);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void get_lorder(Dispatcher server,
                            __db_get_lorder_msg args, __db_get_lorder_reply reply) {
        try {
            reply.lorder = config.getByteOrder();
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void set_lorder(Dispatcher server,
                            __db_lorder_msg args, __db_lorder_reply reply) {
        try {
            config.setByteOrder(args.lorder);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void get_pagesize(Dispatcher server,
                              __db_get_pagesize_msg args, __db_get_pagesize_reply reply) {
        try {
            reply.pagesize = config.getPageSize();
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void set_pagesize(Dispatcher server,
                              __db_pagesize_msg args, __db_pagesize_reply reply) {
        try {
            config.setPageSize(args.pagesize);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void get_q_extentsize(Dispatcher server,
                                  __db_get_extentsize_msg args, __db_get_extentsize_reply reply) {
        try {
            reply.extentsize = config.getQueueExtentSize();
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void set_q_extentsize(Dispatcher server,
                                  __db_extentsize_msg args, __db_extentsize_reply reply) {
        try {
            config.setQueueExtentSize(args.extentsize);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void get_re_delim(Dispatcher server,
                              __db_get_re_delim_msg args, __db_get_re_delim_reply reply) {
        try {
            reply.delim = config.getRecordDelimiter();
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void set_re_delim(Dispatcher server,
                              __db_re_delim_msg args, __db_re_delim_reply reply) {
        try {
            config.setRecordDelimiter(args.delim);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void get_re_len(Dispatcher server,
                            __db_get_re_len_msg args, __db_get_re_len_reply reply) {
        try {
            reply.len = config.getRecordLength();
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void set_re_len(Dispatcher server,
                            __db_re_len_msg args, __db_re_len_reply reply) {
        try {
            config.setRecordLength(args.len);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void get_re_pad(Dispatcher server,
                            __db_get_re_pad_msg args, __db_get_re_pad_reply reply) {
        try {
            reply.pad = config.getRecordPad();
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void set_re_pad(Dispatcher server,
                            __db_re_pad_msg args, __db_re_pad_reply reply) {
        try {
            config.setRecordPad(args.pad);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void stat(Dispatcher server,
                      __db_stat_msg args, __db_stat_reply reply) {
        try {
            RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
            Transaction txn = (rtxn != null) ? rtxn.txn : null;
            StatsConfig config = new StatsConfig();
            config.setClear((args.flags & DbConstants.DB_STAT_CLEAR) != 0);
            config.setFast((args.flags & DbConstants.DB_FAST_STAT) != 0);
            DatabaseStats raw_stat = db.getStats(txn, config);

            if (raw_stat instanceof BtreeStats) {
                BtreeStats bs = (BtreeStats)raw_stat;
                int[] raw_stats = {
                    bs.getMagic(), bs.getVersion(),
                    bs.getMetaFlags(), bs.getNumKeys(),
                    bs.getNumData(), bs.getPageSize(),
                    bs.getMaxKey(), bs.getMinKey(),
                    bs.getReLen(), bs.getRePad(),
                    bs.getLevels(), bs.getIntPages(),
                    bs.getLeafPages(), bs.getDupPages(),
                    bs.getOverPages(), bs.getFree(),
                    bs.getIntPagesFree(), bs.getLeafPagesFree(),
                    bs.getDupPagesFree(), bs.getOverPagesFree()
                };
                reply.stats = raw_stats;
            } else if (raw_stat instanceof HashStats) {
                HashStats hs = (HashStats)raw_stat;
                int[] raw_stats = {
                    hs.getMagic(), hs.getVersion(),
                    hs.getMetaFlags(), hs.getNumKeys(),
                    hs.getNumData(), hs.getPageSize(),
                    hs.getFfactor(), hs.getBuckets(),
                    hs.getFree(), hs.getBFree(),
                    hs.getBigPages(), hs.getBigBFree(),
                    hs.getOverflows(), hs.getOvflFree(),
                    hs.getDup(), hs.getDupFree()
                };
                reply.stats = raw_stats;
            } else if (raw_stat instanceof QueueStats) {
                QueueStats qs = (QueueStats)raw_stat;
                int[] raw_stats = {
                    qs.getMagic(), qs.getVersion(),
                    qs.getMetaFlags(), qs.getNumKeys(),
                    qs.getNumData(), qs.getPageSize(),
                    qs.getExtentSize(), qs.getPages(),
                    qs.getReLen(), qs.getRePad(),
                    qs.getPagesFree(), qs.getFirstRecno(),
                    qs.getCurRecno()
                };
                reply.stats = raw_stats;
            } else
                throw new DatabaseException("Invalid return type from db.stat()", DbConstants.DB_NOTFOUND);

            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
            reply.stats = new int[0];
        }
    }

    public  void sync(Dispatcher server,
                      __db_sync_msg args, __db_sync_reply reply) {
        try {
            db.sync(/* args.flags == 0 */);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void truncate(Dispatcher server,
                          __db_truncate_msg args, __db_truncate_reply reply) {
        try {
            RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
            Transaction txn = (rtxn != null) ? rtxn.txn : null;
            reply.count = db.truncate(txn, true /*, args.flags == 0 */);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
            reply.count = 0;
        }
    }
}
