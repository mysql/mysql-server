/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

/**
 *
 */
public interface Index {

    /** Is this index unique?
     * 
     * @return true if the index is unique
     */
    public boolean isUnique();

    /** Get the external name of the index,
     * i.e the name used to create the index.
     * 
     * @return the name
     */
    public String getName();

    /** Get the actual name of the index,
     * e.g. idx_name_hash$unique.
     * 
     * @return the actual name of the index
     */
    public String getInternalName();

    /** Get the names of the columns in this index, in the order
     * they are declared in the KEY clause of the CREATE TABLE
     * statement.
     * 
     * @return the column names
     */
    public String[] getColumnNames();

}
