/*
 *  Copyright (c) 2010, 2024, Oracle and/or its affiliates.
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

import java.math.BigDecimal;
import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.util.Arrays;

import com.mysql.ndbjtie.ndbapi.NdbScanFilter;

import com.mysql.clusterj.ClusterJFatalInternalException;

import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import com.mysql.clusterj.tie.DbImpl;
import com.mysql.clusterj.tie.DbImpl.BufferManager;

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

    private NdbScanFilter ndbScanFilter;
    private DbImpl.BufferManager bufferManager;

    public ScanFilterImpl(NdbScanFilter ndbScanFilter, DbImpl db) {
        this.ndbScanFilter = ndbScanFilter;
        this.bufferManager = db.getBufferManager();
    }

    public ScanFilterImpl(NdbScanFilter ndbScanFilter, BufferManager bufferManager) {
        this.ndbScanFilter = ndbScanFilter;
        this.bufferManager = bufferManager;
    }

    public void begin() {
        int returnCode = ndbScanFilter.begin(NdbScanFilter.Group.AND);
        handleError(returnCode, ndbScanFilter);
    }

    public void begin(Group group) {
        int returnCode = ndbScanFilter.begin(convertGroup(group));
        handleError(returnCode, ndbScanFilter);
    }

    public void cmpBigInteger(BinaryCondition condition, Column storeColumn, BigInteger value) {
        ByteBuffer buffer = bufferManager.borrowBuffer(100);
        Utility.convertValue(buffer, storeColumn, value);
        int returnCode = ndbScanFilter.cmp(convertCondition(condition),
                storeColumn.getColumnId(), buffer, buffer.limit());
        bufferManager.returnBuffer(100, buffer);
        handleError(returnCode, ndbScanFilter);
    }

    public void cmpBoolean(BinaryCondition condition, Column storeColumn, boolean value) {
        byte byteValue = (value?(byte)0x01:(byte)0x00);
        cmpByte(condition, storeColumn, byteValue);
    }

    public void cmpByte(BinaryCondition condition, Column storeColumn, byte value) {
        // Bit types can use Byte and need 4 bytes for storage
        ByteBuffer buffer = bufferManager.borrowBuffer(4);
        Utility.convertValue(buffer, storeColumn, value);
        int returnCode = ndbScanFilter.cmp(convertCondition(condition),
                storeColumn.getColumnId(), buffer, buffer.limit());
        bufferManager.returnBuffer(4, buffer);
        handleError(returnCode, ndbScanFilter);
    }

    public void cmpBytes(BinaryCondition condition, Column storeColumn, byte[] value) {
        int columnSpace = storeColumn.getColumnSpace();
        ByteBuffer buffer = bufferManager.borrowBuffer(columnSpace);
        if (condition == BinaryCondition.COND_LIKE) {
            Utility.convertValueForLikeFilter(buffer, storeColumn, value);
        } else {
            Utility.convertValue(buffer, storeColumn, value);
        }
        int returnCode = ndbScanFilter.cmp(convertCondition(condition),
                storeColumn.getColumnId(), buffer, buffer.limit());
        bufferManager.returnBuffer(columnSpace, buffer);
        handleError(returnCode, ndbScanFilter);
    }

    public void cmpDecimal(BinaryCondition condition, Column storeColumn, BigDecimal value) {
        ByteBuffer buffer = bufferManager.borrowBuffer(100);
        Utility.convertValue(buffer, storeColumn, value);
        int returnCode = ndbScanFilter.cmp(convertCondition(condition),
                storeColumn.getColumnId(), buffer, buffer.limit());
        bufferManager.returnBuffer(100, buffer);
        handleError(returnCode, ndbScanFilter);
    }

    public void cmpDouble(BinaryCondition condition, Column storeColumn, double value) {
        ByteBuffer buffer = bufferManager.borrowBuffer(8);
        Utility.convertValue(buffer, storeColumn, value);
        int returnCode = ndbScanFilter.cmp(convertCondition(condition),
                storeColumn.getColumnId(), buffer, buffer.limit());
        bufferManager.returnBuffer(8, buffer);
        handleError(returnCode, ndbScanFilter);
    }

    public void cmpFloat(BinaryCondition condition, Column storeColumn, float value) {
        ByteBuffer buffer = bufferManager.borrowBuffer(4);
        Utility.convertValue(buffer, storeColumn, value);
        int returnCode = ndbScanFilter.cmp(convertCondition(condition),
                storeColumn.getColumnId(), buffer, buffer.limit());
        bufferManager.returnBuffer(4, buffer);
        handleError(returnCode, ndbScanFilter);
    }

    public void cmpShort(BinaryCondition condition, Column storeColumn, short value) {
        // Bit types can use Short and need 4 bytes for storage
        ByteBuffer buffer = bufferManager.borrowBuffer(4);
        Utility.convertValue(buffer, storeColumn, value);
        int returnCode = ndbScanFilter.cmp(convertCondition(condition),
                storeColumn.getColumnId(), buffer, buffer.limit());
        bufferManager.returnBuffer(4, buffer);
        handleError(returnCode, ndbScanFilter);
    }

    public void cmpInt(BinaryCondition condition, Column storeColumn, int value) {
        ByteBuffer buffer = bufferManager.borrowBuffer(4);
        Utility.convertValue(buffer, storeColumn, value);
        int returnCode = ndbScanFilter.cmp(convertCondition(condition),
                storeColumn.getColumnId(), buffer, buffer.limit());
        bufferManager.returnBuffer(4, buffer);
        handleError(returnCode, ndbScanFilter);
    }

    public void cmpLong(BinaryCondition condition, Column storeColumn, long value) {
        ByteBuffer buffer = bufferManager.borrowBuffer(8);
        Utility.convertValue(buffer, storeColumn, value);
        if (logger.isDetailEnabled()) {
            int bufferLength = buffer.limit() - buffer.position();
            byte[] array = new byte[bufferLength];
            buffer.get(array);
            buffer.flip();
            if (logger.isDetailEnabled())
                logger.detail("column: " + storeColumn.getName() + " condition: " + condition.toString() +
                    " value: " + value + Arrays.toString(array) + "(" + buffer.capacity() + ")");
        }
        int returnCode = ndbScanFilter.cmp(convertCondition(condition),
                storeColumn.getColumnId(), buffer, buffer.limit());
        handleError(returnCode, ndbScanFilter);
    }

    public void cmpString(BinaryCondition condition, Column storeColumn, String value) {
        if (logger.isDebugEnabled())
            logger.debug(storeColumn.getName() + " " + condition + " " + value);
        ByteBuffer buffer;
        if (condition == BinaryCondition.COND_LIKE) {
            buffer = Utility.convertValueForLikeFilter(storeColumn, value);
        } else {
            buffer = Utility.convertValue(storeColumn, value);
        }
        int returnCode = ndbScanFilter.cmp(convertCondition(condition),
                storeColumn.getColumnId(), buffer, buffer.limit());
        handleError(returnCode, ndbScanFilter);
    }

    public void isNull(Column storeColumn) {
        int returnCode = ndbScanFilter.isnull(storeColumn.getColumnId());
        handleError(returnCode, ndbScanFilter);
    }

    public void isNotNull(Column storeColumn) {
        int returnCode = ndbScanFilter.isnotnull(storeColumn.getColumnId());
        handleError(returnCode, ndbScanFilter);
    }

    public void end() {
        int returnCode = ndbScanFilter.end();
        handleError(returnCode, ndbScanFilter);
    }

    private int convertCondition(BinaryCondition condition) {
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
            case COND_LIKE:
                return NdbScanFilter.BinaryCondition.COND_LIKE;
            default:
                throw new ClusterJFatalInternalException(
                        local.message("ERR_Implementation_Should_Not_Occur"));
        }
    }

    private int convertGroup(Group group) {
        switch(group) {
            case GROUP_AND:
                return NdbScanFilter.Group.AND;
            case GROUP_NAND:
                return NdbScanFilter.Group.NAND;
            case GROUP_OR:
                return NdbScanFilter.Group.OR;
            default:
                throw new ClusterJFatalInternalException(
                        local.message("ERR_Implementation_Should_Not_Occur"));
        }
    }

    protected static void handleError(int returnCode, NdbScanFilter ndbScanFilter) {
        if (returnCode == 0) {
            return;
        } else {
            Utility.throwError(returnCode, ndbScanFilter.getNdbError());
        }
    }

    protected static void handleError(Object object, NdbScanFilter ndbScanFilter) {
        if (object != null) {
            return;
        } else {
            Utility.throwError(null, ndbScanFilter.getNdbError());
        }
    }

    public void delete() {
        NdbScanFilter.delete(ndbScanFilter);
    }

}
