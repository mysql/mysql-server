/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

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
