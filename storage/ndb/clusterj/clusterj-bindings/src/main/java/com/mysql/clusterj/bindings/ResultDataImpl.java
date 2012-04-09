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
import com.mysql.cluster.ndbj.NdbResultSet;
import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.core.store.Blob;
import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import java.math.BigDecimal;
import java.sql.Date;
import java.sql.SQLException;
import java.sql.Time;
import java.sql.Timestamp;

/**
 *
 */
class ResultDataImpl implements ResultData {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(ClusterTransactionImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(ClusterTransactionImpl.class);

    private NdbResultSet resultData;

    public ResultDataImpl(NdbResultSet resultData) {
        this.resultData = resultData;
    }

    public Blob getBlob(Column storeColumn) {
        try {
            return new BlobImpl(resultData.getBlob(storeColumn.getName()));
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public Date getDate(Column storeColumn) {
        try {
            Date result = resultData.getDate(storeColumn.getName());
            if ((result != null) && wasNull(storeColumn.getName())) {
        	logger.info("Column was null but non-null was returned.");
        	return null;
            }
            return result;
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        } catch (SQLException sqlException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    sqlException);
        }
    }

    public BigDecimal getDecimal(Column storeColumn) {
        try {
            BigDecimal result = resultData.getDecimal(storeColumn.getName());
            if ((result != null) && wasNull(storeColumn.getName())) {
        	logger.info("Column was null but non-null was returned.");
        	return null;
            }
            return result;
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public boolean getBoolean(Column storeColumn) {
        throw new UnsupportedOperationException(local.message("ERR_NotImplemented"));
    }

    public boolean[] getBooleans(Column storeColumn) {
        throw new UnsupportedOperationException(local.message("ERR_NotImplemented"));
    }

    public byte getByte(Column storeColumn) {
        // In the ndb-bindings there is no getByte API, so get the result as an int
        try {
            return (byte)resultData.getInt(storeColumn.getName());
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public double getDouble(Column storeColumn) {
        try {
            return resultData.getDouble(storeColumn.getName());
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public float getFloat(Column storeColumn) {
        try {
            return resultData.getFloat(storeColumn.getName());
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public int getInt(Column storeColumn) {
        try {
            return resultData.getInt(storeColumn.getName());
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public long getLong(Column storeColumn) {
        try {
            return resultData.getLong(storeColumn.getName());
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public short getShort(Column storeColumn) {
        try {
            return resultData.getShort(storeColumn.getName());
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public String getString(Column storeColumn) {
        try {
            String result = resultData.getString(storeColumn.getName());
            if (wasNull(storeColumn.getName())) {
                if (result != null) {
                    logger.info("Column " + storeColumn.getName() + " was null but non-null was returned.");
                }
                return null;
            }
            return result;
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public Time getTime(Column storeColumn) {
        try {
            Time result = resultData.getTime(storeColumn.getName());
            if ((result != null) && wasNull(storeColumn.getName())) {
        	logger.info("Column was null but non-null was returned.");
        	return null;
            }
            return result;
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        } catch (SQLException sqlException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    sqlException);
        }
    }

    public Timestamp getTimestamp(Column storeColumn) {
        try {
            Timestamp result = resultData.getTimestamp(storeColumn.getName());
            if ((result != null) && wasNull(storeColumn.getName())) {
        	logger.info("Column was null but non-null was returned.");
        	return null;
            }
            return result;
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public Timestamp getDatetime(Column storeColumn) {
        return getTimestamp(storeColumn);
    }

    public boolean next() {
        try {
            return resultData.next();
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public byte[] getBytes(Column storeColumn) {
        if (logger.isDetailEnabled()) logger.detail("Column name: " + storeColumn.getName());
        try {
            byte[] result = resultData.getBytes(storeColumn.getName());
            if ((result != null) && wasNull(storeColumn.getName())) {
                logger.info("Column was null but non-null was returned.");
                return null;
            }
            return result;
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public byte[] getStringBytes(Column storeColumn) {
        if (logger.isDetailEnabled()) logger.detail("Column name: " + storeColumn.getName());
        try {
            byte[] result = resultData.getStringBytes(storeColumn.getName());
            if ((result != null) && wasNull(storeColumn.getName())) {
                logger.info("Column was null but non-null was returned.");
                return null;
            }
            return result;
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }


    public Object getObject(Column storeColumn) {
        try {
            return resultData.getObject(storeColumn.getName());
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        } catch (SQLException sqlException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    sqlException);
        }
    }

    public boolean wasNull(String columnName) {
	try {
	    return resultData.wasNull();
	} catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
	}
    }

    public Boolean getObjectBoolean(Column storeColumn) {
        throw new UnsupportedOperationException(local.message("ERR_NotImplemented"));
    }

    public Byte getObjectByte(Column storeColumn) {
        Byte result = getByte(storeColumn);
        return (wasNull(storeColumn.getName())?null:result);
    }

    public Double getObjectDouble(Column storeColumn) {
        Double result = getDouble(storeColumn);
        return (wasNull(storeColumn.getName())?null:result);
    }

    public Float getObjectFloat(Column storeColumn) {
        Float result = getFloat(storeColumn);
        return (wasNull(storeColumn.getName())?null:result);
    }

    public Integer getObjectInteger(Column storeColumn) {
        Integer result = getInt(storeColumn);
        return (wasNull(storeColumn.getName())?null:result);
    }

    public Long getObjectLong(Column storeColumn) {
        Long result = getLong(storeColumn);
        return (wasNull(storeColumn.getName())?null:result);
    }

    public Short getObjectShort(Column storeColumn) {
        Short result = getShort(storeColumn);
        return (wasNull(storeColumn.getName())?null:result);
    }

}
