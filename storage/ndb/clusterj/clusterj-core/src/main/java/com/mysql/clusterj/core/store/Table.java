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
public interface Table {

    Column getColumn(String columnName);

    String getName();

    /** Get the names of the primary key columns */
    String[] getPrimaryKeyColumnNames();

    /** Get the names of the partition key columns */
    String[] getPartitionKeyColumnNames();

    /** Create a partition key for this table */
    public PartitionKey createPartitionKey();

    /** Get the names of the indexes on this table */
    public String[] getIndexNames();

    /** Get the names of all columns for this table */
    String[] getColumnNames();

}
