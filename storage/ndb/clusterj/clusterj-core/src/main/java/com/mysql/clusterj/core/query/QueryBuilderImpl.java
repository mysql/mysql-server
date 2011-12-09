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

package com.mysql.clusterj.core.query;


import com.mysql.clusterj.core.*;
import com.mysql.clusterj.core.spi.DomainTypeHandler;
import com.mysql.clusterj.query.QueryBuilder;

public class QueryBuilderImpl implements QueryBuilder {

    /** My session. */
    protected SessionImpl session;

    /** Construct a query builder.
     * 
     * @param session the session
     */
    public QueryBuilderImpl(SessionImpl session) {
        this.session = session;
    }

    /** Create a query definition for the named class.
     * @param cls the class of the candidate
     * @return the query definition
     */
    public <T> QueryDomainTypeImpl<T> createQueryDefinition(Class<T> cls) {
	DomainTypeHandler<T> domainTypeHandler = session.getDomainTypeHandler(cls);
        return new QueryDomainTypeImpl<T>(domainTypeHandler, cls);
    }

    /** Create a query definition for the domain type handler (no class mapping).
     * @param domainTypeHandler the domain type handler
     * @return the query definition
     */
    public <T> QueryDomainTypeImpl<T> createQueryDefinition(DomainTypeHandler<T> domainTypeHandler) {
        return new QueryDomainTypeImpl<T>(domainTypeHandler);
    }

}
