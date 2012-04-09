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

package com.mysql.clusterj.openjpa;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import org.apache.openjpa.jdbc.kernel.JDBCStoreQuery;
import org.apache.openjpa.jdbc.meta.ClassMapping;
import org.apache.openjpa.kernel.exps.ExpressionFactory;
import org.apache.openjpa.kernel.exps.ExpressionParser;
import org.apache.openjpa.kernel.exps.QueryExpressions;
import org.apache.openjpa.meta.ClassMetaData;

/**
 *
 */
@SuppressWarnings("serial")
public class NdbOpenJPAStoreQuery extends JDBCStoreQuery {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(NdbOpenJPAStoreQuery.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(NdbOpenJPAStoreQuery.class);

    NdbOpenJPAStoreQuery(NdbOpenJPAStoreManager storeManager,
            ExpressionParser expressionParser) {
        super(storeManager, expressionParser);
    }

    @Override
    protected Number executeDelete(Executor ex, ClassMetaData base,
        ClassMetaData[] metas, boolean subclasses, ExpressionFactory[] facts,
        QueryExpressions[] exps, Object[] params) {
        if (logger.isDebugEnabled()) {
            logger.debug("NdbOpenJPAStoreQuery.executeDelete(Executor ex, ClassMetaData base, " +
                    "ClassMetaData[] metas, boolean subclasses, ExpressionFactory[] facts, " +
                    "QueryExpressions[] exps, Object[] params).\n" +
                    "Class: " + base.getTypeAlias() + 
                    " query expressions: " + exps + "[" + exps.length + "]" +
                    " exps[0].filter: " + exps[0].filter);
        }
        NdbOpenJPAStoreManager store = (NdbOpenJPAStoreManager)getStore();
        NdbOpenJPADomainTypeHandlerImpl<?> domainTypeHandler = store.getDomainTypeHandler((ClassMapping)base);
        if (domainTypeHandler.isSupportedType() 
                && exps.length == 1 
                && exps[0].filter.getClass().getName().contains("EmptyExpression")) {
            // filter is empty so delete the entire extent
            if (logger.isDebugEnabled()) {
                logger.debug("Empty Expression for delete will delete the entire extent.");
            }
            int count = store.deleteAll(domainTypeHandler);
            return count;
        } else {
            if (logger.isDebugEnabled()) logger.debug("NdbOpenJPAStoreQuery.executeDelete delegated to super.");
            return super.executeDelete(ex, base, metas, subclasses, facts, exps, params);
        }
    }

}
