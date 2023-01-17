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

package com.mysql.clusterj.core.query;

import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.core.spi.QueryExecutionContext;
import com.mysql.clusterj.core.store.ScanFilter;
import com.mysql.clusterj.core.store.ScanOperation;
import com.mysql.clusterj.core.store.ScanFilter.Group;

public class NotPredicateImpl extends PredicateImpl {

    /** The predicate being negated */
    private PredicateImpl predicate;

    public NotPredicateImpl(PredicateImpl predicate) {
        super(predicate.dobj);
        this.predicate = predicate;
    }

    @Override
    public void markParameters() {
        // Nothing to do because "not" can't use indexes
    }

    @Override
    public void unmarkParameters() {
        // Nothing to do because "not" can't use indexes
    }

    void markBoundsForCandidateIndices(QueryExecutionContext context,
            CandidateIndexImpl[] candidateIndices) {
        // Nothing to do because "not" can't use indexes
    }

    /** Create a filter for the operation. Call the negated predicate to set
     * the filter values.
     * @param context the query execution context with the parameter values
     * @param op the operation
     */
    public void filterCmpValue(QueryExecutionContext context,
            ScanOperation op) {
        try {
            ScanFilter filter = op.getScanFilter(context);
            filter.begin(Group.GROUP_NAND);
            predicate.filterCmpValue(context, op, filter);
            filter.end();
        } catch (ClusterJException ex) {
            throw ex;
        } catch (Exception ex) {
            throw new ClusterJException(
                    local.message("ERR_Get_NdbFilter"), ex);
        }
    }

    /** Use an existing filter for the operation. Call the negated predicate to set
     * the filter values.
     * @param context the query execution context with the parameter values
     * @param op the operation
     * @param filter the existing filter
     */
    public void filterCmpValue(QueryExecutionContext context,
            ScanOperation op, ScanFilter filter) {
        try {
            filter.begin(Group.GROUP_NAND);
            predicate.filterCmpValue(context, op, filter);
            filter.end();
        } catch (ClusterJException ex) {
            throw ex;
        } catch (Exception ex) {
            throw new ClusterJException(
                    local.message("ERR_Get_NdbFilter"), ex);
        }
    }

}
