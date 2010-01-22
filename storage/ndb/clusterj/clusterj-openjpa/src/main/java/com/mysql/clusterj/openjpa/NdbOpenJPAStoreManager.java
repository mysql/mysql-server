/*
   Copyright (C) 2009 Sun Microsystems Inc.
   All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj.openjpa;

import java.sql.SQLException;
import java.util.ArrayList;
import java.util.BitSet;
import java.util.Collection;
import java.util.Map;

import org.apache.openjpa.jdbc.kernel.ConnectionInfo;
import org.apache.openjpa.jdbc.kernel.JDBCFetchConfiguration;
import org.apache.openjpa.jdbc.kernel.JDBCStoreManager;
import org.apache.openjpa.jdbc.meta.ClassMapping;
import org.apache.openjpa.jdbc.meta.ValueMapping;
import org.apache.openjpa.jdbc.sql.Result;
import org.apache.openjpa.kernel.FetchConfiguration;
import org.apache.openjpa.kernel.OpenJPAStateManager;
import org.apache.openjpa.kernel.PCState;
import org.apache.openjpa.kernel.QueryLanguages;
import org.apache.openjpa.kernel.StoreContext;
import org.apache.openjpa.kernel.StoreQuery;
import org.apache.openjpa.kernel.exps.ExpressionParser;
import org.apache.openjpa.meta.ClassMetaData;
import org.apache.openjpa.meta.FieldMetaData;
import org.apache.openjpa.util.OpenJPAId;

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.SessionFactory;
import com.mysql.clusterj.Transaction;
import com.mysql.clusterj.core.query.QueryExecutionContextImpl;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.SessionSPI;
import com.mysql.clusterj.core.spi.ValueHandler;
import com.mysql.clusterj.core.store.Dictionary;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import com.mysql.clusterj.query.QueryDomainType;

/**
 *
 */
public class NdbOpenJPAStoreManager extends JDBCStoreManager {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(NdbOpenJPAStoreManager.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(NdbOpenJPAStoreManager.class);

    @SuppressWarnings("unused")
    private StoreContext storeContext;
    private NdbOpenJPAConfiguration ndbConfiguration;
    private SessionFactory sessionFactory;

    // TODO This really belongs in store context.
    private SessionSPI session;
    private Transaction tx;
    private Dictionary dictionary;

    public NdbOpenJPAStoreManager() {
        super();
    }

    @Override
    public void setContext(StoreContext ctx) {
        super.setContext(ctx);
        setContext(ctx, (NdbOpenJPAConfiguration) ctx.getConfiguration());
    }

    public void setContext(StoreContext ctx, NdbOpenJPAConfiguration conf) {
        storeContext = ctx;
        ndbConfiguration = conf;
        sessionFactory = conf.getSessionFactory();
    }

    protected NdbOpenJPADomainTypeHandlerImpl<?> getDomainTypeHandler(OpenJPAStateManager sm) {
        // get DomainTypeHandler from StateManager
        ClassMapping cmp = (ClassMapping) sm.getMetaData();
        return getDomainTypeHandler(cmp);
    }

    protected NdbOpenJPADomainTypeHandlerImpl<?> getDomainTypeHandler(ClassMapping cmp) {
        NdbOpenJPADomainTypeHandlerImpl<?> domainTypeHandler =
            ndbConfiguration.getDomainTypeHandler(cmp, dictionary);
        return domainTypeHandler;
    }

    protected int deleteAll(DomainTypeHandler<?> base) {
        // used by NdbOpenJPAStoreQuery to delete all instances of a class
        return session.deletePersistentAll(base);
    }

    private SessionSPI getSession() {
        if (session == null) {
            session = (SessionSPI) sessionFactory.getSession();
            dictionary = session.getDictionary();
        }
        return session;
    }
    /**
     * Find the object with the given oid.
     */
    @Override
    public Object find(Object oid, ValueMapping vm,
        JDBCFetchConfiguration fetch) {
        if (logger.isDebugEnabled()) {
            logger.debug("NdbStoreManager.find(Object oid, ValueMapping vm, JDBCFetchConfiguration fetch) delegated to super.");
        }
        return super.find(oid, vm, fetch);
    }

    /** Load the fields for the persistent instance owned by the sm.
     * This should only be called for relationship fields, since non-relationship fields
     * are loaded eagerly when the instance is fetched or queried.
     * @param sm the StateManager
     * @param fields the fields to load
     * @param fetch the FetchConfiguration
     * @param lockLevel the lock level to use when getting data
     * @param context the StoreContext
     * @return true if any field was loaded
     */
    @Override
    public boolean load(OpenJPAStateManager sm, BitSet fields,
            FetchConfiguration fetch, int lockLevel, Object context) {
        if (logger.isDebugEnabled()) {
            logger.debug("NdbStoreManager.load(OpenJPAStateManager sm, BitSet fields, FetchConfiguration fetch, int lockLevel, Object context)...");
            logger.debug("Id: " + sm.getId() + " requested fields: " + printBitSet(sm, fields));
        }
        NdbOpenJPADomainTypeHandlerImpl<?> domainTypeHandler = getDomainTypeHandler(sm);
        try {
            return domainTypeHandler.load(sm, this, fields, (JDBCFetchConfiguration) fetch, context);
        } catch (SQLException sQLException) {
            logger.error("Fatal error from NdbOpenJPAStoreManager.load " + sQLException);
            sQLException.printStackTrace();
            return false;
        }
        //return super.load(sm, fields, fetch, lockLevel, context);
    }

    @Override
    public Object load(ClassMapping mapping, JDBCFetchConfiguration fetch,
        BitSet exclude, Result result) throws SQLException {
        if (logger.isDebugEnabled()) {
            logger.debug("NdbStoreManager.load(ClassMapping mapping, JDBCFetchConfiguration fetch, BitSet exclude, Result result) for " +  mapping.getDescribedType().getName() + " delegated to super.");
        }
        return super.load(mapping, fetch, exclude, result);
    }

    @SuppressWarnings("unchecked")
    @Override
    public Collection loadAll(Collection sms, PCState state, int load,
        FetchConfiguration fetch, Object context) {
        if (logger.isDebugEnabled()) {
            logger.debug("NdbStoreManager.loadAll(Collection sms, PCState state, int load, FetchConfiguration fetch, Object context) delegated to super.");
        }
        return super.loadAll(sms, state, load, fetch, context);
    }

    @Override
    public boolean initialize(OpenJPAStateManager sm, PCState state,
        FetchConfiguration fetch, Object context) {
        if (logger.isDebugEnabled()) {
            logger.debug("NdbStoreManager.initialize(OpenJPAStateManager sm, PCState state, FetchConfiguration fetch, Object context)");
        }
        // TODO: support user-defined oid types
        OpenJPAId id = (OpenJPAId)sm.getId();
        if (logger.isTraceEnabled()) {
            logger.trace("Id: " + id.getClass() + " " + id);
        }

        try {
            // get session from session factory
            getSession();
            // get domain type handler for StateManager
            NdbOpenJPADomainTypeHandlerImpl<?> domainTypeHandler =
                    getDomainTypeHandler(sm);
//                Object instance = session.initializeFromDatabase(
//                    domainTypeHandler, null,
//                    domainTypeHandler.getValueHandler(sm),
//                    domainTypeHandler.createKeyValueHandler(id.getIdObject()));
                // initialize via OpenJPA protocol
                // select all columns from table
                ResultData resultData = session.selectUnique(domainTypeHandler,
                        domainTypeHandler.createKeyValueHandler(id.getIdObject()),
                        null);
                // create an OpenJPA Result from the ndb result data
                NdbOpenJPAResult result = new NdbOpenJPAResult(resultData, domainTypeHandler, null);
                if (result.next()) {
                    // we have an instance; create the PC instance
                    domainTypeHandler.newInstance(sm);
                    // for each field, call its handler to initialize the field
                    // TODO: should compare using this technique against
                    // using clusterj directly (see above) since
                    // there is a lot more overhead using the openjpa technique
                    domainTypeHandler.load(sm, this, (JDBCFetchConfiguration)fetch, result);
//                    NdbOpenJPADomainFieldHandlerImpl[] fieldHandlers = domainTypeHandler.getDomainFieldHandlers();
//                    for (NdbOpenJPADomainFieldHandlerImpl fmd:fieldHandlers) {
//                           if (true) {
//        //                     if (fmd.isToOne()) {
//                            FieldMapping fm = fmd.getFieldMapping();
//                            fm.load(sm, this, (JDBCFetchConfiguration)fetch, result);
//                        }
//        //                  fmd.load(sm, fetch, result);
//                    }
                }
            if (logger.isDetailEnabled()) {
                logger.detail("After initializing PCState: " +
                        sm.getPCState().getClass().getSimpleName() + " " +
                        printLoaded(sm));
            }
            return true;

        } catch (Exception e) {
            e.printStackTrace();
            // if any problem, fall back
            return super.initialize(sm, state, fetch, context);
        }
    }

    @Override
    protected boolean initializeState(OpenJPAStateManager sm, PCState state,
            JDBCFetchConfiguration fetch, ConnectionInfo info)
            throws ClassNotFoundException, SQLException {
        if (logger.isDebugEnabled()) {
            logger.debug("NdbStoreManager.initializeState(" +
                    "OpenJPAStateManager, PCState, JDBCFetchConfiguration, " +
                    "ConnectionInfo) delegated to super.");
        }
        return super.initializeState(sm, state, fetch, info);
    }

    /**
     * Flush the given state manager collection to the datastore, returning
     * a collection of exceptions encountered during flushing.
     * The given collection may include states that do not require data
     * store action, such as persistent-clean instances or persistent-dirty
     * instances that have not been modified since they were last flushed.
     * For datastore updates and inserts, the dirty, non-flushed fields of
     * each state should be flushed. New instances without an assigned object
     * id should be given one via {@link OpenJPAStateManager#setObjectId}. New
     * instances with value-strategy fields that have not been assigned yet
     * should have their fields set. Datastore version information should be
     * updated during flush, and the state manager's version indicator
     * updated through the {@link OpenJPAStateManager#setNextVersion} method.
     * The current version will roll over to this next version upon successful
     * commit.
     */
    @SuppressWarnings("unchecked")
    @Override
    public Collection<Exception> flush(Collection sms) {
        Collection<OpenJPAStateManager> stateManagers =
                (Collection<OpenJPAStateManager>)sms;
        StringBuffer buffer = null;
        if (logger.isTraceEnabled()) {
            buffer = new StringBuffer();
        } 
        // make sure all instances are OK to insert/update/delete
        boolean allSupportedTypes = true;
        for (OpenJPAStateManager sm: stateManagers) {
            DomainTypeHandler<?> domainTypeHandler = getDomainTypeHandler(sm);
            if (!domainTypeHandler.isSupportedType()) {
                allSupportedTypes = false;
            }
            if (logger.isTraceEnabled()) {
                buffer.append(printState(sm));
            }
        }
        if (logger.isTraceEnabled()) {
                logger.trace(buffer.toString());
        }
        if (!allSupportedTypes) {
            return super.flush(sms);
        }
        // now flush changes to the cluster back end
        getSession();
        Collection exceptions = new ArrayList<Exception>();
        for (OpenJPAStateManager sm:stateManagers) {
            // get DomainTypeHandler from StateManager
            NdbOpenJPADomainTypeHandlerImpl<?> domainTypeHandler = getDomainTypeHandler(sm);
            // get the value handler for the StateManager
            ValueHandler valueHandler = domainTypeHandler.getValueHandler(sm, this);
            // now flush based on current PCState
            PCState pcState = sm.getPCState();
            try {
                if (pcState == PCState.PNEW) {
                    // flush new instance
                    session.insert(domainTypeHandler, valueHandler);
                } else if (pcState == PCState.PDELETED) {
                    // flush deleted instance
                    session.delete(domainTypeHandler, valueHandler);
                } else if (pcState == PCState.PDIRTY) {
                    // flush dirty instance
                    session.update(domainTypeHandler, valueHandler);
                } else {
                    throw new ClusterJUserException(
                            local.message("ERR_Unsupported_Flush_Operation",
                            pcState.toString()));
                }
            } catch (Exception ex) {
                if (logger.isDebugEnabled()) {
                    logger.debug("Exception caught: " + ex.toString());
                }
                exceptions.add(ex);
            }
        }
        // after all instances are flushed, send to the back end
        session.flush();

        return exceptions;
    }

    @Override
    public void beforeStateChange(OpenJPAStateManager sm, PCState fromState,
            PCState toState) {
        if (logger.isDetailEnabled()) {
                logger.detail(
                printState("from ", fromState) +
                printState(" to ", toState));
        }
        super.beforeStateChange(sm, fromState, toState);
    }

    @Override
    public StoreQuery newQuery(String language) {
        ExpressionParser ep = QueryLanguages.parserForLanguage(language);
        return new NdbOpenJPAStoreQuery(this, ep);
    }

    @Override
    public void beginOptimistic() {
        if (logger.isTraceEnabled()) {
            logger.trace(" Transaction " + hashCode() + printIsActive(tx));
        }
//        super.beginOptimistic();
        try {
            getSession();
            tx = session.currentTransaction();
            if (tx.isActive()) {
                tx.commit();
            }
            tx.begin();
        } catch (Exception e) {
            logger.detail("NdbOpenJPAStoreManager.beginOptimistic():" +
                    "caught exception in session.currentTransaction.begin().");
            throw new ClusterJDatastoreException(
                    local.message("ERR_Datastore_Exception"), e);
        }
    }

    @Override
    public void begin() {
        if (logger.isTraceEnabled()) {logger.trace(" Transaction " + hashCode() + printIsActive(tx));}
        getSession();
        try {
            // end ndb transaction if active
            tx = session.currentTransaction();
            if (tx.isActive()) {
                tx.commit();
            }
            tx.begin();
        } catch (Exception e) {
            logger.detail("Caught exception in session.currentTransaction.commit()." +
                    e.getMessage());
        }
        // TODO: handle JDBC connection for queries
//        super.begin();
    }

    @Override
    public void commit() {
        if (logger.isTraceEnabled()) {logger.trace(" Transaction " + hashCode() + printIsActive(tx));}
        try {
            session.commit();
        } catch (Exception ex) {
            logger.detail(" failed" + ex.toString());
            throw new ClusterJException(
                    local.message("ERR_Commit_Failed", ex.toString()));
        }
        // TODO: handle JDBC connection for queries
//        super.commit();
    }

    @Override
    public void rollback() {
        if (logger.isTraceEnabled()) {logger.trace(" Transaction " + hashCode() + printIsActive(tx));}
        session.rollback();
        // TODO: handle JDBC connection for queries
//        super.rollback();
    }

    @Override
    public void close() {
        if (logger.isTraceEnabled()) {logger.trace(" Transaction " + hashCode() + printIsActive(tx));}
        if (session != null && !session.isClosed()) {
            if (session.currentTransaction().isActive()) {
                tx.commit();
            }
            session.close();
        }
    }

    protected String printState(OpenJPAStateManager sm) {
        StringBuffer buffer = new StringBuffer();
        buffer.append("class: ");
        buffer.append(sm.getPersistenceCapable().getClass().getName());
        buffer.append(" objectId: ");
        buffer.append(sm.getObjectId());
        buffer.append(" PCState: ");
        buffer.append(sm.getPCState());
        buffer.append("\n");
        return buffer.toString();
    }

    protected String printState(String header, PCState state) {
        StringBuffer buffer = new StringBuffer(header);
        buffer.append(state.getClass().getSimpleName());
        return buffer.toString();
    }

    protected String printLoaded(OpenJPAStateManager sm) {
        BitSet loaded = sm.getLoaded();
        return "Loaded: " + printBitSet(sm, loaded);
    }

    protected String printBitSet(OpenJPAStateManager sm, BitSet fields) {
        ClassMetaData classMetaData = sm.getMetaData();
        FieldMetaData[] fieldMetaDatas = classMetaData.getDefinedFields();
        StringBuffer buffer = new StringBuffer("[");
        if (fields != null) {
            String separator = "";
            for (int i = 0; i < fields.size(); ++i) {
                if (fields.get(i)) {
                    buffer.append(separator);
                    buffer.append(i);
                    buffer.append(" ");
                    buffer.append(fieldMetaDatas[i].getName());
                    separator = ";";
                }
            }
        }
        buffer.append("] ");
        return buffer.toString();
    }

    protected String printIsActive(Transaction tx) {
        return (tx==null?" is null.":tx.isActive()?" is active.":" is not active.");
    }
// The following is not used in ClusterJ, since managed mode is not implemented
//        LockManager lm = ctx.getLockManager();
//        if (lm instanceof JDBCLockManager)
//            _lm = (JDBCLockManager) lm;
//
//        if (!ctx.isManaged() && _conf.isConnectionFactoryModeManaged())
//            _ds = _conf.getDataSource2(ctx);
//        else
//            _ds = _conf.getDataSource(ctx);
//
//        if (_conf.getUpdateManagerInstance().orderDirty())
//            ctx.setOrderDirtyObjects(true);

    /** Create a query.
     * @param type the root type of the query
     * @return the query domain type
     */
    public <T> QueryDomainType<T> createQueryDomainType(Class<T> type) {
	return session.getQueryBuilder().createQueryDefinition(type);
    }

    /** Execute the query and return the result list. 
     * @param domainTypeHandler the domain type handler
     * @param queryDomainType the QueryDomainType
     * @param parameterMap the bound parameters
     * @return the result of the query
     */
    public NdbOpenJPAResult executeQuery(DomainTypeHandler<?> domainTypeHandler, QueryDomainType<?> queryDomainType,
            Map<String, Object> parameterMap) {
        QueryExecutionContextImpl context = new QueryExecutionContextImpl(session, parameterMap);
        ResultData resultData = context.getResultData(queryDomainType);
        NdbOpenJPAResult result = new NdbOpenJPAResult(resultData, domainTypeHandler, null);
        return result;
    }

    public Dictionary getDictionary() {
        return dictionary;
    }

}
