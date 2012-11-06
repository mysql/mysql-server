/*
   Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj.core.store;

import com.mysql.clusterj.ColumnType;

/** Column metadata for ndb columns.
 *
 */
public interface Column {

    /** Get the name of the column.
     * @return the name
     */
    public String getName();

    /** Get the store type of the column.
     * 
     * @return the store type
     */
    public ColumnType getType();

    /** Is this column a primary key column?
     * @return true if this column is a primary key column
     */
    public boolean isPrimaryKey();

    /** Is this column a partition key column?
     * @return true if this column is a partition key column
     */
    public boolean isPartitionKey();

    /** Get the Charset name of the column. This is the value of the 
     * CHARACTER SET parameter in the column definition, or if not specified,
     * the value of the DEFAULT CHARACTER SET parameter in the table definition.
     * 
     * @return the Charset name
     */
    public String getCharsetName();

    /** The charset number.
     * 
     * @return the Charset number
     */
    public int getCharsetNumber();

    /** For character columns, get the maximum length in bytes that can be stored
     * in the column, excluding the prefix, after conversion via the charset.
     */
    public int getLength();

    /** For variable size columns, get the length of the prefix (one or two bytes) 
     * that specifies the length of the column data.
     * @return the prefix length, either one or two bytes for variable sized columns, zero otherwise
     */
    public int getPrefixLength();

    /** The size of a column in bytes. Integral types return the "precision" in bytes of the type
     * (1, 2, 4, or 8 bytes). Character and byte types return 1. See getLength() for character and byte types.
     * @return the size
     */
    public int getSize();

    /** The column ID, used for scans to compare column values.
     * 
     * @return the column id
     */
    public int getColumnId();

    /** The space needed to retrieve the value into memory, 
     * including the length byte or bytes at the beginning of the field.
     * 
     * @return the space required by the column
     */
    public int getColumnSpace();

    /** The precision of a decimal column (number of decimal digits before plus after
     * the decimal point).
     * 
     * @return the precision
     */
    public int getPrecision();

    /** The scale of a decimal column (the number of decimal digits after the decimal point).
     * 
     * @return the scale
     */
    public int getScale();

    /** Decode a byte[] into a String using this column's charset.
     * 
     * @param bytes the byte[] from the database
     * @return the decoded String
     */
    public String decode(byte[] bytes);

    /** Encode a String into a byte[] for storage or comparison.
     * 
     * @param string the String
     * @return the encoded byte[]
     */
    public byte[] encode(String string);

    /** Is this column nullable?
     * 
     * @return true if the column is nullable
     */
    public boolean getNullable();

    /** Is this column a lob?
     * 
     * @return true if this column is a blob or text type
     */
    public boolean isLob();

}
