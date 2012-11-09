/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

import com.mysql.clusterj.core.spi.QueryExecutionContext;
import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.store.ScanOperation;

public class IsNotNullPredicateImpl extends PredicateImpl {

    /** My property */
    protected PropertyImpl property;

    /** Construct a new IsNotNull predicate
     * 
     * @param dobj the query domain object that owns this predicate
     * @param property the property
     */
    public IsNotNullPredicateImpl(QueryDomainTypeImpl<?> dobj, PropertyImpl property) {
        super(dobj);
        this.property = property;
    }

    /** Set the condition into the filter.
     * @param context the query execution context with the parameter values (ignored for isNotNull)
     * @param op the operation
     * @param filter the filter
     */
    @Override
    public void filterCmpValue(QueryExecutionContext context, ScanOperation op, ScanFilter filter) {
        property.filterIsNotNull(filter);
    }

}
