/*
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

package com.mysql.clusterj.tie;

import com.mysql.ndbjtie.mysql.CharsetMap;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.ColumnConst;

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJFatalInternalException;

import com.mysql.clusterj.ColumnMetadata.Type;

import com.mysql.clusterj.core.store.Column;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/**
 *
 */
class ColumnImpl implements Column {

    /** My message translator */
    static final I18NHelper local = I18NHelper
            .getInstance(ColumnImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(ColumnImpl.class);

    /** The CharsetMap */
    static final CharsetMap charsetMap = Utility.getCharsetMap();

    /** The native charset name */
    private String nativeCharsetName;

    /** The charset name */
    private String charsetName;

    /** The charset number */
    private int charsetNumber = 0;

    /** The ndb column type for the column */
    private Type columnType;

    /** The prefix length for variable size columns */
    private int prefixLength = -1;

    /** The space required for storage of data including the prefix length */
    private int columnSpace = 0;

    /** The name of the column */
    private String columnName;

    /** The name of the table */
    private String tableName;

    /** The column id */
    private int columnId;

    /** Is this column a primary key column? */
    private boolean primaryKey;

    /** Is this column a partition key column? */
    private boolean partitionKey;

    private int length;

    private int inlineSize;

    private int precision;

    private int scale;

    private int size;

    private boolean nullable;

    public ColumnImpl(String tableName, ColumnConst ndbColumn) {
        this.columnName = ndbColumn.getName();
        this.columnId = ndbColumn.getColumnNo();
        this.tableName = tableName;
        int ndbType = ndbColumn.getType();
        this.columnType = convertType(ndbType);
        this.primaryKey = ndbColumn.getPrimaryKey();
        this.partitionKey = ndbColumn.getPartitionKey();
        this.nullable = ndbColumn.getNullable();
        this.length = ndbColumn.getLength();
        this.inlineSize = ndbColumn.getInlineSize();
        this.precision = ndbColumn.getPrecision();
        this.scale = ndbColumn.getScale();
        this.size = ndbColumn.getSize();
        switch(ndbColumn.getType()) {
            case ColumnConst.Type.Tinyint:
            case ColumnConst.Type.Tinyunsigned:
                this.prefixLength = 0;
                this.columnSpace = 0;
                break;
            case ColumnConst.Type.Smallint:
            case ColumnConst.Type.Smallunsigned:
                this.prefixLength = 0;
                this.columnSpace = 0;
                break;
            case ColumnConst.Type.Mediumint:
            case ColumnConst.Type.Mediumunsigned:
                this.prefixLength = 0;
                this.columnSpace = 0;
                break;
            case ColumnConst.Type.Int:
            case ColumnConst.Type.Unsigned:
                this.prefixLength = 0;
                this.columnSpace = 0;
                break;
            case ColumnConst.Type.Bigint:
            case ColumnConst.Type.Bigunsigned:
                this.prefixLength = 0;
                this.columnSpace = 0;
                break;
            case ColumnConst.Type.Float:
                this.prefixLength = 0;
                this.columnSpace = 0;
                break;
            case ColumnConst.Type.Double:
                this.prefixLength = 0;
                this.columnSpace = 0;
                break;
            case ColumnConst.Type.Olddecimal:
            case ColumnConst.Type.Olddecimalunsigned:
            case ColumnConst.Type.Decimal:
            case ColumnConst.Type.Decimalunsigned:
                this.prefixLength = 0;
                this.columnSpace = alignTo4(Utility.getDecimalColumnSpace(precision, scale));
                break;
            case ColumnConst.Type.Char:
                this.prefixLength = 0;
                this.columnSpace = length;
                this.charsetNumber = ndbColumn.getCharsetNumber();
                mapCharsetName();
                break;
            case ColumnConst.Type.Varchar:
                prefixLength = 1;
                this.columnSpace = alignTo4(length + prefixLength);
                this.charsetNumber = ndbColumn.getCharsetNumber();
                mapCharsetName();
                break;
            case ColumnConst.Type.Binary:
                this.prefixLength = 0;
                this.columnSpace = length;
                break;
            case ColumnConst.Type.Varbinary:
                this.prefixLength = 1;
                this.columnSpace = alignTo4(length + prefixLength);
                break;
            case ColumnConst.Type.Datetime:
                this.prefixLength = 0;
                this.columnSpace = 0;
                break;
            case ColumnConst.Type.Date:
                this.prefixLength = 0;
                this.columnSpace = 0;
                break;
            case ColumnConst.Type.Blob:
                this.prefixLength = 0;
                this.columnSpace = inlineSize;
                break;
            case ColumnConst.Type.Text:
                this.prefixLength = 0;
                this.columnSpace = inlineSize;
                this.charsetNumber = ndbColumn.getCharsetNumber();
                mapCharsetName();
                break;
            case ColumnConst.Type.Bit:
                this.prefixLength = 0;
                this.columnSpace = 0;
                break;
            case ColumnConst.Type.Longvarchar:
                this.prefixLength = 2;
                this.columnSpace = alignTo4(length + prefixLength);
                this.charsetNumber = ndbColumn.getCharsetNumber();
                mapCharsetName();
                break;
            case ColumnConst.Type.Longvarbinary:
                this.prefixLength = 2;
                this.columnSpace = alignTo4(length + prefixLength);
                this.charsetNumber = ndbColumn.getCharsetNumber();
                mapCharsetName();
                break;
            case ColumnConst.Type.Time:
                this.prefixLength = 0;
                this.columnSpace = 0;
                break;
            case ColumnConst.Type.Year:
                this.prefixLength = 0;
                this.columnSpace = 4;
                break;
            case ColumnConst.Type.Timestamp:
                this.prefixLength = 0;
                this.columnSpace = 4;
                break;
            default: throw new ClusterJFatalInternalException(
                    local.message("ERR_Unknown_Column_Type",
                    tableName, ndbColumn.getName(), ndbType));
        }
        if (logger.isDetailEnabled()) logger.detail("Column " + columnName
                + " columnSpace: " + columnSpace + " prefixLength: " + prefixLength
                + " inlineSize: " + inlineSize + " length: " + length + " size: " + size
                + " charsetNumber: " + charsetNumber + " charsetName: " + charsetName
                + " nativeCharsetNumber: " + nativeCharsetName);
    }

    private int alignTo4(int size) {
        int extra = 4 - ((size % 4) % 4);
        int result = size + extra;
        return result;
    }

    private void mapCharsetName() {
        this.nativeCharsetName = charsetMap.getName(charsetNumber);
        this.charsetName = charsetMap.getMysqlName(charsetNumber);
        if (charsetName == null) {
            throw new ClusterJDatastoreException(
                    local.message("ERR_Unknown_Charset_Name",
                    tableName, columnName, nativeCharsetName));
        }
    }

    public Type getType() {
        return columnType;
    }

    private Type convertType(int type) {
        switch (type) {
            case ColumnConst.Type.Bigint: return Type.Bigint;
            case ColumnConst.Type.Bigunsigned: return Type.Bigunsigned;
            case ColumnConst.Type.Binary: return Type.Binary;
            case ColumnConst.Type.Bit: return Type.Bit;
            case ColumnConst.Type.Blob: return Type.Blob;
            case ColumnConst.Type.Char: return Type.Char;
            case ColumnConst.Type.Date: return Type.Date;
            case ColumnConst.Type.Datetime: return Type.Datetime;
            case ColumnConst.Type.Decimal: return Type.Decimal;
            case ColumnConst.Type.Decimalunsigned: return Type.Decimalunsigned;
            case ColumnConst.Type.Double: return Type.Double;
            case ColumnConst.Type.Float: return Type.Float;
            case ColumnConst.Type.Int: return Type.Int;
            case ColumnConst.Type.Longvarbinary: return Type.Longvarbinary;
            case ColumnConst.Type.Longvarchar: return Type.Longvarchar;
            case ColumnConst.Type.Mediumint: return Type.Mediumint;
            case ColumnConst.Type.Mediumunsigned: return Type.Mediumunsigned;
            case ColumnConst.Type.Olddecimal: return Type.Olddecimal;
            case ColumnConst.Type.Olddecimalunsigned: return Type.Olddecimalunsigned;
            case ColumnConst.Type.Smallint: return Type.Smallint;
            case ColumnConst.Type.Smallunsigned: return Type.Smallunsigned;
            case ColumnConst.Type.Text: return Type.Text;
            case ColumnConst.Type.Time: return Type.Time;
            case ColumnConst.Type.Timestamp: return Type.Timestamp;
            case ColumnConst.Type.Tinyint: return Type.Tinyint;
            case ColumnConst.Type.Tinyunsigned: return Type.Tinyunsigned;
            case ColumnConst.Type.Undefined: return Type.Undefined;
            case ColumnConst.Type.Unsigned: return Type.Unsigned;
            case ColumnConst.Type.Varbinary: return Type.Varbinary;
            case ColumnConst.Type.Varchar: return Type.Varchar;
            case ColumnConst.Type.Year: return Type.Year;
            default: throw new ClusterJFatalInternalException(
                    local.message("ERR_Unknown_Column_Type",
                    tableName, columnName, type));
        }
    }

    public String getCharsetName() {
        return charsetName;
    }

    public String getName() {
        return columnName;
    }

    public boolean isPrimaryKey() {
        return primaryKey;
    }

    public boolean isPartitionKey() {
        return partitionKey;
    }

    public int getLength() {
        return length;
    }

    public int getPrefixLength() {
        if (prefixLength != -1) {
            return prefixLength;
        } else {
            throw new ClusterJFatalInternalException(local.message(
                    "ERR_Prefix_Length_Not_Defined", tableName, columnName));
        }
    }

    public int getColumnId() {
        return columnId;
    }

    public int getColumnSpace() {
        return columnSpace;
    }

    public int getPrecision() {
        return precision;
    }

    public int getScale() {
        return scale;
    }

    public int getCharsetNumber() {
        return charsetNumber;
    }

    public String decode(byte[] array) {
        return Utility.decode(array, charsetNumber);
    }

    public byte[] encode(String string) {
        return Utility.encode(string, charsetNumber);
    }

    @Override
    public String toString() {
        return columnName;
    }

    public boolean getNullable() {
        return nullable;
    }

}
