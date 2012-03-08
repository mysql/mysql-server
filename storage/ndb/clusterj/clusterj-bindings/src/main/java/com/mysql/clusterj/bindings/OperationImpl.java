/*
 *  Copyright 2010 Sun Microsystems, Inc.
 *  All rights reserved. Use is subject to license terms.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package com.mysql.clusterj.bindings;

import com.mysql.cluster.ndbj.NdbApiException;
import com.mysql.cluster.ndbj.NdbOperation;

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.core.store.Blob;
import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.ResultData;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import java.math.BigDecimal;

import java.sql.Date;
import java.sql.Time;
import java.sql.Timestamp;

/**
 *
 */
class OperationImpl implements Operation {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(OperationImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(OperationImpl.class);

    private NdbOperation ndbOperation;

    private ClusterTransactionImpl clusterTransaction;

    public OperationImpl(NdbOperation operation, ClusterTransactionImpl transaction) {
        this.ndbOperation = operation;
        this.clusterTransaction = transaction;
    }

    public void equalBoolean(Column storeColumn, boolean booleanValue) {
        throw new UnsupportedOperationException(local.message("ERR_NotImplemented"));
    }

    public void equalByte(Column storeColumn, byte b) {
        try {
            ndbOperation.equalInt(storeColumn.getName(), (int)b);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void equalBytes(Column storeColumn, byte[] value) {
        try {
            ndbOperation.equalBytes(storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void equalDecimal(Column storeColumn, BigDecimal value) {
        try {
            ndbOperation.equalDecimal(storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void equalInt(Column storeColumn, int value) {
        try {
            ndbOperation.equalInt(storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void equalLong(Column storeColumn, long value) {
        try {
            ndbOperation.equalLong(storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void equalString(Column storeColumn, String value) {
        try {
            ndbOperation.equalString(storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void equalTimestamp(Column storeColumn, Timestamp value) {
        try {
            ndbOperation.equalTimestamp(storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void equalDatetime(Column storeColumn, Timestamp value) {
        try {
            ndbOperation.equalDatetime(storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void equalDate(Column storeColumn, Date value) {
        try {
            Timestamp timestamp = new Timestamp(((Date)value).getTime());
            ndbOperation.equalDatetime(storeColumn.getName(), timestamp);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void equalTime(Column storeColumn, Time value) {
        try {
            Timestamp timestamp = new Timestamp(((Time)value).getTime());
            ndbOperation.equalDatetime(storeColumn.getName(), timestamp);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void getBlob(Column storeColumn) {
        try {
            ndbOperation.getBlob(storeColumn.getName());
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public Blob getBlobHandle(Column storeColumn) {
        try {
            return new BlobImpl(ndbOperation.getBlobHandle(storeColumn.getName()));
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void getValue(Column storeColumn) {
        try {
            ndbOperation.getValue(storeColumn.getName());
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void postExecuteCallback(Runnable callback) {
        clusterTransaction.postExecuteCallback(callback);
    }

    public ResultData resultData() {
        // execute the transaction to get results
        clusterTransaction.executeNoCommit();
        return new ResultDataImpl(ndbOperation.resultData());
    }

    public void setBoolean(Column storeColumn, Boolean value) {
        throw new UnsupportedOperationException(local.message("ERR_NotImplemented"));
    }

    public void setByte(Column storeColumn, byte value) {
        try {
            ndbOperation.setInt(storeColumn.getName(), (int)value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void setBytes(Column storeColumn, byte[] value) {
        try {
            ndbOperation.setBytes(storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void setDate(Column storeColumn, Date value) {
        try {
            ndbOperation.setDate(storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void setDecimal(Column storeColumn, BigDecimal value) {
        try {
            ndbOperation.setDecimal(storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void setDouble(Column storeColumn, Double value) {
        try {
            ndbOperation.setDouble(storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void setFloat(Column storeColumn, Float value) {
        try {
            ndbOperation.setFloat(storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void setInt(Column storeColumn, Integer value) {
        try {
            ndbOperation.setInt(storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void setLong(Column storeColumn, long value) {
        try {
            ndbOperation.setLong(storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void setNull(Column storeColumn) {
        try {
            ndbOperation.setNull(storeColumn.getName());
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void setShort(Column storeColumn, Short value) {
        try {
            ndbOperation.setShort(storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void setString(Column storeColumn, String value) {
        try {
            ndbOperation.setString(storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void setTime(Column storeColumn, Time value) {
        try {
            ndbOperation.setTime(storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void setTimestamp(Column storeColumn, Timestamp value) {
        try {
            ndbOperation.setTimestamp(storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void setDatetime(Column storeColumn, Timestamp value) {
        try {
            ndbOperation.setDatetime(storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

}
