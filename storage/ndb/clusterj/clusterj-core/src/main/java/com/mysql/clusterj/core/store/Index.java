/*
   Copyright (c) 2010, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

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
