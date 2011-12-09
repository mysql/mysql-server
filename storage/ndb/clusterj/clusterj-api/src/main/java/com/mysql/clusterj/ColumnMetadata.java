/*
   Copyright 2010, 2011, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj;

public interface ColumnMetadata {

    /** Return the name of the column.
     * @return the name of the column
     */
    String name();

    /** Return the type of the column.
     * @return the type of the column
     */
    ColumnType columnType();

    /** Return the java type of the column.
     * @return the java type of the column
     */
    Class<?> javaType();

    /** Return the maximum number of bytes that can be stored in the column
     * after translating the characters using the character set.
     * @return the maximum number of bytes that can be stored in the column
     */
    int maximumLength();

    /** Return the column number. This number is used as the first parameter in
     * the get and set methods of DynamicColumn.
     * @return the column number.
     */
    int number();

    /** Return whether this column is a primary key column.
     * @return true if this column is a primary key column
     */
    boolean isPrimaryKey();

    /** Return whether this column is a partition key column.
     * @return true if this column is a partition key column
     */
    boolean isPartitionKey();

    /** Return the precision of the column.
     * @return the precision of the column
     */
    int precision();

    /** Return the scale of the column.
     * @return the scale of the column
     */
    int scale();

    /** Return whether this column is nullable.
     * @return whether this column is nullable
     */
    boolean nullable();

    /** Return the charset name.
     * @return the charset name
     */
    String charsetName();

}
