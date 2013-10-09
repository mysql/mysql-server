/*
 *  Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.
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

package com.mysql.clusterj.jdbc;

import com.mysql.clusterj.core.metadata.AbstractDomainFieldHandlerImpl;
import com.mysql.clusterj.core.store.Table;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/** An instance of this class handles a column of a table. Most of the behavior
 * is in the superclass, which is common to all implementations. The constructor
 * determines which type is used for the Java representation of the database type.
 */
public class DomainFieldHandlerImpl extends AbstractDomainFieldHandlerImpl {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(DomainFieldHandlerImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(DomainFieldHandlerImpl.class);

    public int compareTo(Object other) {
        return compareTo((DomainFieldHandlerImpl)other);
    }

    public DomainFieldHandlerImpl(DomainTypeHandlerImpl<?> domainTypeHandler,
            Table table, int fieldNumber, com.mysql.clusterj.core.store.Column column) {
        this.domainTypeHandler = domainTypeHandler;
        this.fieldNumber = fieldNumber;
        this.name = column.getName();
        this.storeColumn = column;
        this.columnName = storeColumn.getName();
        this.columnNames = new String[] {columnName};
        storeColumnType = storeColumn.getType();
        charsetName = storeColumn.getCharsetName();
        if (logger.isDebugEnabled()) logger.debug("new DomainFieldHandlerImpl: fieldNumber: " + fieldNumber + "; name: " + name);
        if (logger.isDebugEnabled())
            logger.debug("Column type for " + name + " is "
                    + storeColumnType.toString() + "; charset name is "
                    + charsetName);
        primaryKey = storeColumn.isPrimaryKey();
        if (primaryKey) {
            domainTypeHandler.registerPrimaryKeyColumn(this, columnName);
        }
        if (primaryKey) {
            // primary key
            switch (column.getType()) {
            case Int:
            case Unsigned:
            case Mediumint:
            case Mediumunsigned:
                objectOperationHandlerDelegate = objectOperationHandlerKeyInt;
                break;
            case Bigint:
                objectOperationHandlerDelegate = objectOperationHandlerKeyLong;
                break;
            case Char:
            case Varchar:
            case Longvarchar:
                objectOperationHandlerDelegate = objectOperationHandlerKeyString;
                break;
            default:
                    // bad primary key type
                objectOperationHandlerDelegate = objectOperationHandlerUnsupportedType;
                error(
                        local.message("ERR_Unsupported_Field_Type", storeColumnType.name(), name));
            }
        } else if (storeColumn.getNullable()) {
            // nullable columns
            switch (column.getType()) {
            case Int:
            case Unsigned:
            case Mediumint:
            case Mediumunsigned:
                objectOperationHandlerDelegate = objectOperationHandlerObjectInteger;
                break;
            case Bigint:
                objectOperationHandlerDelegate = objectOperationHandlerObjectLong;
                break;
            case Char:
            case Varchar:
            case Longvarchar:
                objectOperationHandlerDelegate = objectOperationHandlerString;
                break;
            case Binary:
            case Varbinary:
            case Longvarbinary:
                objectOperationHandlerDelegate = objectOperationHandlerBytes;
                break;
            case Blob:
                objectOperationHandlerDelegate = objectOperationHandlerBytesLob;
                break;
            case Date:
                objectOperationHandlerDelegate = objectOperationHandlerJavaSqlDate;
                break;
            case Decimal:
            case Decimalunsigned:
                objectOperationHandlerDelegate = objectOperationHandlerDecimal;
                break;
            case Double:
                objectOperationHandlerDelegate = objectOperationHandlerObjectDouble;
                break;
            case Float:
                objectOperationHandlerDelegate = objectOperationHandlerObjectFloat;
                break;
            case Smallint:
                objectOperationHandlerDelegate = objectOperationHandlerObjectShort;
                break;
            case Text:
                objectOperationHandlerDelegate = objectOperationHandlerStringLob;
                break;
            case Time:
                objectOperationHandlerDelegate = objectOperationHandlerJavaSqlTime;
                break;
            case Timestamp:
                objectOperationHandlerDelegate = objectOperationHandlerJavaSqlTimestamp;
                break;
            case Tinyint:
            case Tinyunsigned:
                objectOperationHandlerDelegate = objectOperationHandlerObjectByte;
                break;
            default:
                // unsupported column type
                objectOperationHandlerDelegate = objectOperationHandlerUnsupportedType;
                error(
                        local.message("ERR_Unsupported_Field_Type", storeColumnType.name(), name));
            }
        } else {
            // not nullable columns
            switch (column.getType()) {
            case Int:
            case Unsigned:
            case Mediumint:
            case Mediumunsigned:
                objectOperationHandlerDelegate = objectOperationHandlerInt;
                break;
            case Bigint:
                objectOperationHandlerDelegate = objectOperationHandlerLong;
                break;
            case Char:
            case Varchar:
            case Longvarchar:
                objectOperationHandlerDelegate = objectOperationHandlerString;
                break;
            case Binary:
            case Varbinary:
            case Longvarbinary:
                objectOperationHandlerDelegate = objectOperationHandlerBytes;
                break;
            case Blob:
                objectOperationHandlerDelegate = objectOperationHandlerBytesLob;
                break;
            case Date:
                objectOperationHandlerDelegate = objectOperationHandlerJavaSqlDate;
                break;
            case Decimal:
            case Decimalunsigned:
                objectOperationHandlerDelegate = objectOperationHandlerDecimal;
                break;
            case Double:
                objectOperationHandlerDelegate = objectOperationHandlerDouble;
                break;
            case Float:
                objectOperationHandlerDelegate = objectOperationHandlerFloat;
                break;
            case Smallint:
                objectOperationHandlerDelegate = objectOperationHandlerShort;
                break;
            case Text:
                objectOperationHandlerDelegate = objectOperationHandlerStringLob;
                break;
            case Time:
                objectOperationHandlerDelegate = objectOperationHandlerJavaSqlTime;
                break;
            case Timestamp:
                objectOperationHandlerDelegate = objectOperationHandlerJavaSqlTimestamp;
                break;
            case Tinyint:
            case Tinyunsigned:
                objectOperationHandlerDelegate = objectOperationHandlerByte;
                break;
            default:
                // unsupported column type
                objectOperationHandlerDelegate = objectOperationHandlerUnsupportedType;
                error(
                        local.message("ERR_Unsupported_Field_Type", storeColumnType.name(), name));
            }
        }
        indices = domainTypeHandler.registerIndices(this, columnName);
        indexNames = domainTypeHandler.getIndexNames(indices);
        logger.debug("Index names for " + name + " are " + indexNames);
        logger.debug("Indices for " + name + " are " + printIndices());

        reportErrors();
    }

}
