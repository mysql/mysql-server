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
import com.mysql.cluster.ndbj.NdbScanFilter;

import com.mysql.clusterj.ClusterJDatastoreException;

import com.mysql.clusterj.ClusterJFatalInternalException;
import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.ScanFilter;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import java.sql.Date;
import java.sql.Time;
import java.sql.Timestamp;

/**
 *
 */
class ScanFilterImpl implements ScanFilter {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(ScanFilterImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(ScanFilterImpl.class);

    private NdbScanFilter scanFilter;

    public ScanFilterImpl(NdbScanFilter scanFilter) {
        this.scanFilter = scanFilter;
    }

    public void begin() {
        try {
            scanFilter.begin();
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void begin(Group group) {
        try {
            scanFilter.begin(convertGroup(group));
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void cmpBoolean(BinaryCondition condition, Column storeColumn, boolean booleanValue) {
        throw new UnsupportedOperationException(local.message("ERR_NotImplemented"));
    }

    public void cmpBytes(BinaryCondition condition, Column storeColumn, byte[] value) {
        try {
            scanFilter.cmp(convertCondition(condition), storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void cmpDouble(BinaryCondition condition, Column storeColumn, double value) {
        try {
            scanFilter.cmp(convertCondition(condition), storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void cmpFloat(BinaryCondition condition, Column storeColumn, float value) {
        try {
            scanFilter.cmp(convertCondition(condition), storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void cmpInt(BinaryCondition condition, Column storeColumn, int value) {
        try {
            scanFilter.cmp(convertCondition(condition), storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void cmpLong(BinaryCondition condition, Column storeColumn, long value) {
        try {
            scanFilter.cmp(convertCondition(condition), storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void cmpString(BinaryCondition condition, Column storeColumn, String value) {
        try {
            scanFilter.cmpString(convertCondition(condition), storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void cmpTimestamp(BinaryCondition condition, Column storeColumn, Timestamp value) {
        try {
            scanFilter.cmpTimestamp(convertCondition(condition), storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void cmpDatetime(BinaryCondition condition, Column storeColumn, Timestamp value) {
        try {
            scanFilter.cmp(convertCondition(condition), storeColumn.getName(), value);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void cmpDate(BinaryCondition condition, Column storeColumn, Date value) {
        try {
            Timestamp timestamp = new Timestamp(value.getTime());
            scanFilter.cmp(convertCondition(condition), storeColumn.getName(), timestamp);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void cmpTime(BinaryCondition condition, Column storeColumn, Time value) {
        try {
            Timestamp timestamp = new Timestamp(value.getTime());
            scanFilter.cmp(convertCondition(condition), storeColumn.getName(), timestamp);
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    public void end() {
        try {
            scanFilter.end();
        } catch (NdbApiException ndbApiException) {
            throw new ClusterJDatastoreException(local.message("ERR_Datastore"),
                    ndbApiException);
        }
    }

    private NdbScanFilter.BinaryCondition convertCondition(BinaryCondition condition) {
        switch (condition) {
            case COND_EQ:
                return NdbScanFilter.BinaryCondition.COND_EQ;
            case COND_LE:
                return NdbScanFilter.BinaryCondition.COND_LE;
            case COND_LT:
                return NdbScanFilter.BinaryCondition.COND_LT;
            case COND_GE:
                return NdbScanFilter.BinaryCondition.COND_GE;
            case COND_GT:
                return NdbScanFilter.BinaryCondition.COND_GT;
            default:
                throw new ClusterJFatalInternalException(
                        local.message("ERR_Implementation_Should_Not_Occur"));
        }
    }

    private NdbScanFilter.Group convertGroup(Group group) {
        switch(group) {
            case GROUP_AND:
                return NdbScanFilter.Group.AND;
            default:
                throw new ClusterJFatalInternalException(
                        local.message("ERR_Implementation_Should_Not_Occur"));
        }
    }

}
