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
