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

package testsuite.clusterj.domaintypehandler;

import com.mysql.clusterj.core.CacheManager;
import com.mysql.clusterj.core.query.CandidateIndexImpl;
import com.mysql.clusterj.core.spi.DomainFieldHandler;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.DomainTypeHandlerFactory;
import com.mysql.clusterj.core.spi.ValueHandler;
import com.mysql.clusterj.core.store.ClusterTransaction;
import com.mysql.clusterj.core.store.Column;
import com.mysql.clusterj.core.store.Dictionary;
import com.mysql.clusterj.core.store.Operation;
import com.mysql.clusterj.core.store.PartitionKey;
import com.mysql.clusterj.core.store.ResultData;
import com.mysql.clusterj.core.store.Table;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.util.BitSet;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

public class CrazyDomainTypeHandlerFactoryImpl implements DomainTypeHandlerFactory {

    static Map<String, Constructor> constructorMap = new HashMap<String, Constructor>();

    static Class[] exceptions = new Class[] {NullPointerException.class, IllegalAccessException.class};

    static {
        for (Class exception: exceptions) {
            try {
                constructorMap.put(exception.getSimpleName(), exception.getConstructor(new Class[]{}));
            } catch (NoSuchMethodException ex) {
                throw new RuntimeException(ex);
            } catch (SecurityException ex) {
                throw new RuntimeException(ex);
            }
        }
    }

    public <T> DomainTypeHandler<T> createDomainTypeHandler(Class<T> domainClass, Dictionary dictionary) {
        String className = domainClass.getSimpleName();
        if (className.startsWith("Throw")) {
            String throwClassName = className.substring(5);
            try {
                Constructor ctor = constructorMap.get(throwClassName);
                RuntimeException throwable = (RuntimeException) ctor.newInstance(new Object[]{});
                throw throwable;
            } catch (InstantiationException ex) {
                throw new RuntimeException(ex);
            } catch (IllegalAccessException ex) {
                throw new RuntimeException(ex);
            } catch (IllegalArgumentException ex) {
                throw new RuntimeException(ex);
            } catch (InvocationTargetException ex) {
                throw new RuntimeException(ex);
            } catch (SecurityException ex) {
                throw new RuntimeException(ex);
            }
        } else if (className.equals("CrazyDelegate")) {
            final String delegateName = className;
            return new DomainTypeHandler<T>() {

                public CandidateIndexImpl[] createCandidateIndexes() {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public String getName() {
                    return delegateName;
                }

                public boolean isSupportedType() {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public String getTableName() {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public DomainFieldHandler getFieldHandler(String fieldName) {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public Class getProxyClass() {
                    throw new UnsupportedOperationException("Nice Job!");
                }

                public T newInstance() {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public ValueHandler getValueHandler(Object instance) {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public T getInstance(ValueHandler handler) {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public void objectMarkModified(ValueHandler handler, String fieldName) {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public void objectSetValues(ResultData rs, ValueHandler handler) {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public void objectSetValuesExcept(ResultData rs, ValueHandler handler, String indexName) {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public void objectSetCacheManager(CacheManager cm, Object instance) {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public void objectResetModified(ValueHandler handler) {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public void operationGetValues(Operation op) {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public void operationGetValues(Operation op, BitSet fields) {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public void operationSetKeys(ValueHandler handler, Operation op) {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public void operationSetModifiedValues(ValueHandler handler, Operation op) {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public void operationSetModifiedNonPKValues(ValueHandler valueHandler, Operation op) {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public void operationSetNonPKValues(ValueHandler handler, Operation op) {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public ValueHandler createKeyValueHandler(Object keys) {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public int[] getKeyFieldNumbers() {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public Class getOidClass() {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public Set<Column> getStoreColumns(BitSet fields) {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public Table getStoreTable() {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public PartitionKey createPartitionKey(
                        ValueHandler handler) {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public String[] getFieldNames() {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public void operationSetValues(ValueHandler valueHandler,
                        Operation op) {
                    throw new UnsupportedOperationException("Not supported yet.");
                }

                public void objectSetKeys(Object keys, Object instance) {
                    throw new UnsupportedOperationException("Not supported yet.");
                }
            };
        } else {
            return null;
        }
    }

}
