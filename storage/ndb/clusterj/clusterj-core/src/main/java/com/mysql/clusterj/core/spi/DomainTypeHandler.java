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

package com.mysql.clusterj.core.spi;

import com.mysql.clusterj.core.CacheManager;
import com.mysql.clusterj.core.query.CandidateIndexImpl;
import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.PartitionKey;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.store.Table;

import java.util.BitSet;
import java.util.Set;

/** DomainTypeHandler is the interface that must be implemented to allow core
 * components to manage instances of a persistent class or interface.
 *
 */
public interface DomainTypeHandler<T> {

    public CandidateIndexImpl[] createCandidateIndexes();

    public String getName();

    public Class<?> getOidClass();

    public boolean isSupportedType();

    public String getTableName();

    public DomainFieldHandler getFieldHandler(String fieldName);

    public Class<T> getProxyClass();

    public T newInstance();

    public ValueHandler getValueHandler(Object instance);

    public T getInstance(ValueHandler handler);

    public void objectMarkModified(ValueHandler handler, String fieldName);

    public void objectSetValues(ResultData rs, ValueHandler handler);

    public void objectSetKeys(Object keys, Object instance);

    public void objectSetCacheManager(CacheManager cm, Object instance);

    public void objectResetModified(ValueHandler handler);

    public void operationGetValues(Operation op);

    public void operationGetValues(Operation op, BitSet fields);

    public void operationSetKeys(ValueHandler handler, Operation op);

    public void operationSetNonPKValues(ValueHandler handler, Operation op);

    public void operationSetModifiedValues(ValueHandler handler, Operation op);

    public void operationSetModifiedNonPKValues(ValueHandler valueHandler, Operation op);

    public ValueHandler createKeyValueHandler(Object keys);

    public int[] getKeyFieldNumbers();

    public Set<Column> getStoreColumns(BitSet fields);

    public Table getStoreTable();

    public PartitionKey createPartitionKey(ValueHandler handler);

    public String[] getFieldNames();

    public void operationSetValues(ValueHandler valueHandler, Operation op);

}
