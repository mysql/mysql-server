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

import java.util.HashMap;
import java.util.Map;

import com.mysql.cluster.ndbj.NdbColumn;
import com.mysql.cluster.ndbj.NdbTable;

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJFatalInternalException;

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

    /** The charset map. This map converts between the database charset name
     * and the charset name used in Java Charset.
     */
    static private Map<String, String> charsetMap = new HashMap<String, String>();
    static {
        charsetMap.put("latin1", "windows-1252");
        charsetMap.put("utf8", "UTF-8");
        charsetMap.put("sjis", "SJIS");
        charsetMap.put("big5", "big5");
    }

    /** The ndb column metadata instance */
    private NdbColumn column;

    /** The native charset name */
    private String nativeCharsetName;

    /** The ISO charset name */
    private String charsetName;

    /** The ndb column type for the column */
    private Column.Type columnType;

    /** The table this column belongs to */
    private NdbTable table;

    private int prefixLength;

    private String tableName;

    private String columnName;

    public ColumnImpl(NdbTable table, NdbColumn column) {
        this.column = column;
        this.columnName = column.getName();
        this.table = table;
        this.tableName = table.getName();
        this.columnType = convertType(this.column.getType());
        switch(column.getType()) {
            case Char:
                prefixLength = 0;
                mapCharsetName();
                break;
            case Varchar:
                prefixLength = 1;
                mapCharsetName();
                break;
            case Longvarchar:
                prefixLength = 2;
                mapCharsetName();
                break;
            case Text:
                prefixLength = 0;
                mapCharsetName();
                break;
        }
    }

    private void mapCharsetName() {
        this.nativeCharsetName = column.getCharsetName();
        this.charsetName = charsetMap.get(nativeCharsetName);
        if (charsetName == null) {
            throw new ClusterJDatastoreException(
                    local.message("ERR_Unknown_Charset_Name",
                    tableName, columnName, nativeCharsetName));
        }
    }

    public String getName() {
        return columnName;
    }

    public Column.Type getType() {
        return columnType;
    }

    private Type convertType(com.mysql.cluster.ndbj.NdbColumn.Type type) {
        switch (type) {
            case Bigint: return Column.Type.Bigint;
            case Bigunsigned: return Column.Type.Bigunsigned;
            case Binary: return Column.Type.Binary;
            case Bit: return Column.Type.Bit;
            case Blob: return Column.Type.Blob;
            case Char: return Column.Type.Char;
            case Date: return Column.Type.Date;
            case Datetime: return Column.Type.Datetime;
            case Decimal: return Column.Type.Decimal;
            case Decimalunsigned: return Column.Type.Decimalunsigned;
            case Double: return Column.Type.Double;
            case Float: return Column.Type.Float;
            case Int: return Column.Type.Int;
            case Longvarbinary: return Column.Type.Longvarbinary;
            case Longvarchar: return Column.Type.Longvarchar;
            case Mediumint: return Column.Type.Mediumint;
            case Mediumunsigned: return Column.Type.Mediumunsigned;
            case Olddecimal: return Column.Type.Olddecimal;
            case Olddecimalunsigned: return Column.Type.Olddecimalunsigned;
            case Smallint: return Column.Type.Smallint;
            case Smallunsigned: return Column.Type.Smallunsigned;
            case Text: return Column.Type.Text;
            case Time: return Column.Type.Time;
            case Timestamp: return Column.Type.Timestamp;
            case Tinyint: return Column.Type.Tinyint;
            case Tinyunsigned: return Column.Type.Tinyunsigned;
            case Undefined: return Column.Type.Undefined;
            case Unsigned: return Column.Type.Unsigned;
            case Varbinary: return Column.Type.Varbinary;
            case Varchar: return Column.Type.Varchar;
            case Year: return Column.Type.Year;
            default: throw new ClusterJFatalInternalException(
                    local.message("ERR_Unknown_Column_Type",
                    table.getName(), column.getName(), type.toString()));
        }
    }

    public String getCharsetName() {
        return charsetName;
    }

    public int getPrefixLength() {
        if (prefixLength != 0) {
            return prefixLength;
        } else {
            throw new ClusterJFatalInternalException(local.message(
                    "ERR_Prefix_Length_Not_Defined", tableName, columnName));
        }
    }

    public int getColumnId() {
        throw new ClusterJFatalInternalException("Not implemented");
    }

    public int getColumnSpace() {
        throw new ClusterJFatalInternalException("Not implemented");
    }

    public int getPrecision() {
        throw new ClusterJFatalInternalException("Not implemented");
    }

    public int getScale() {
        throw new ClusterJFatalInternalException("Not implemented");
    }

}
