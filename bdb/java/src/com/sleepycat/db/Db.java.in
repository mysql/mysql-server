/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: Db.java,v 11.110 2002/09/09 20:47:31 bostic Exp $
 */

package com.sleepycat.db;

import java.io.OutputStream;
import java.io.FileNotFoundException;

/**
 *
 * @author Donald D. Anderson
 */
public class Db
{
    // BEGIN-JAVA-SPECIAL-CONSTANTS
    /* DO NOT EDIT: automatically built by dist/s_java. */
    public static final int DB_BTREE = 1;
    public static final int DB_DONOTINDEX = -30999;
    public static final int DB_HASH = 2;
    public static final int DB_KEYEMPTY = -30998;
    public static final int DB_KEYEXIST = -30997;
    public static final int DB_LOCK_DEADLOCK = -30996;
    public static final int DB_LOCK_NOTGRANTED = -30995;
    public static final int DB_NOSERVER = -30994;
    public static final int DB_NOSERVER_HOME = -30993;
    public static final int DB_NOSERVER_ID = -30992;
    public static final int DB_NOTFOUND = -30991;
    public static final int DB_OLD_VERSION = -30990;
    public static final int DB_PAGE_NOTFOUND = -30989;
    public static final int DB_QUEUE = 4;
    public static final int DB_RECNO = 3;
    public static final int DB_REP_DUPMASTER = -30988;
    public static final int DB_REP_HOLDELECTION = -30987;
    public static final int DB_REP_NEWMASTER = -30986;
    public static final int DB_REP_NEWSITE = -30985;
    public static final int DB_REP_OUTDATED = -30984;
    public static final int DB_RUNRECOVERY = -30982;
    public static final int DB_SECONDARY_BAD = -30981;
    public static final int DB_TXN_ABORT = 0;
    public static final int DB_TXN_APPLY = 1;
    public static final int DB_TXN_BACKWARD_ROLL = 3;
    public static final int DB_TXN_FORWARD_ROLL = 4;
    public static final int DB_TXN_PRINT = 8;
    public static final int DB_UNKNOWN = 5;
    public static final int DB_VERIFY_BAD = -30980;
    public static final int DB_AFTER;
    public static final int DB_AGGRESSIVE;
    public static final int DB_APPEND;
    public static final int DB_ARCH_ABS;
    public static final int DB_ARCH_DATA;
    public static final int DB_ARCH_LOG;
    public static final int DB_AUTO_COMMIT;
    public static final int DB_BEFORE;
    public static final int DB_CACHED_COUNTS;
    public static final int DB_CDB_ALLDB;
    public static final int DB_CHKSUM_SHA1;
    public static final int DB_CLIENT;
    public static final int DB_CONSUME;
    public static final int DB_CONSUME_WAIT;
    public static final int DB_CREATE;
    public static final int DB_CURRENT;
    public static final int DB_CXX_NO_EXCEPTIONS;
    public static final int DB_DBT_MALLOC;
    public static final int DB_DBT_PARTIAL;
    public static final int DB_DBT_REALLOC;
    public static final int DB_DBT_USERMEM;
    public static final int DB_DIRECT;
    public static final int DB_DIRECT_DB;
    public static final int DB_DIRECT_LOG;
    public static final int DB_DIRTY_READ;
    public static final int DB_DUP;
    public static final int DB_DUPSORT;
    public static final int DB_EID_BROADCAST;
    public static final int DB_EID_INVALID;
    public static final int DB_ENCRYPT;
    public static final int DB_ENCRYPT_AES;
    public static final int DB_EXCL;
    public static final int DB_FAST_STAT;
    public static final int DB_FIRST;
    public static final int DB_FLUSH;
    public static final int DB_FORCE;
    public static final int DB_GET_BOTH;
    public static final int DB_GET_BOTH_RANGE;
    public static final int DB_GET_RECNO;
    public static final int DB_INIT_CDB;
    public static final int DB_INIT_LOCK;
    public static final int DB_INIT_LOG;
    public static final int DB_INIT_MPOOL;
    public static final int DB_INIT_TXN;
    public static final int DB_JOINENV;
    public static final int DB_JOIN_ITEM;
    public static final int DB_JOIN_NOSORT;
    public static final int DB_KEYFIRST;
    public static final int DB_KEYLAST;
    public static final int DB_LAST;
    public static final int DB_LOCKDOWN;
    public static final int DB_LOCK_DEFAULT;
    public static final int DB_LOCK_EXPIRE;
    public static final int DB_LOCK_GET;
    public static final int DB_LOCK_GET_TIMEOUT;
    public static final int DB_LOCK_IREAD;
    public static final int DB_LOCK_IWR;
    public static final int DB_LOCK_IWRITE;
    public static final int DB_LOCK_MAXLOCKS;
    public static final int DB_LOCK_MINLOCKS;
    public static final int DB_LOCK_MINWRITE;
    public static final int DB_LOCK_NOWAIT;
    public static final int DB_LOCK_OLDEST;
    public static final int DB_LOCK_PUT;
    public static final int DB_LOCK_PUT_ALL;
    public static final int DB_LOCK_PUT_OBJ;
    public static final int DB_LOCK_RANDOM;
    public static final int DB_LOCK_READ;
    public static final int DB_LOCK_TIMEOUT;
    public static final int DB_LOCK_WRITE;
    public static final int DB_LOCK_YOUNGEST;
    public static final int DB_MULTIPLE;
    public static final int DB_MULTIPLE_KEY;
    public static final int DB_NEXT;
    public static final int DB_NEXT_DUP;
    public static final int DB_NEXT_NODUP;
    public static final int DB_NODUPDATA;
    public static final int DB_NOLOCKING;
    public static final int DB_NOMMAP;
    public static final int DB_NOORDERCHK;
    public static final int DB_NOOVERWRITE;
    public static final int DB_NOPANIC;
    public static final int DB_NOSYNC;
    public static final int DB_ODDFILESIZE;
    public static final int DB_ORDERCHKONLY;
    public static final int DB_OVERWRITE;
    public static final int DB_PANIC_ENVIRONMENT;
    public static final int DB_POSITION;
    public static final int DB_PREV;
    public static final int DB_PREV_NODUP;
    public static final int DB_PRINTABLE;
    public static final int DB_PRIORITY_DEFAULT;
    public static final int DB_PRIORITY_HIGH;
    public static final int DB_PRIORITY_LOW;
    public static final int DB_PRIORITY_VERY_HIGH;
    public static final int DB_PRIORITY_VERY_LOW;
    public static final int DB_PRIVATE;
    public static final int DB_RDONLY;
    public static final int DB_RECNUM;
    public static final int DB_RECORDCOUNT;
    public static final int DB_RECOVER;
    public static final int DB_RECOVER_FATAL;
    public static final int DB_REGION_INIT;
    public static final int DB_RENUMBER;
    public static final int DB_REP_CLIENT;
    public static final int DB_REP_LOGSONLY;
    public static final int DB_REP_MASTER;
    public static final int DB_REP_PERMANENT;
    public static final int DB_REP_UNAVAIL;
    public static final int DB_REVSPLITOFF;
    public static final int DB_RMW;
    public static final int DB_SALVAGE;
    public static final int DB_SET;
    public static final int DB_SET_LOCK_TIMEOUT;
    public static final int DB_SET_RANGE;
    public static final int DB_SET_RECNO;
    public static final int DB_SET_TXN_TIMEOUT;
    public static final int DB_SNAPSHOT;
    public static final int DB_STAT_CLEAR;
    public static final int DB_SYSTEM_MEM;
    public static final int DB_THREAD;
    public static final int DB_TRUNCATE;
    public static final int DB_TXN_NOSYNC;
    public static final int DB_TXN_NOWAIT;
    public static final int DB_TXN_SYNC;
    public static final int DB_TXN_WRITE_NOSYNC;
    public static final int DB_UPGRADE;
    public static final int DB_USE_ENVIRON;
    public static final int DB_USE_ENVIRON_ROOT;
    public static final int DB_VERB_CHKPOINT;
    public static final int DB_VERB_DEADLOCK;
    public static final int DB_VERB_RECOVERY;
    public static final int DB_VERB_REPLICATION;
    public static final int DB_VERB_WAITSFOR;
    public static final int DB_VERIFY;
    public static final int DB_VERSION_MAJOR;
    public static final int DB_VERSION_MINOR;
    public static final int DB_VERSION_PATCH;
    public static final int DB_WRITECURSOR;
    public static final int DB_XA_CREATE;
    public static final int DB_XIDDATASIZE;
    public static final int DB_YIELDCPU;
    // END-JAVA-SPECIAL-CONSTANTS

    // Note: the env can be null
    //
    public Db(DbEnv env, int flags)
        throws DbException
    {
        constructor_env_ = env;
        _init(env, flags);
        if (env == null) {
            dbenv_ = new DbEnv(this);
        }
        else {
            dbenv_ = env;
        }
        dbenv_._add_db(this);
    }

    //
    // Our parent DbEnv is notifying us that the environment is closing.
    //
    /*package*/ void _notify_dbenv_close()
    {
        dbenv_ = null;
        _notify_internal();
    }

    private native void _init(DbEnv env, int flags)
         throws DbException;

    private native void _notify_internal();

    // methods
    //

    public synchronized void associate(DbTxn txn, Db secondary,
                                       DbSecondaryKeyCreate key_creator,
                                       int flags)
        throws DbException
    {
        secondary.secondary_key_create_ = key_creator;
        _associate(txn, secondary, key_creator, flags);
    }

    public native void _associate(DbTxn txn, Db secondary,
                                  DbSecondaryKeyCreate key_creator, int flags)
        throws DbException;

    public synchronized int close(int flags)
        throws DbException
    {
        try {
            dbenv_._remove_db(this);
            return _close(flags);
        }
        finally {
            if (constructor_env_ == null) {
                dbenv_._notify_db_close();
            }
            dbenv_ = null;
        }
    }

    public native int _close(int flags)
         throws DbException;

    public native Dbc cursor(DbTxn txnid, int flags)
         throws DbException;

    public native int del(DbTxn txnid, Dbt key, int flags)
         throws DbException;

    public native void err(int errcode, String message);

    public native void errx(String message);

    public native int fd()
         throws DbException;

    // overrides Object.finalize
    protected void finalize()
        throws Throwable
    {
        if (dbenv_ == null)
            _finalize(null, null);
        else
            _finalize(dbenv_.errcall_, dbenv_.errpfx_);
    }

    protected native void _finalize(DbErrcall errcall, String errpfx)
        throws Throwable;

    // returns: 0, DB_NOTFOUND, or throws error
    public native int get(DbTxn txnid, Dbt key, Dbt data, int flags)
         throws DbException;

    public native boolean get_byteswapped();

    public native /*DBTYPE*/ int get_type();

    public native Dbc join(Dbc curslist[], int flags)
         throws DbException;

    public native void key_range(DbTxn txnid, Dbt key,
                                 DbKeyRange range, int flags)
         throws DbException;

    public synchronized void open(DbTxn txnid, String file,
                     String database, /*DBTYPE*/ int type,
                     int flags, int mode)
         throws DbException, FileNotFoundException
    {
        _open(txnid, file, database, type, flags, mode);
    }

    // (Internal)
    public native void _open(DbTxn txnid, String file,
                            String database, /*DBTYPE*/ int type,
                            int flags, int mode)
         throws DbException, FileNotFoundException;


    // returns: 0, DB_NOTFOUND, or throws error
    public native int pget(DbTxn txnid, Dbt key, Dbt pkey, Dbt data, int flags)
         throws DbException;

    // returns: 0, DB_KEYEXIST, or throws error
    public native int put(DbTxn txnid, Dbt key, Dbt data, int flags)
         throws DbException;

    public synchronized void rename(String file, String database,
                                    String newname, int flags)
        throws DbException, FileNotFoundException
    {
        try {
            _rename(file, database, newname, flags);
        }
        finally {
            if (constructor_env_ == null) {
                dbenv_._notify_db_close();
            }
            dbenv_ = null;
        }
    }

    public native void _rename(String file, String database,
                               String newname, int flags)
        throws DbException, FileNotFoundException;


    public synchronized void remove(String file,
                                           String database, int flags)
        throws DbException, FileNotFoundException
    {
        try {
            _remove(file, database, flags);
        }
        finally {
            if (constructor_env_ == null) {
                dbenv_._notify_db_close();
            }
            dbenv_ = null;
        }
    }

    public native void _remove(String file, String database,
                               int flags)
        throws DbException, FileNotFoundException;

    // Comparison function.
    public void set_append_recno(DbAppendRecno append_recno)
        throws DbException
    {
        append_recno_ = append_recno;
        append_recno_changed(append_recno);
    }

    // (Internal)
    private native void append_recno_changed(DbAppendRecno append_recno)
        throws DbException;

    // Comparison function.
    public void set_bt_compare(DbBtreeCompare bt_compare)
        throws DbException
    {
        bt_compare_ = bt_compare;
        bt_compare_changed(bt_compare);
    }

    // (Internal)
    private native void bt_compare_changed(DbBtreeCompare bt_compare)
        throws DbException;

    // Maximum keys per page.
    public native void set_bt_maxkey(int maxkey)
        throws DbException;

    // Minimum keys per page.
    public native void set_bt_minkey(int minkey)
        throws DbException;

    // Prefix function.
    public void set_bt_prefix(DbBtreePrefix bt_prefix)
        throws DbException
    {
        bt_prefix_ = bt_prefix;
        bt_prefix_changed(bt_prefix);
    }

    // (Internal)
    private native void bt_prefix_changed(DbBtreePrefix bt_prefix)
        throws DbException;

    // Set cache size
    public native void set_cachesize(int gbytes, int bytes, int ncaches)
        throws DbException;

    // Set cache priority
    public native void set_cache_priority(/* DB_CACHE_PRIORITY */ int priority)
        throws DbException;

    // Duplication resolution
    public void set_dup_compare(DbDupCompare dup_compare)
        throws DbException
    {
        dup_compare_ = dup_compare;
        dup_compare_changed(dup_compare);
    }

    // (Internal)
    private native void dup_compare_changed(DbDupCompare dup_compare)
        throws DbException;

    // Encryption
    public native void set_encrypt(String passwd, /*u_int32_t*/ int flags)
        throws DbException;

    // Error message callback.
    public void set_errcall(DbErrcall errcall)
    {
        if (dbenv_ != null)
            dbenv_.set_errcall(errcall);
    }

    // Error stream.
    public void set_error_stream(OutputStream s)
    {
        DbOutputStreamErrcall errcall = new DbOutputStreamErrcall(s);
        set_errcall(errcall);
    }

    // Error message prefix.
    public void set_errpfx(String errpfx)
    {
        if (dbenv_ != null)
            dbenv_.set_errpfx(errpfx);
    }


    // Feedback
    public void set_feedback(DbFeedback feedback)
        throws DbException
    {
        feedback_ = feedback;
        feedback_changed(feedback);
    }

    // (Internal)
    private native void feedback_changed(DbFeedback feedback)
        throws DbException;

    // Flags.
    public native void set_flags(/*u_int32_t*/ int flags)
        throws DbException;

    // Internal - only intended for testing purposes in the Java RPC server
    public native int get_flags_raw()
        throws DbException;

    // Fill factor.
    public native void set_h_ffactor(/*unsigned*/ int h_ffactor)
        throws DbException;

    // Hash function.
    public void set_h_hash(DbHash h_hash)
        throws DbException
    {
        h_hash_ = h_hash;
        hash_changed(h_hash);
    }

    // (Internal)
    private native void hash_changed(DbHash hash)
        throws DbException;

    // Number of elements.
    public native void set_h_nelem(/*unsigned*/ int h_nelem)
        throws DbException;

    // Byte order.
    public native void set_lorder(int lorder)
        throws DbException;

    // Underlying page size.
    public native void set_pagesize(/*size_t*/ long pagesize)
        throws DbException;

    // Variable-length delimiting byte.
    public native void set_re_delim(int re_delim)
        throws DbException;

    // Length for fixed-length records.
    public native void set_re_len(/*u_int32_t*/ int re_len)
        throws DbException;

    // Fixed-length padding byte.
    public native void set_re_pad(int re_pad)
        throws DbException;

    // Source file name.
    public native void set_re_source(String re_source)
        throws DbException;

    // Extent size of Queue
    public native void set_q_extentsize(/*u_int32_t*/ int extent_size)
        throws DbException;

    // returns a DbBtreeStat or DbHashStat
    public native Object stat(int flags)
         throws DbException;

    public native void sync(int flags)
         throws DbException;

    public native int truncate(DbTxn txnid, int flags)
         throws DbException;

    public native void upgrade(String name, int flags)
         throws DbException;

    public native void verify(String name, String subdb,
                              OutputStream outstr, int flags)
         throws DbException;

    ////////////////////////////////////////////////////////////////
    //
    // private data
    //
    private long private_dbobj_ = 0;
    private long private_info_ = 0;
    private DbEnv dbenv_ = null;
    private DbEnv constructor_env_ = null;
    private DbFeedback feedback_ = null;
    private DbAppendRecno append_recno_ = null;
    private DbBtreeCompare bt_compare_ = null;
    private DbBtreePrefix bt_prefix_ = null;
    private DbDupCompare dup_compare_ = null;
    private DbHash h_hash_ = null;
    private DbSecondaryKeyCreate secondary_key_create_ = null;

    ////////////////////////////////////////////////////////////////
    //
    // static methods and data that implement
    // loading the native library and doing any
    // extra sanity checks on startup.
    //
    private static boolean already_loaded_ = false;

    public static void load_db()
    {
        if (already_loaded_)
            return;

        // An alternate library name can be specified via a property.
        //
        String override;

        if ((override = System.getProperty("sleepycat.db.libfile")) != null) {
            System.load(override);
        }
        else if ((override = System.getProperty("sleepycat.db.libname")) != null) {
            System.loadLibrary(override);
        }
        else {
            String os = System.getProperty("os.name");
            if (os != null && os.startsWith("Windows")) {
                // library name is "libdb_java30.dll" (for example) on Win/*
                System.loadLibrary("libdb_java" +
                                   DbConstants.DB_VERSION_MAJOR +
                                   DbConstants.DB_VERSION_MINOR);
            }
            else {
                // library name is "libdb_java-3.0.so" (for example) on UNIX
                // Note: "db_java" isn't good enough;
                // some Unixes require us to use the explicit SONAME.
                System.loadLibrary("db_java-" +
                                   DbConstants.DB_VERSION_MAJOR + "." +
                                   DbConstants.DB_VERSION_MINOR);
            }
        }

        already_loaded_ = true;
    }

    static private native void one_time_init();

    static private void check_constant(int c1, int c2)
    {
        if (c1 != c2) {
            System.err.println("Db: constant mismatch");
            Thread.dumpStack();
            System.exit(1);
        }
    }

    static {
        Db.load_db();

        // BEGIN-JAVA-CONSTANT-INITIALIZATION
        /* DO NOT EDIT: automatically built by dist/s_java. */
        DB_AFTER = DbConstants.DB_AFTER;
        DB_AGGRESSIVE = DbConstants.DB_AGGRESSIVE;
        DB_APPEND = DbConstants.DB_APPEND;
        DB_ARCH_ABS = DbConstants.DB_ARCH_ABS;
        DB_ARCH_DATA = DbConstants.DB_ARCH_DATA;
        DB_ARCH_LOG = DbConstants.DB_ARCH_LOG;
        DB_AUTO_COMMIT = DbConstants.DB_AUTO_COMMIT;
        DB_BEFORE = DbConstants.DB_BEFORE;
        DB_CACHED_COUNTS = DbConstants.DB_CACHED_COUNTS;
        DB_CDB_ALLDB = DbConstants.DB_CDB_ALLDB;
        DB_CHKSUM_SHA1 = DbConstants.DB_CHKSUM_SHA1;
        DB_CLIENT = DbConstants.DB_CLIENT;
        DB_CONSUME = DbConstants.DB_CONSUME;
        DB_CONSUME_WAIT = DbConstants.DB_CONSUME_WAIT;
        DB_CREATE = DbConstants.DB_CREATE;
        DB_CURRENT = DbConstants.DB_CURRENT;
        DB_CXX_NO_EXCEPTIONS = DbConstants.DB_CXX_NO_EXCEPTIONS;
        DB_DBT_MALLOC = DbConstants.DB_DBT_MALLOC;
        DB_DBT_PARTIAL = DbConstants.DB_DBT_PARTIAL;
        DB_DBT_REALLOC = DbConstants.DB_DBT_REALLOC;
        DB_DBT_USERMEM = DbConstants.DB_DBT_USERMEM;
        DB_DIRECT = DbConstants.DB_DIRECT;
        DB_DIRECT_DB = DbConstants.DB_DIRECT_DB;
        DB_DIRECT_LOG = DbConstants.DB_DIRECT_LOG;
        DB_DIRTY_READ = DbConstants.DB_DIRTY_READ;
        DB_DUP = DbConstants.DB_DUP;
        DB_DUPSORT = DbConstants.DB_DUPSORT;
        DB_EID_BROADCAST = DbConstants.DB_EID_BROADCAST;
        DB_EID_INVALID = DbConstants.DB_EID_INVALID;
        DB_ENCRYPT = DbConstants.DB_ENCRYPT;
        DB_ENCRYPT_AES = DbConstants.DB_ENCRYPT_AES;
        DB_EXCL = DbConstants.DB_EXCL;
        DB_FAST_STAT = DbConstants.DB_FAST_STAT;
        DB_FIRST = DbConstants.DB_FIRST;
        DB_FLUSH = DbConstants.DB_FLUSH;
        DB_FORCE = DbConstants.DB_FORCE;
        DB_GET_BOTH = DbConstants.DB_GET_BOTH;
        DB_GET_BOTH_RANGE = DbConstants.DB_GET_BOTH_RANGE;
        DB_GET_RECNO = DbConstants.DB_GET_RECNO;
        DB_INIT_CDB = DbConstants.DB_INIT_CDB;
        DB_INIT_LOCK = DbConstants.DB_INIT_LOCK;
        DB_INIT_LOG = DbConstants.DB_INIT_LOG;
        DB_INIT_MPOOL = DbConstants.DB_INIT_MPOOL;
        DB_INIT_TXN = DbConstants.DB_INIT_TXN;
        DB_JOINENV = DbConstants.DB_JOINENV;
        DB_JOIN_ITEM = DbConstants.DB_JOIN_ITEM;
        DB_JOIN_NOSORT = DbConstants.DB_JOIN_NOSORT;
        DB_KEYFIRST = DbConstants.DB_KEYFIRST;
        DB_KEYLAST = DbConstants.DB_KEYLAST;
        DB_LAST = DbConstants.DB_LAST;
        DB_LOCKDOWN = DbConstants.DB_LOCKDOWN;
        DB_LOCK_DEFAULT = DbConstants.DB_LOCK_DEFAULT;
        DB_LOCK_EXPIRE = DbConstants.DB_LOCK_EXPIRE;
        DB_LOCK_GET = DbConstants.DB_LOCK_GET;
        DB_LOCK_GET_TIMEOUT = DbConstants.DB_LOCK_GET_TIMEOUT;
        DB_LOCK_IREAD = DbConstants.DB_LOCK_IREAD;
        DB_LOCK_IWR = DbConstants.DB_LOCK_IWR;
        DB_LOCK_IWRITE = DbConstants.DB_LOCK_IWRITE;
        DB_LOCK_MAXLOCKS = DbConstants.DB_LOCK_MAXLOCKS;
        DB_LOCK_MINLOCKS = DbConstants.DB_LOCK_MINLOCKS;
        DB_LOCK_MINWRITE = DbConstants.DB_LOCK_MINWRITE;
        DB_LOCK_NOWAIT = DbConstants.DB_LOCK_NOWAIT;
        DB_LOCK_OLDEST = DbConstants.DB_LOCK_OLDEST;
        DB_LOCK_PUT = DbConstants.DB_LOCK_PUT;
        DB_LOCK_PUT_ALL = DbConstants.DB_LOCK_PUT_ALL;
        DB_LOCK_PUT_OBJ = DbConstants.DB_LOCK_PUT_OBJ;
        DB_LOCK_RANDOM = DbConstants.DB_LOCK_RANDOM;
        DB_LOCK_READ = DbConstants.DB_LOCK_READ;
        DB_LOCK_TIMEOUT = DbConstants.DB_LOCK_TIMEOUT;
        DB_LOCK_WRITE = DbConstants.DB_LOCK_WRITE;
        DB_LOCK_YOUNGEST = DbConstants.DB_LOCK_YOUNGEST;
        DB_MULTIPLE = DbConstants.DB_MULTIPLE;
        DB_MULTIPLE_KEY = DbConstants.DB_MULTIPLE_KEY;
        DB_NEXT = DbConstants.DB_NEXT;
        DB_NEXT_DUP = DbConstants.DB_NEXT_DUP;
        DB_NEXT_NODUP = DbConstants.DB_NEXT_NODUP;
        DB_NODUPDATA = DbConstants.DB_NODUPDATA;
        DB_NOLOCKING = DbConstants.DB_NOLOCKING;
        DB_NOMMAP = DbConstants.DB_NOMMAP;
        DB_NOORDERCHK = DbConstants.DB_NOORDERCHK;
        DB_NOOVERWRITE = DbConstants.DB_NOOVERWRITE;
        DB_NOPANIC = DbConstants.DB_NOPANIC;
        DB_NOSYNC = DbConstants.DB_NOSYNC;
        DB_ODDFILESIZE = DbConstants.DB_ODDFILESIZE;
        DB_ORDERCHKONLY = DbConstants.DB_ORDERCHKONLY;
        DB_OVERWRITE = DbConstants.DB_OVERWRITE;
        DB_PANIC_ENVIRONMENT = DbConstants.DB_PANIC_ENVIRONMENT;
        DB_POSITION = DbConstants.DB_POSITION;
        DB_PREV = DbConstants.DB_PREV;
        DB_PREV_NODUP = DbConstants.DB_PREV_NODUP;
        DB_PRINTABLE = DbConstants.DB_PRINTABLE;
        DB_PRIORITY_DEFAULT = DbConstants.DB_PRIORITY_DEFAULT;
        DB_PRIORITY_HIGH = DbConstants.DB_PRIORITY_HIGH;
        DB_PRIORITY_LOW = DbConstants.DB_PRIORITY_LOW;
        DB_PRIORITY_VERY_HIGH = DbConstants.DB_PRIORITY_VERY_HIGH;
        DB_PRIORITY_VERY_LOW = DbConstants.DB_PRIORITY_VERY_LOW;
        DB_PRIVATE = DbConstants.DB_PRIVATE;
        DB_RDONLY = DbConstants.DB_RDONLY;
        DB_RECNUM = DbConstants.DB_RECNUM;
        DB_RECORDCOUNT = DbConstants.DB_RECORDCOUNT;
        DB_RECOVER = DbConstants.DB_RECOVER;
        DB_RECOVER_FATAL = DbConstants.DB_RECOVER_FATAL;
        DB_REGION_INIT = DbConstants.DB_REGION_INIT;
        DB_RENUMBER = DbConstants.DB_RENUMBER;
        DB_REP_CLIENT = DbConstants.DB_REP_CLIENT;
        DB_REP_LOGSONLY = DbConstants.DB_REP_LOGSONLY;
        DB_REP_MASTER = DbConstants.DB_REP_MASTER;
        DB_REP_PERMANENT = DbConstants.DB_REP_PERMANENT;
        DB_REP_UNAVAIL = DbConstants.DB_REP_UNAVAIL;
        DB_REVSPLITOFF = DbConstants.DB_REVSPLITOFF;
        DB_RMW = DbConstants.DB_RMW;
        DB_SALVAGE = DbConstants.DB_SALVAGE;
        DB_SET = DbConstants.DB_SET;
        DB_SET_LOCK_TIMEOUT = DbConstants.DB_SET_LOCK_TIMEOUT;
        DB_SET_RANGE = DbConstants.DB_SET_RANGE;
        DB_SET_RECNO = DbConstants.DB_SET_RECNO;
        DB_SET_TXN_TIMEOUT = DbConstants.DB_SET_TXN_TIMEOUT;
        DB_SNAPSHOT = DbConstants.DB_SNAPSHOT;
        DB_STAT_CLEAR = DbConstants.DB_STAT_CLEAR;
        DB_SYSTEM_MEM = DbConstants.DB_SYSTEM_MEM;
        DB_THREAD = DbConstants.DB_THREAD;
        DB_TRUNCATE = DbConstants.DB_TRUNCATE;
        DB_TXN_NOSYNC = DbConstants.DB_TXN_NOSYNC;
        DB_TXN_NOWAIT = DbConstants.DB_TXN_NOWAIT;
        DB_TXN_SYNC = DbConstants.DB_TXN_SYNC;
        DB_TXN_WRITE_NOSYNC = DbConstants.DB_TXN_WRITE_NOSYNC;
        DB_UPGRADE = DbConstants.DB_UPGRADE;
        DB_USE_ENVIRON = DbConstants.DB_USE_ENVIRON;
        DB_USE_ENVIRON_ROOT = DbConstants.DB_USE_ENVIRON_ROOT;
        DB_VERB_CHKPOINT = DbConstants.DB_VERB_CHKPOINT;
        DB_VERB_DEADLOCK = DbConstants.DB_VERB_DEADLOCK;
        DB_VERB_RECOVERY = DbConstants.DB_VERB_RECOVERY;
        DB_VERB_REPLICATION = DbConstants.DB_VERB_REPLICATION;
        DB_VERB_WAITSFOR = DbConstants.DB_VERB_WAITSFOR;
        DB_VERIFY = DbConstants.DB_VERIFY;
        DB_VERSION_MAJOR = DbConstants.DB_VERSION_MAJOR;
        DB_VERSION_MINOR = DbConstants.DB_VERSION_MINOR;
        DB_VERSION_PATCH = DbConstants.DB_VERSION_PATCH;
        DB_WRITECURSOR = DbConstants.DB_WRITECURSOR;
        DB_XA_CREATE = DbConstants.DB_XA_CREATE;
        DB_XIDDATASIZE = DbConstants.DB_XIDDATASIZE;
        DB_YIELDCPU = DbConstants.DB_YIELDCPU;
        // END-JAVA-CONSTANT-INITIALIZATION

        one_time_init();
    }
}
// end of Db.java
