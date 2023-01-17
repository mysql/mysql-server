/*
   Copyright (c) 2012, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package com.mysql.clusterj.tie;

import java.lang.reflect.Method;

import java.math.BigDecimal;
import java.math.BigInteger;

import java.sql.Date;
import java.sql.Time;
import java.sql.Timestamp;

import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.ColumnMetadata;

import com.mysql.clusterj.core.CacheManager;

import com.mysql.clusterj.core.metadata.DomainTypeHandlerImpl;
import com.mysql.clusterj.core.metadata.InvocationHandlerImpl;

import com.mysql.clusterj.core.spi.DomainFieldHandler;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.SmartValueHandler;

import com.mysql.clusterj.core.store.ClusterTransaction;
import com.mysql.clusterj.core.store.Db;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.ResultData;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/** NdbRecordSmartValueHandlerImpl is the implementation class that
 * provides a value handler for a proxy or dynamic object that stores
 * values in an ndb record buffer instead of an object array. 
 * Subsequent database operations (insert, update, delete) use the
 * already-stored data instead of constructing a new data buffer. This class
 * replaces the InvocationHandler and uses the type-specific data transforms
 * done by DomainFieldHandler.
 * There are two separate numbering patterns used by this class: ValueHandler
 * methods access data using field numbers indexed according to the domain model.
 * Field numbers are translated to column numbers when calling NdbRecordOperationImpl.
 * Transient fields are implemented here, since NdbRecord knows nothing about
 * transient fields in the domain model.
 * The operation wrapper (NdbRecordOperationImpl) is initialized when the domain object
 * is created. But the operation to be performed (insert, delete, update, write) is only known
 * when the domain object is involved in a user operation. So the initialization of 
 * the corresponding NdbOperation is deferred.
 */
public class NdbRecordSmartValueHandlerImpl implements SmartValueHandler {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(InvocationHandlerImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(InvocationHandlerImpl.class);

    /** Finalize this object. This method is called by the garbage collector
     * when the proxy that delegates to this object is no longer reachable.
     */
    @SuppressWarnings("deprecation")
    protected void finalize() throws Throwable {
        if (logger.isDetailEnabled()) logger.detail("NdbRecordSmartValueHandler.finalize");
        try {
            release();
        } finally {
            super.finalize();
        }
    }

    /** Release any resources associated with this object.
     * This method is called by the owner of this object.
     */
    public void release() {
        if (logger.isDetailEnabled()) logger.detail("NdbRecordSmartValueHandler.release");
        if (wasReleased()) {
            return;
        }
        // NdbRecordOperationImpl holds references to key buffer and value buffer ByteBuffers
        operation.release();
        operation = null;
        domainTypeHandler = null;
        domainFieldHandlers = null;
        fieldNumberToColumnNumberMap = null;
        transientValues = null;
        proxy = null;
    }

    /** Was this value handler released? */
    public boolean wasReleased() {
        return operation == null;
    }

    /** Assert that this value handler was not released */
    void assertNotReleased() {
        if (wasReleased()) {
            throw new ClusterJUserException(local.message("ERR_Cannot_Access_Object_After_Release"));
        }
    }

    protected NdbRecordOperationImpl operation;

    protected DomainTypeHandlerImpl<?> domainTypeHandler;

    protected DomainFieldHandler[] domainFieldHandlers;

    private int[] fieldNumberToColumnNumberMap;

    private Boolean found = null;

    /** The number of transient fields (that do not have a column and so cannot be stored in the NdbRecord) */
    private int numberOfTransientFields;

    /** Values of transient fields; only initialized if there are transient fields */
    private Object[] transientValues = null;

    private boolean[] transientModified; 

    private Object proxy;

    public NdbRecordSmartValueHandlerImpl(DomainTypeHandlerImpl<?> domainTypeHandler) {
        this.domainTypeHandler = domainTypeHandler;
        this.domainFieldHandlers = domainTypeHandler.getFieldHandlers();
        fieldNumberToColumnNumberMap = domainTypeHandler.getFieldNumberToColumnNumberMap();
        numberOfTransientFields = domainTypeHandler.getNumberOfTransientFields();
        transientModified = new boolean[numberOfTransientFields];
        if (numberOfTransientFields != 0) {
            transientValues = this.domainTypeHandler.newTransientValues();
        }
    }

    public NdbRecordSmartValueHandlerImpl(DomainTypeHandlerImpl<?> domainTypeHandler, Db db) {
        this(domainTypeHandler);
        this.operation = ((DbImpl)db).newNdbRecordOperationImpl(domainTypeHandler.getStoreTable());
    }

    public NdbRecordSmartValueHandlerImpl(DomainTypeHandlerImpl<?> domainTypeHandler, Db db, ResultData resultData) {
        this(domainTypeHandler);
        this.operation = ((NdbRecordResultDataImpl)resultData).transformOperation();
    }

    public Operation insert(ClusterTransaction clusterTransaction) {
        if (logger.isDetailEnabled()) logger.detail("smart insert for type: " + domainTypeHandler.getName()
                + "\n" + operation.dumpValues());
        ClusterTransactionImpl clusterTransactionImpl = (ClusterTransactionImpl)clusterTransaction;
        operation.insert(clusterTransactionImpl);
        return operation;
    }

    public Operation delete(ClusterTransaction clusterTransaction) {
        if (logger.isDetailEnabled()) logger.detail("smart delete for type: " + domainTypeHandler.getName()
                + "\n" + operation.dumpValues());
        operation.delete((ClusterTransactionImpl)clusterTransaction);
        return operation;
    }

    public Operation update(ClusterTransaction clusterTransaction) {
        if (logger.isDetailEnabled()) logger.detail("smart update for type: " + domainTypeHandler.getName()
                + " record: " + operation.dumpValues());
        operation.update((ClusterTransactionImpl)clusterTransaction);
        return operation;
    }

    public Operation write(ClusterTransaction clusterTransaction) {
        if (logger.isDetailEnabled()) logger.detail("smart write for type: " + domainTypeHandler.getName()
                + " record: " + operation.dumpValues());
        operation.write((ClusterTransactionImpl)clusterTransaction);
        return operation;
    }

    public Operation load(ClusterTransaction clusterTransaction) {
        if (logger.isDetailEnabled()) logger.detail("smart load for type: " + domainTypeHandler.getName()
                + " record: " + operation.dumpValues());
        // only load mapped columns that are persistent
        for (int i = 0; i < domainFieldHandlers.length; ++i) {
            DomainFieldHandler domainFieldHandler = domainFieldHandlers[i];
            int columnId = fieldNumberToColumnNumberMap[i];
            if (domainFieldHandler.isPersistent()) {
                if (domainFieldHandler.isLob()) {
                    operation.getBlobHandle(columnId);
                } else {
                    operation.columnSet(columnId);
                }
            }
        }
        operation.load((ClusterTransactionImpl)clusterTransaction);
        final NdbRecordSmartValueHandlerImpl valueHandler = this;
        // defer execution of the key operation until the next find, flush, or query
        Runnable postExecuteOperation = new Runnable() {
            public void run() {
                if (operation.getErrorCode() == 0) {
                    // found row in database
                    valueHandler.found(Boolean.TRUE);
                    operation.loadBlobValues();
                    domainTypeHandler.objectResetModified(valueHandler);
                } else {
                    // mark instance as not found
                    valueHandler.found(Boolean.FALSE);
                }
            }
        };
        clusterTransaction.postExecuteCallback(postExecuteOperation);
        return operation;
    }

    public BigDecimal getBigDecimal(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            return operation.getDecimal(columnId);
        }
        return (BigDecimal)transientValues[-1 - columnId];
    }

    public BigInteger getBigInteger(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            return operation.getBigInteger(columnId);
        }
        return (BigInteger)transientValues[-1 - columnId];
    }

    public boolean getBoolean(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            return operation.getBoolean(columnId);
        }
        Boolean value = (Boolean)transientValues[-1 - columnId];
        return value == null?false:value;
    }

    public boolean[] getBooleans(int fieldNumber) {
        throw new ClusterJFatalInternalException(local.message("ERR_Method_Not_Implemented",
                "NdbRecordSmartValueHandler.getBooleans(int)"));
    }

    public byte getByte(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            return operation.getByte(columnId);
        }
        Byte value = (Byte)transientValues[-1 - columnId];
        return value == null?0:value;
    }

    public byte[] getBytes(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            return operation.getBytes(columnId);
        }
        return (byte[])transientValues[-1 - columnId];
    }

    public double getDouble(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            return operation.getDouble(columnId);
        }
        Double value = (Double)transientValues[-1 - columnId];
        return value == null?0.0D:value;
    }

    public float getFloat(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            return operation.getFloat(columnId);
        }
        Float value = (Float)transientValues[-1 - columnId];
        return value == null?0.0F:value;
    }

    public int getInt(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            int result = operation.getInt(columnId);
            return result;
        }
        Integer value = (Integer)transientValues[-1 - columnId];
        return (value == null)?0:value;
    }

    public Date getJavaSqlDate(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            Long millis = operation.getObjectLong(columnId);
            return millis == null? null:new java.sql.Date(millis);
        }
        return (java.sql.Date)transientValues[-1 - columnId];
    }

    public Time getJavaSqlTime(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            Long millis = operation.getObjectLong(columnId);
            return millis == null? null:new java.sql.Time(millis);
        }
        return (java.sql.Time)transientValues[-1 - columnId];
    }

    public Timestamp getJavaSqlTimestamp(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            Long millis = operation.getObjectLong(columnId);
            return millis == null? null:new java.sql.Timestamp(millis);
        }
        return (java.sql.Timestamp)transientValues[-1 - columnId];
    }

    public java.util.Date getJavaUtilDate(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            Long millis = operation.getObjectLong(columnId);
            return millis == null? null:new java.util.Date(millis);
        }
        return (java.util.Date)transientValues[-1 - columnId];
    }

    public byte[] getLobBytes(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        NdbRecordBlobImpl blob = operation.getBlobHandle(columnId);
        return blob.getBytesData();
    }

    public String getLobString(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        NdbRecordBlobImpl blob = operation.getBlobHandle(columnId);
        String result = blob.getStringData();
        return result;
    }

    public long getLong(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            return operation.getLong(columnId);
        }
         Long value = (Long)transientValues[-1 - columnId];
         return value == null?0L:value;
    }

    public Boolean getObjectBoolean(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            return operation.getObjectBoolean(columnId);
        }
        return (Boolean)transientValues[-1 - columnId];
    }

    public Byte getObjectByte(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            return operation.getObjectByte(columnId);
        }
        return (Byte)transientValues[-1 - columnId];
    }

    public Double getObjectDouble(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            return operation.getObjectDouble(columnId);
        }
        return (Double)transientValues[-1 - columnId];
    }

    public Float getObjectFloat(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            return operation.getObjectFloat(columnId);
        }
        return (Float)transientValues[-1 - columnId];
    }

    public Integer getObjectInt(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            return operation.getObjectInteger(columnId);
        }
        return (Integer)transientValues[-1 - columnId];
    }

    public Long getObjectLong(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            return operation.getObjectLong(columnId);
        }
        return (Long)transientValues[-1 - columnId];
    }

    public Short getObjectShort(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            return operation.getObjectShort(columnId);
        }
        return (Short)transientValues[-1 - columnId];
    }

    public short getShort(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            return operation.getShort(columnId);
        }
        Short value = (Short)transientValues[-1 - columnId];
        return value == null?0:value;
    }

    public String getString(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            return operation.getString(columnId);
        }
        return (String)transientValues[-1 - columnId];
    }

    public boolean isModified(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            return operation.isModified(columnId);
        }
        return transientModified[-1 - columnId];
    }

    public boolean isNull(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            return operation.isNull(columnId);
        }
        return transientValues[-1 - columnId] == null;
    }

    public void markModified(int fieldNumber) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            operation.markModified(columnId);
            return;
        }
        transientModified[-1 - columnId] = true;
    }

    public String pkToString(DomainTypeHandler<?> domainTypeHandler) {
        // TODO Auto-generated method stub
        return null;
    }

    public void resetModified() {
        operation.resetModified();
        transientModified = new boolean[numberOfTransientFields];
    }

    public void setBigDecimal(int fieldNumber, BigDecimal value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            operation.setDecimal(columnId, value);
        } else {
            transientValues[-1 - columnId] = value;
            transientModified[-1 - columnId] = true;
        }
    }

    public void setBigInteger(int fieldNumber, BigInteger bigIntegerExact) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            operation.setBigInteger(columnId, bigIntegerExact);
        } else {
            transientValues[-1 - columnId] = bigIntegerExact;
            transientModified[-1 - columnId] = true;
        }
    }

    public void setBoolean(int fieldNumber, boolean b) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            operation.setBoolean(columnId, b);
        } else {
            transientValues[-1 - columnId] = b;
            transientModified[-1 - columnId] = true;
        }
    }

    public void setBooleans(int fieldNumber, boolean[] b) {
        throw new ClusterJFatalInternalException(local.message("ERR_Method_Not_Implemented",
                "NdbRecordSmartValueHandler.setBooleans(int, boolean[])"));
    }

    public void setByte(int fieldNumber, byte value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            operation.setByte(columnId, value);
        } else {
            transientModified[-1 - columnId] = true;
            transientValues[-1 - columnId] = value;
        }
    }

    public void setBytes(int fieldNumber, byte[] value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            operation.setBytes(columnId, value);
        } else {
            transientValues[-1 - columnId] = value;
            transientModified[-1 - columnId] = true;
        }
    }

    public void setDouble(int fieldNumber, double value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            operation.setDouble(columnId, value);
        } else {
            transientValues[-1 - columnId] = value;
            transientModified[-1 - columnId] = true;
        }
    }

    public void setFloat(int fieldNumber, float value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            operation.setFloat(columnId, value);
        } else {
            transientValues[-1 - columnId] = value;
            transientModified[-1 - columnId] = true;
        }
    }

    public void setInt(int fieldNumber, int value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            operation.setInt(columnId, value);
        } else {
            transientValues[-1 - columnId] = value;
            transientModified[-1 - columnId] = true;
        }
    }

    public void setJavaSqlDate(int fieldNumber, Date value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (value == null) {
            operation.setNull(columnId);
        } else {
            setLong(fieldNumber, value.getTime());            
        }
    }

    public void setJavaSqlTime(int fieldNumber, Time value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (value == null) {
            operation.setNull(columnId);
        } else {
            setLong(fieldNumber, value.getTime());            
        }
    }

    public void setJavaSqlTimestamp(int fieldNumber, Timestamp value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (value == null) {
            operation.setNull(columnId);
        } else {
            setLong(fieldNumber, value.getTime());            
        }
    }

    public void setJavaUtilDate(int fieldNumber, java.util.Date value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (value == null) {
            operation.setNull(columnId);
        } else {
            setLong(fieldNumber, value.getTime());            
        }
    }

    public void setLobBytes(int fieldNumber, byte[] value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        NdbRecordBlobImpl blob = operation.getBlobHandle(columnId);
        operation.columnSet(columnId);
        blob.setData(value);
    }

    public void setLobString(int fieldNumber, String value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        NdbRecordBlobImpl blob = operation.getBlobHandle(columnId);
        operation.columnSet(columnId);
        blob.setData(value);
    }

    public void setLong(int fieldNumber, long value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            operation.setLong(columnId, value);
        } else {
            transientValues[-1 - columnId] = value;
            transientModified[-1 - columnId] = true;
        }
    }

    public void setObject(int fieldNumber, Object value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            throw new ClusterJFatalInternalException(local.message("ERR_Method_Not_Implemented",
            "NdbRecordSmartValueHandler.setObject(int, Object) for persistent values"));
        } else {
            transientValues[-1 - columnId] = value;
            transientModified[-1 - columnId] = true;
        }
    }

    public void setObjectBoolean(int fieldNumber, Boolean value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            operation.setObjectBoolean(columnId, value);
        } else {
            transientValues[-1 - columnId] = value;
            transientModified[-1 - columnId] = true;
        }
    }

    public void setObjectByte(int fieldNumber, Byte value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            operation.setObjectByte(columnId, value);
        } else {
            transientValues[-1 - columnId] = value;
            transientModified[-1 - columnId] = true;
        }
    }

    public void setObjectDouble(int fieldNumber, Double value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            operation.setObjectDouble(columnId, value);
        } else {
            transientValues[-1 - columnId] = value;
            transientModified[-1 - columnId] = true;
        }
    }

    public void setObjectFloat(int fieldNumber, Float value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            operation.setObjectFloat(columnId, value);
        } else {
            transientValues[-1 - columnId] = value;
            transientModified[-1 - columnId] = true;
        }
    }

    public void setObjectInt(int fieldNumber, Integer value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            operation.setObjectInt(columnId, value);
        } else {
            transientValues[-1 - columnId] = value;
            transientModified[-1 - columnId] = true;
        }
    }

    public void setObjectLong(int fieldNumber, Long value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            operation.setObjectLong(columnId, value);
        } else {
            transientValues[-1 - columnId] = value;
            transientModified[-1 - columnId] = true;
        }
    }

    public void setObjectShort(int fieldNumber, Short value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            operation.setObjectShort(columnId, value);
        } else {
            transientValues[-1 - columnId] = value;
            transientModified[-1 - columnId] = true;
        }
    }

    public void setShort(int fieldNumber, short value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            operation.setObjectShort(columnId, value);
        } else {
            transientValues[-1 - columnId] = value;
            transientModified[-1 - columnId] = true;
        }
    }

    public void setString(int fieldNumber, String value) {
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId >= 0) {
            operation.setString(columnId, value);
        } else {
            transientValues[-1 - columnId] = value;
            transientModified[-1 - columnId] = true;
        }
    }

    public ColumnMetadata[] columnMetadata() {
        return domainTypeHandler.columnMetadata();
    }

    public Boolean found() {
        return found;
    }

    public void found(Boolean found) {
        this.found = found;
    }

    /** Return the value of a dynamic field stored in the NdbRecord buffer.
     * @param fieldNumber the field number
     * @return the value from data storage
     */
    public Object get(int fieldNumber) {
        assertNotReleased();
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId < 0) {
            return transientValues[-1 - columnId];
        }
        return domainFieldHandlers[fieldNumber].objectGetValue(this);
    }

    public void set(int fieldNumber, Object value) {
        assertNotReleased();
        int columnId = fieldNumberToColumnNumberMap[fieldNumber];
        if (columnId < 0) {
            transientValues[-1 - columnId] = value;
            transientModified[-1 - columnId] = true;
        } else {
            domainFieldHandlers[fieldNumber].objectSetValue(value, this);
        }
    }

    public Object invoke(Object proxy, Method method, Object[] args)
            throws Throwable {
        String methodName = method.getName();
        if (logger.isDetailEnabled()) logger.detail("invoke with Method: " + method.getName());
        
        String propertyHead = methodName.substring(3,4);
        String propertyTail = methodName.substring(4);
        String propertyName = propertyHead.toLowerCase() + propertyTail;
        int fieldNumber;
        Object result;
        assertNotReleased();
        if (methodName.startsWith("get")) {
            // get the field number from the name
            fieldNumber = domainTypeHandler.getFieldNumber(propertyName);
            result = get(fieldNumber);
            if (logger.isDetailEnabled()) logger.detail(methodName + ": Returning field number " + fieldNumber
                + " value: " + result);
            return result;
        } else if (methodName.startsWith("set")) {
            if (logger.isDetailEnabled()) logger.detail("Property name: " + propertyName
                    + " value: " + args[0]);
            // get the field number from the name
            fieldNumber = domainTypeHandler.getFieldNumber(propertyName);
            set(fieldNumber, args[0]);
        } else if ("toString".equals(methodName)) {
            return(domainTypeHandler.getDomainClass().getName()
                    + pkToString(domainTypeHandler));
        } else if ("hashCode".equals(methodName)) {
            return(this.hashCode());
        } else {
            throw new ClusterJUserException(
                    local.message("ERR_Method_Name", methodName));
        }
        return null;
    }

    public void setCacheManager(CacheManager cm) {
    }

    public void setProxy(Object proxy) {
        this.proxy = proxy;
    }

    public Object getProxy() {
        return this.proxy;
    }

}
