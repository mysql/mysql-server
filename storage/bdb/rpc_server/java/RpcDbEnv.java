/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: RpcDbEnv.java,v 1.15 2004/04/21 01:09:11 mjc Exp $
 */

package com.sleepycat.db.rpcserver;

import com.sleepycat.db.*;
import com.sleepycat.db.internal.DbConstants;
import java.io.*;
import java.util.*;

/**
 * RPC wrapper around a dbenv for the Java RPC server.
 */
public class RpcDbEnv extends Timer {
    EnvironmentConfig config;
    Environment dbenv;
    String home;
    long idletime, timeout;
    int openflags, onflags, offflags;
    int refcount = 1;

    void dispose() {
        if (dbenv != null) {
            try {
                dbenv.close();
            } catch (Throwable t) {
                Util.handleException(t);
            }
            dbenv = null;
        }
    }

    public  void close(Dispatcher server,
                       __env_close_msg args, __env_close_reply reply) {
        if (--refcount != 0) {
            reply.status = 0;
            return;
        }

        try {
            server.delEnv(this, false);
            if (dbenv != null)
                dbenv.close(/* args.flags == 0 */);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        } finally {
            dbenv = null;
        }
    }

    public  void create(Dispatcher server,
                        __env_create_msg args, __env_create_reply reply) {
        this.idletime = (args.timeout != 0) ? args.timeout : Server.idleto;
        this.timeout = Server.defto;
        try {
            config = new EnvironmentConfig();
            config.setErrorStream(Server.errstream);
            reply.envcl_id = server.addEnv(this);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void dbremove(Dispatcher server,
                          __env_dbremove_msg args, __env_dbremove_reply reply) {
        try {
            args.name = (args.name.length() > 0) ? args.name : null;
            args.subdb = (args.subdb.length() > 0) ? args.subdb : null;

            RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
            Transaction txn = (rtxn != null) ? rtxn.txn : null;
            dbenv.removeDatabase(txn, args.name, args.subdb /*, args.flags == 0 */);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void dbrename(Dispatcher server,
                          __env_dbrename_msg args, __env_dbrename_reply reply) {
        try {
            args.name = (args.name.length() > 0) ? args.name : null;
            args.subdb = (args.subdb.length() > 0) ? args.subdb : null;
            args.newname = (args.newname.length() > 0) ? args.newname : null;

            RpcDbTxn rtxn = server.getTxn(args.txnpcl_id);
            Transaction txn = (rtxn != null) ? rtxn.txn : null;
            dbenv.renameDatabase(txn, args.name, args.subdb, args.newname /*, args.flags == 0 */);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    private boolean findSharedEnvironment(Dispatcher server, __env_open_reply reply)
        throws DatabaseException {
        RpcDbEnv rdbenv = null;
        boolean matchFound = false;
        LocalIterator i = ((Server)server).env_list.iterator();

        while (!matchFound && i.hasNext()) {
            rdbenv = (RpcDbEnv)i.next();
            if (rdbenv != null && rdbenv != this &&
                (home == rdbenv.home ||
                 (home != null && home.equals(rdbenv.home))) &&
                openflags == rdbenv.openflags &&
                onflags == rdbenv.onflags &&
                offflags == rdbenv.offflags)
                matchFound = true;
        }

        if (matchFound) {
            /*
             * The only thing left to check is the timeout.
             * Since the server timeout set by the client is a hint, for sharing
             * we'll give them the benefit of the doubt and grant them the
             * longer timeout.
             */
            if (rdbenv.timeout < timeout)
                rdbenv.timeout = timeout;

            ++rdbenv.refcount;
            reply.envcl_id = ((FreeList.FreeListIterator)i).current;
            reply.status = 0;

            Server.err.println("Sharing Environment: " + reply.envcl_id);
        }

        return matchFound;
    }

    public  void get_home(Dispatcher server,
                          __env_get_home_msg args, __env_get_home_reply reply) {
        try {
            reply.home = dbenv.getHome().toString();
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void get_open_flags(Dispatcher server,
                                __env_get_open_flags_msg args, __env_get_open_flags_reply reply) {
        try {
            reply.flags = 0;
            if (config.getAllowCreate()) reply.flags |= DbConstants.DB_CREATE;
            if (config.getInitializeCache()) reply.flags |= DbConstants.DB_INIT_MPOOL;
            if (config.getInitializeCDB()) reply.flags |= DbConstants.DB_INIT_CDB;
            if (config.getInitializeLocking()) reply.flags |= DbConstants.DB_INIT_LOCK;
            if (config.getInitializeLogging()) reply.flags |= DbConstants.DB_INIT_LOG;
            if (config.getInitializeReplication()) reply.flags |= DbConstants.DB_INIT_REP;
            if (config.getJoinEnvironment()) reply.flags |= DbConstants.DB_JOINENV;
            if (config.getLockDown()) reply.flags |= DbConstants.DB_LOCKDOWN;
            if (config.getPrivate()) reply.flags |= DbConstants.DB_PRIVATE;
            if (config.getReadOnly()) reply.flags |= DbConstants.DB_RDONLY;
            if (config.getRunRecovery()) reply.flags |= DbConstants.DB_RECOVER;
            if (config.getRunFatalRecovery()) reply.flags |= DbConstants.DB_RECOVER_FATAL;
            if (config.getSystemMemory()) reply.flags |= DbConstants.DB_SYSTEM_MEM;
            if (config.getTransactional()) reply.flags |= DbConstants.DB_INIT_TXN;
            if (config.getUseEnvironment()) reply.flags |= DbConstants.DB_USE_ENVIRON;
            if (config.getUseEnvironmentRoot()) reply.flags |= DbConstants.DB_USE_ENVIRON_ROOT;

            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void open(Dispatcher server,
                      __env_open_msg args, __env_open_reply reply) {
        try {
            home = (args.home.length() > 0) ? args.home : null;

            /*
             * If they are using locking do deadlock detection for
             * them, internally.
             */
            if ((args.flags & DbConstants.DB_INIT_LOCK) != 0)
                config.setLockDetectMode(LockDetectMode.DEFAULT);

            // adjust flags for RPC
            int newflags = (args.flags & ~Server.DB_SERVER_FLAGMASK);
            openflags = (newflags & Server.DB_SERVER_ENVFLAGS);

            config.setAllowCreate((args.flags & DbConstants.DB_CREATE) != 0);
            config.setInitializeCache((args.flags & DbConstants.DB_INIT_MPOOL) != 0);
            config.setInitializeCDB((args.flags & DbConstants.DB_INIT_CDB) != 0);
            config.setInitializeLocking((args.flags & DbConstants.DB_INIT_LOCK) != 0);
            config.setInitializeLogging((args.flags & DbConstants.DB_INIT_LOG) != 0);
            config.setInitializeReplication((args.flags & DbConstants.DB_INIT_REP) != 0);
            config.setJoinEnvironment((args.flags & DbConstants.DB_JOINENV) != 0);
            config.setLockDown((args.flags & DbConstants.DB_LOCKDOWN) != 0);
            config.setPrivate((args.flags & DbConstants.DB_PRIVATE) != 0);
            config.setReadOnly((args.flags & DbConstants.DB_RDONLY) != 0);
            config.setRunRecovery((args.flags & DbConstants.DB_RECOVER) != 0);
            config.setRunFatalRecovery((args.flags & DbConstants.DB_RECOVER_FATAL) != 0);
            config.setSystemMemory((args.flags & DbConstants.DB_SYSTEM_MEM) != 0);
            config.setTransactional((args.flags & DbConstants.DB_INIT_TXN) != 0);
            config.setUseEnvironment((args.flags & DbConstants.DB_USE_ENVIRON) != 0);
            config.setUseEnvironmentRoot((args.flags & DbConstants.DB_USE_ENVIRON_ROOT) != 0);

            if (findSharedEnvironment(server, reply))
                dbenv = null;
            else if (Server.check_home(home)) {
                dbenv = new Environment(new File(home), config);
                // Get the configuration after opening -- it may have changed if we're joining an environment
                config = dbenv.getConfig();
                reply.status = 0;
                reply.envcl_id = args.dbenvcl_id;
            } else
                reply.status = DbConstants.DB_NOSERVER_HOME;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }

        // System.err.println("Environment.open: reply.status = " + reply.status + ", reply.envcl_id = " + reply.envcl_id);
    }

    public  void remove(Dispatcher server,
                        __env_remove_msg args, __env_remove_reply reply) {
        Server.err.println("RpcDbEnv.remove(" + args.home + ")");
        try {
            args.home = (args.home.length() > 0) ? args.home : null;
            // TODO: check home?

            boolean force = (args.flags & DbConstants.DB_FORCE) != 0;
            config.setUseEnvironment((args.flags & DbConstants.DB_USE_ENVIRON) != 0);
            config.setUseEnvironmentRoot((args.flags & DbConstants.DB_USE_ENVIRON_ROOT) != 0);

            Environment.remove(new File(args.home), force, config);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        } finally {
            server.delEnv(this, false);
        }
    }

    public  void get_cachesize(Dispatcher server,
                               __env_get_cachesize_msg args, __env_get_cachesize_reply reply) {
        try {
            long cachesize = config.getCacheSize();
            final long GIGABYTE = 1073741824;
            reply.gbytes = (int)(cachesize / GIGABYTE);
            reply.bytes = (int)(cachesize % GIGABYTE);
            reply.ncache = config.getCacheCount();
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void set_cachesize(Dispatcher server,
                               __env_cachesize_msg args, __env_cachesize_reply reply) {
        try {
            long bytes = (long)args.gbytes * 1024 * 1024 * 1024;
            bytes += args.bytes;
            config.setCacheSize(bytes);
            config.setCacheCount(args.ncache);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void get_encrypt_flags(Dispatcher server,
                                   __env_get_encrypt_flags_msg args, __env_get_encrypt_flags_reply reply) {
        try {
            reply.flags = config.getEncrypted() ? DbConstants.DB_ENCRYPT_AES : 0;
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void set_encrypt(Dispatcher server,
                             __env_encrypt_msg args, __env_encrypt_reply reply) {
        try {
            config.setEncrypted(args.passwd);
            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void get_flags(Dispatcher server,
                           __env_get_flags_msg args, __env_get_flags_reply reply) {
        try {
            reply.flags = 0;
            if (config.getCDBLockAllDatabases()) reply.flags |= DbConstants.DB_CDB_ALLDB;
            if (config.getDirectDatabaseIO()) reply.flags |= DbConstants.DB_DIRECT_DB;
            if (config.getDirectLogIO()) reply.flags |= DbConstants.DB_DIRECT_LOG;
            if (config.getInitializeRegions()) reply.flags |= DbConstants.DB_REGION_INIT;
            if (config.getLogAutoRemove()) reply.flags |= DbConstants.DB_LOG_AUTOREMOVE;
            if (config.getNoLocking()) reply.flags |= DbConstants.DB_NOLOCKING;
            if (config.getNoMMap()) reply.flags |= DbConstants.DB_NOMMAP;
            if (config.getNoPanic()) reply.flags |= DbConstants.DB_NOPANIC;
            if (config.getOverwrite()) reply.flags |= DbConstants.DB_OVERWRITE;
            if (config.getTxnNoSync()) reply.flags |= DbConstants.DB_TXN_NOSYNC;
            if (config.getTxnNotDurable()) reply.flags |= DbConstants.DB_TXN_NOT_DURABLE;
            if (config.getTxnWriteNoSync()) reply.flags |= DbConstants.DB_TXN_WRITE_NOSYNC;
            if (config.getYieldCPU()) reply.flags |= DbConstants.DB_YIELDCPU;

            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    public  void set_flags(Dispatcher server,
                           __env_flags_msg args, __env_flags_reply reply) {
        try {
            boolean onoff = (args.onoff != 0);
            if (onoff)
                onflags |= args.flags;
            else
                offflags |= args.flags;

            if ((args.flags & DbConstants.DB_CDB_ALLDB) != 0) config.setCDBLockAllDatabases(onoff);
            if ((args.flags & DbConstants.DB_DIRECT_DB) != 0) config.setDirectDatabaseIO(onoff);
            if ((args.flags & DbConstants.DB_DIRECT_LOG) != 0) config.setDirectLogIO(onoff);
            if ((args.flags & DbConstants.DB_REGION_INIT) != 0) config.setInitializeRegions(onoff);
            if ((args.flags & DbConstants.DB_LOG_AUTOREMOVE) != 0) config.setLogAutoRemove(onoff);
            if ((args.flags & DbConstants.DB_NOLOCKING) != 0) config.setNoLocking(onoff);
            if ((args.flags & DbConstants.DB_NOMMAP) != 0) config.setNoMMap(onoff);
            if ((args.flags & DbConstants.DB_NOPANIC) != 0) config.setNoPanic(onoff);
            if ((args.flags & DbConstants.DB_OVERWRITE) != 0) config.setOverwrite(onoff);
            if ((args.flags & DbConstants.DB_TXN_NOSYNC) != 0) config.setTxnNoSync(onoff);
            if ((args.flags & DbConstants.DB_TXN_NOT_DURABLE) != 0) config.setTxnNotDurable(onoff);
            if ((args.flags & DbConstants.DB_TXN_WRITE_NOSYNC) != 0) config.setTxnWriteNoSync(onoff);
            if ((args.flags & DbConstants.DB_YIELDCPU) != 0) config.setYieldCPU(onoff);

            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }

    // txn_recover implementation
    public  void txn_recover(Dispatcher server,
                             __txn_recover_msg args, __txn_recover_reply reply) {
        try {
            PreparedTransaction[] prep_list = dbenv.recover(args.count, args.flags == DbConstants.DB_NEXT);
            if (prep_list != null && prep_list.length > 0) {
                int count = prep_list.length;
                reply.retcount = count;
                reply.txn = new int[count];
                reply.gid = new byte[count * DbConstants.DB_XIDDATASIZE];

                for (int i = 0; i < count; i++) {
                    reply.txn[i] = server.addTxn(new RpcDbTxn(this, prep_list[i].getTransaction()));
                    System.arraycopy(prep_list[i].getGID(), 0, reply.gid, i * DbConstants.DB_XIDDATASIZE, DbConstants.DB_XIDDATASIZE);
                }
            }

            reply.status = 0;
        } catch (Throwable t) {
            reply.status = Util.handleException(t);
        }
    }
}
