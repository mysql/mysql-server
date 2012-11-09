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

package com.mysql.clusterj.core.metadata;

import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.core.spi.DomainTypeHandlerFactory;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.core.spi.ValueHandlerFactory;
import com.mysql.clusterj.core.store.Dictionary;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import java.util.List;

/**
 *
 */
public class DomainTypeHandlerFactoryImpl implements DomainTypeHandlerFactory {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(DomainTypeHandlerFactoryImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(DomainTypeHandlerFactoryImpl.class);

    protected static List<DomainTypeHandlerFactory> domainTypeHandlerFactories;
    protected static StringBuffer domainTypeHandlerFactoryErrorMessages = new StringBuffer();
    static {
        domainTypeHandlerFactories = ClusterJHelper.getServiceInstances(
                DomainTypeHandlerFactory.class, 
                Thread.currentThread().getContextClassLoader(),
                domainTypeHandlerFactoryErrorMessages);
        logger.info("Found " + domainTypeHandlerFactories.size() + " DomainTypeHandlerFactories");
        for (DomainTypeHandlerFactory factory: domainTypeHandlerFactories) {
            logger.info(factory.toString());
        }
    }

    public <T> DomainTypeHandler<T> createDomainTypeHandler(Class<T> domainClass, Dictionary dictionary,
            ValueHandlerFactory valueHandlerFactory) {
        DomainTypeHandler<T> handler = null;
        StringBuffer errorMessages = new StringBuffer();
        for (DomainTypeHandlerFactory factory: domainTypeHandlerFactories) {
            try {
                errorMessages.append("Trying factory ");
                errorMessages.append(factory.toString());
                errorMessages.append("\n");
                handler = factory.createDomainTypeHandler(domainClass, dictionary, valueHandlerFactory);
                if (handler != null) {
                    return handler;
                }
            } catch (Exception ex) {
                errorMessages.append("Caught exception: ");
                errorMessages.append(ex.toString());
                errorMessages.append("\n");
            }
        }
        // none of the factories can handle it; default to the standard factory

        try {
            errorMessages.append("Trying standard factory com.mysql.clusterj.core.metadata.DomainTypeHandlerImpl.\n");
            handler = new DomainTypeHandlerImpl<T>(domainClass, dictionary, valueHandlerFactory);
            return handler;
        } catch (ClusterJException e) {
            errorMessages.append(e.toString());
            throw e;
        } catch (Exception e) {
            errorMessages.append(e.toString());
            throw new ClusterJUserException(errorMessages.toString(), e);
        } finally {
            // if handler is null, there may be a problem with the schema, so remove it from the local dictionary
            if (handler == null) {
                String tableName = DomainTypeHandlerImpl.getTableName(domainClass);
                if (tableName != null) {
                    logger.info(local.message("MSG_Removing_Schema", tableName, domainClass.getName()));
                    dictionary.removeCachedTable(tableName);                    
                }
            }
        }
    }

}
