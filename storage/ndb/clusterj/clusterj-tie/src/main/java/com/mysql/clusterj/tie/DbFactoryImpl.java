/*
 *  Copyright (c) 2024, 2026, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is designed to work with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have either included with
 *  the program or referenced in the documentation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License, version 2.0, for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj.tie;

import com.mysql.clusterj.core.store.Db;
import com.mysql.clusterj.core.store.DbFactory;
import com.mysql.clusterj.core.store.Index;
import com.mysql.clusterj.core.store.Table;

import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import com.mysql.ndbjtie.ndbapi.Ndb;
import com.mysql.ndbjtie.ndbapi.NdbDictionary;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;
import java.util.IdentityHashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

public class DbFactoryImpl implements DbFactory {

   /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(DbFactoryImpl.class);

    protected ClusterConnectionImpl connectionImpl;

    protected final String databaseName;

    private boolean isClosing = false;

   /** The DbImplForNdbRecord */
    DbImplForNdbRecord dbForNdbRecord;

    /** The map of table name to NdbRecordImpl */
    private ConcurrentMap<String, NdbRecordImpl> ndbRecordImplMap =
        new ConcurrentHashMap<String, NdbRecordImpl>();

    /** List of obsolete NdbRecordImpl to be cleaned upon close() */
    private List<NdbRecordImpl> deadNdbRecords = new ArrayList<NdbRecordImpl>();

    /** All regular dbs (not dbForNdbRecord) given out */
    private Map<DbImpl, Object> dbs =
        Collections.synchronizedMap(new IdentityHashMap<DbImpl, Object>());

    /** The dictionary used to create NdbRecords */
    NdbDictionary.Dictionary dictionaryForNdbRecord = null;

    /** The byte buffer pool */
    private VariableByteBufferPoolImpl byteBufferPool;

    /* The session cache is a queue of DbImplCore items */
    final private ArrayDeque<DbImplCore> sessionCache =
        new ArrayDeque<DbImplCore>();

    /* Maximum size of this instance's session cache */
    int localMaxCacheSize;

    /* Current target size of the session cache */
    int targetCacheSize;

    /* Dictionary wait/retry time for table not found in getTable() */
    int tableWaitMsec;

    protected DbFactoryImpl(ClusterConnectionImpl conn, String dbName,
                            int[] byteBufferPoolSizes) {
        connectionImpl = conn;
        isClosing = false;
        databaseName = dbName;
        byteBufferPool = VariableByteBufferPoolImpl.getPool(byteBufferPoolSizes);
    }

    public VariableByteBufferPoolImpl getByteBufferPool() {
        return byteBufferPool;
    }

    public void useSessionCache(int size) {
        localMaxCacheSize = size;
        GlobalCacheRegistry.increaseLimit(size);
    }

    public void setTableWaitTime(int waitMsec) {
        tableWaitMsec = waitMsec;
    }

    public int getTableWaitTime() {
        return tableWaitMsec;
    }

    public Db createDb(int maxTransactions) {
        DbImpl db;
        DbImplCore item = getCachedNdb(maxTransactions);

        if(item == null)
            db = connectionImpl.createDbImplForFactory(this, maxTransactions);
        else {
            db = new DbImpl(this, item);
            connectionImpl.initializeAutoIncrement(db);
        }

        if (dictionaryForNdbRecord == null) {
            dbForNdbRecord = connectionImpl.createDbForNdbRecord(this);
            dbForNdbRecord.initializeQueryObjects();
            dictionaryForNdbRecord = dbForNdbRecord.getNdbDictionary();
        }
        dbs.put(db, null);
        return db;
    }

    /**
     * Get the cached NdbRecord implementation for the table
     * used with this cluster connection. All columns are included
     * in the NdbRecord.
     * Use a ConcurrentHashMap for best multithread performance.
     * There are three possibilities:
     *   - Case 1: return the already-cached NdbRecord
     *   - Case 2: return a new instance created by this method
     *   - Case 3: return the winner of a race with another thread
     *
     * In cases 2 and 3, creation of the new instance is synchronized on the
     * cluster connection, because of concurrency limits in jtie & ndbapi.
     *
     * @param storeTable the store table
     * @return the NdbRecordImpl for the table
     */
    protected NdbRecordImpl getCachedNdbRecordImpl(Table storeTable) {
        dbForNdbRecord.assertNotClosed("ClusterConnectionImpl.getCachedNdbRecordImpl for table");
        // tableKey is table name plus projection indicator
        String tableName = storeTable.getKey();
        // find the NdbRecordImpl in the global cache
        NdbRecordImpl result = ndbRecordImplMap.get(tableName);
        if (result != null) {
            // case 1
            if (logger.isDebugEnabled())logger.debug("NdbRecordImpl found for " + tableName);
            return result;
        } else {
            // dictionary is single thread
            NdbRecordImpl newNdbRecordImpl;
            synchronized (connectionImpl) {
                // try again; another thread might have beat us
                result = ndbRecordImplMap.get(tableName);
                if (result != null) {
                    return result;
                }
                newNdbRecordImpl = new NdbRecordImpl(storeTable, dictionaryForNdbRecord);
            }
            NdbRecordImpl winner = ndbRecordImplMap.putIfAbsent(tableName, newNdbRecordImpl);
            if (winner == null) {
                // case 2: the previous value was null, so return the new (winning) value
                if (logger.isDebugEnabled())logger.debug("NdbRecordImpl created for " + tableName);
                return newNdbRecordImpl;
            } else {
                // case 3: another thread beat us, so return the winner and garbage collect ours
                if (logger.isDebugEnabled())logger.debug("NdbRecordImpl lost race for " + tableName);
                newNdbRecordImpl.releaseNdbRecord();
                return winner;
            }
        }
    }

    /**
     * Get the cached NdbRecord implementation for the index and table
     * used with this cluster connection.
     * The NdbRecordImpl is cached under the name tableName+indexName.
     * Only the key columns are included in the NdbRecord.
     * Use a ConcurrentHashMap for best multithread performance.
     * There are three possibilities:
     *  - Case 1: return the already-cached NdbRecord
     *  - Case 2: return a new instance created by this method
     *  - Case 3: return the winner of a race with another thread
     *
     * @param storeTable the store table
     * @param storeIndex the store index
     * @return the NdbRecordImpl for the index
     */
    protected NdbRecordImpl getCachedNdbRecordImpl(Index storeIndex, Table storeTable) {
        dbForNdbRecord.assertNotClosed("ClusterConnectionImpl.getCachedNdbRecordImpl for index");
        String recordName = storeTable.getName() + "+" + storeIndex.getInternalName();
        // find the NdbRecordImpl in the global cache
        NdbRecordImpl result = ndbRecordImplMap.get(recordName);
        if (result != null) {
            // case 1
            if (logger.isDebugEnabled())logger.debug("NdbRecordImpl found for " + recordName);
            return result;
        } else {
            // dictionary is single thread
            NdbRecordImpl newNdbRecordImpl;
            synchronized (connectionImpl) {
                // try again; another thread might have beat us
                result = ndbRecordImplMap.get(recordName);
                if (result != null) {
                    return result;
                }
                newNdbRecordImpl = new NdbRecordImpl(storeIndex, storeTable, dictionaryForNdbRecord);
            }
            NdbRecordImpl winner = ndbRecordImplMap.putIfAbsent(recordName, newNdbRecordImpl);
            if (winner == null) {
                // case 2: the previous value was null, so return the new (winning) value
                if (logger.isDebugEnabled())logger.debug("NdbRecordImpl created for " + recordName);
                return newNdbRecordImpl;
            } else {
                // case 3: another thread beat us, so return the winner and garbage collect ours
                if (logger.isDebugEnabled())logger.debug("NdbRecordImpl lost race for " + recordName);
                newNdbRecordImpl.releaseNdbRecord();
                return winner;
            }
        }
    }

    /** Remove the cached NdbRecord(s) associated with this table. This allows schema change to work.
     * All NdbRecords including any index NdbRecords will be removed. Index NdbRecords are named
     * tableName+indexName. Cached schema objects in NdbDictionary are also removed.
     * This method should be called by the application after receiving an exception that indicates
     * that the table definition has changed since the metadata was loaded. Such changes as
     * truncate table or dropping indexes, columns, or tables may cause errors reported
     * to the application, including code 241 "Invalid schema object version" and
     * code 284 "Unknown table error in transaction coordinator".
     * @param tableName the name of the table
     */
    public void unloadSchema(String tableName) {
        boolean haveCachedTable = false;
        synchronized(connectionImpl) {
            Iterator<Map.Entry<String, NdbRecordImpl>> iterator = ndbRecordImplMap.entrySet().iterator();
            while (iterator.hasNext()) {
                Map.Entry<String, NdbRecordImpl> entry = iterator.next();
                String key = entry.getKey();
                if (key.startsWith(tableName)) {
                    haveCachedTable = true;
                    // entries are of the form:
                    //   tableName or
                    //   tableName+indexName
                    // split tableName[+indexName] into one or two parts
                    // the "\" character is escaped once for Java and again for regular expression to escape +
                    String[] tablePlusIndex = key.split("\\+");
                    if (tablePlusIndex.length >1) {
                        String indexName = tablePlusIndex[1];
                        if (logger.isDebugEnabled())logger.debug("Removing dictionary entry for cached index " +
                                tableName + " " + indexName);
                        dictionaryForNdbRecord.invalidateIndex(indexName, tableName);
                    }
                    // remove all records whose key begins with the table name; this will remove index records also
                    if (logger.isDebugEnabled())logger.debug("Removing cached NdbRecord for " + key);
                    NdbRecordImpl record = entry.getValue();
                    iterator.remove();
                    if (record != null) {
                        deadNdbRecords.add(record);
                    }
                }
            }
            // invalidate cached dictionary table after invalidate cached indexes
            if (haveCachedTable) {
                if (logger.isDebugEnabled())logger.debug("Removing dictionary entry for cached table " + tableName);
                dictionaryForNdbRecord.invalidateTable(tableName);
            }
        }
    }

    private void sleep(long millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    public void closing() {
        if (dbs.size() > 0) {
            for (DbImpl db: dbs.keySet()) {
                // mark all dbs as closing so no more operations will start
                db.closing();
            }
        }
        if (dbForNdbRecord != null) {
            dbForNdbRecord.closing();
        }
        closeSessionCache();
        this.isClosing = true;
    }

    public void close() {
        if (!this.isClosing) {
            this.closing();
            // sleep 1000 msec to allow operations in other threads to terminate
            sleep(1000);
        }

        if (dbs.size() != 0) {
            Map<Db, Object> dbsToClose = new IdentityHashMap<Db, Object>(dbs);
            for (Db db: dbsToClose.keySet()) {
                db.close();
            }
        }
        for (NdbRecordImpl ndbRecord: ndbRecordImplMap.values()) {
            ndbRecord.releaseNdbRecord();
        }
        for (NdbRecordImpl ndbRecord: deadNdbRecords) {
            ndbRecord.releaseNdbRecord();
        }
        if (dbForNdbRecord != null) {
            dbForNdbRecord.close();
            dbForNdbRecord = null;
        }
        ndbRecordImplMap.clear();
        deadNdbRecords.clear();
        GlobalCacheRegistry.returnLease(targetCacheSize);
        targetCacheSize = 0;
    }

    public void closeDb(Db db) {
        dbs.remove(db);
    }

    public int dbCount() {
        // dbForNdbRecord is not included in the dbs list
        return dbs.size();
    }

    private boolean cacheEnabled() {
        return localMaxCacheSize > 0;
    }

    /* Get an Ndb object from the cache */
    private DbImplCore getCachedNdb(int maxTransactions) {
        DbImplCore node = null;
        synchronized (sessionCache) {
            if(cacheEnabled()) {
                node = sessionCache.pollFirst();
                if(node == null) {
                    /* After a miss, ask for one more cache space */
                    if(targetCacheSize < localMaxCacheSize)
                        targetCacheSize = GlobalCacheRegistry.requestLease(targetCacheSize, 1);
                } else if(node.maxTransactions < maxTransactions) {
                    node.delete(); // just discard it
                    node = null;
                }
            }
        }
        return node;
    }

    /* Return an Ndb object to the cache */
    void returnNdb(DbImpl db) {
        if(! cacheEnabled()) {
            db.delete();
            return;
        }

        // Construct a little DbImplCore for the cache
        DbImplCore item = new DbImplCore(db);

        // Put items on a list to avoid calling into NDBAPI with lock held
        ArrayList<DbImplCore> itemsToDelete = new ArrayList<DbImplCore>();

        synchronized (sessionCache) {
            /* Push item onto queue */
            if(sessionCache.size() <= targetCacheSize) {
                sessionCache.push(item);
            } else {
                itemsToDelete.add(item);
            }

            /* Possibly adjust the target size */
            if(GlobalCacheRegistry.shouldShrinkCache(targetCacheSize))
                targetCacheSize = GlobalCacheRegistry.requestLease(targetCacheSize, -1);

            /* Expire off the end of the queue */
            while(sessionCache.size() > targetCacheSize) {
                itemsToDelete.add(sessionCache.pollLast());
            }
        }

        itemsToDelete.forEach((i) -> i.delete());
        itemsToDelete.clear();
    }

    /* Purge the cache */
    private void closeSessionCache() {
        synchronized(sessionCache) {
            localMaxCacheSize = 0;  // disable the cache
            logger.debug(() -> "Purging " + sessionCache.size() + " Ndb objects from cache");
            DbImplCore node = sessionCache.pollLast();
            while(node != null) {
                node.delete();
                node = sessionCache.pollLast();
            }
        }
    }
}


/* Singleton class GlobalCacheRegistry maintains the global limit on the
   number of cached sessions by giving out leases to each DbFactoryImpl
*/
class GlobalCacheRegistry {

    /* The size limit */
    static private int sizeLimit = 0;

    /* The current total allocation */
    static private AtomicInteger currentTotalSize = new AtomicInteger();

    /* Counter used in shouldShrinkCache() */
    static private AtomicInteger counter = new AtomicInteger();

    /* Set the limit */
    static synchronized void increaseLimit(int n) {
        if(sizeLimit < n) sizeLimit = n;
    }

    /* Request a lease */
    static int requestLease(int currentLeaseSize, int difference) {
        assert sizeLimit > 0;

        int allocation = currentLeaseSize + difference;  // optimistic
        int newSize = currentTotalSize.addAndGet(difference);
        if(newSize < 0) {
            assert false;
            currentTotalSize.set(0);
            allocation = 0;
        } else if(newSize > sizeLimit) {
            currentTotalSize.set(sizeLimit);
            allocation -= (newSize - sizeLimit);
        }
        assert allocation >= 0;
        return allocation;
    }

    /* Return an allocation; called when closing */
    static void returnLease(int size) {
        int newSize = currentTotalSize.addAndGet(-size);
        assert newSize >= 0;
    }

    /* Heuristically check whether a current lease could be smaller.
       Returns true if the DbFactory should decrement its lease size.

       The algorithm for local cache management is implemented here,
       in getCachedNdb(), and in returnNdb(). In summary:

        * Each DbFactory begins with a lease of 0.
        * On a cache miss, in getCachedNdb(), if the lease is less than the
          local max, increment the lease.
        * In releaseNdb(), if the lease is greater than 3 and also greater
          than twice the number of active threads in the ThreadGroup,
          decrement the lease. However, getting the thread count is
          expensive, so sometimes skip this test.
    */
    static boolean shouldShrinkCache(int currentLeaseSize) {
        if (currentLeaseSize <= 3) return false;
        if ((counter.incrementAndGet() & 0x7) != 0) return false;
        if (Thread.activeCount() * 2 < currentLeaseSize) return true;
        return false;
    }
}
