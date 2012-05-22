/*
   Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj.tie;

import com.mysql.clusterj.core.metadata.DomainTypeHandlerImpl;
import com.mysql.clusterj.core.spi.ValueHandler;
import com.mysql.clusterj.core.spi.ValueHandlerFactory;
import com.mysql.clusterj.core.store.Db;
import com.mysql.clusterj.core.store.ResultData;

public class NdbRecordSmartValueHandlerFactoryImpl implements ValueHandlerFactory {

    public <T> ValueHandler getValueHandler(DomainTypeHandlerImpl<T> domainTypeHandler, Db db) {
        return new NdbRecordSmartValueHandlerImpl(domainTypeHandler, db);
    }

    public <T> ValueHandler getKeyValueHandler(DomainTypeHandlerImpl<T> domainTypeHandler, Db db,
            Object keyValues) {
        NdbRecordSmartValueHandlerImpl result = new NdbRecordSmartValueHandlerImpl(domainTypeHandler, db);
        domainTypeHandler.objectSetKeys(keyValues, result);
        return result;
    }

    public <T> ValueHandler getValueHandler(
            DomainTypeHandlerImpl<T> domainTypeHandler, Db db, ResultData resultData) {
        NdbRecordSmartValueHandlerImpl result;
        result = new NdbRecordSmartValueHandlerImpl(domainTypeHandler, db, resultData);
        return result;
    }

}
