/*
 *  Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.
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

package com.mysql.clusterj.core.store;

import com.mysql.clusterj.core.spi.ValueHandlerFactory;

/**
 *
 */
public interface ClusterConnection {

    public void connect(int connectRetries, int connectDelay, boolean verbose);

    public Db createDb(String database, int maxTransactions);

    public void waitUntilReady(int connectTimeoutBefore, int connectTimeoutAfter);

    public void closing();

    public void close();

    public int dbCount();

    public void close(Db db);

    public void unloadSchema(String tableName);

    public ValueHandlerFactory getSmartValueHandlerFactory();

    public void initializeAutoIncrement(long[] autoIncrement);

    public void setByteBufferPoolSizes(int[] poolSizes);
}
