/*
   Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

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


/** This interface represents a partition key. A partition key
 * consists of a table and values that make up the partition key.
 * An instance of this interface can be used to specify the
 * partition key used when starting a transaction.
 */
public interface PartitionKey {

    public void addIntKey(Column storeColumn, int key);

    public void addLongKey(Column storeColumn, long key);

    public void addStringKey(Column storeColumn, String string);

    public void addBytesKey(Column storeColumn, byte[] bytes);

    public void addShortKey(Column storeColumn, short key);

    public void addByteKey(Column storeColumn, byte key);

}
