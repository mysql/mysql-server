/*
 *  Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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

package com.mysql.clusterj.jdbc;

import java.sql.SQLException;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;
import com.mysql.jdbc.ResultSetInternalMethods;

/** This class is part of the statement interceptor contract with the MySQL JDBC connection.
 * When a statement is intercepted and executed, an instance of this class is returned if there
 * is only an insert/delete/update count. A sibling class, ResultSetInternalMethodsImpl, is
 * returned if there is a real result to be iterated.
 * This class in turn delegates to the clusterj ResultData to retrieve data from the cluster.
 */
public class ResultSetInternalMethodsUpdateCount extends
        AbstractResultSetInternalMethods {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(ResultSetInternalMethodsUpdateCount.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(ResultSetInternalMethodsUpdateCount.class);

    /** Counts for multi-statement result */
    private long[] counts;

    /** The current result set for multi-statement results */
    private int current = 0;

    /** Construct an instance with the count to be returned.
     * 
     * @param count the number of "rows affected"
     */
    public ResultSetInternalMethodsUpdateCount(long count) {
        this.counts = new long[1];
        this.counts[0] = count;
    }

    /** Construct an instance with the count to be returned.
     * 
     * @param counts the number of "rows affected" for each row of a multi-statement result
     */
    public ResultSetInternalMethodsUpdateCount(long[] counts) {
        this.counts = counts;
    }

    /**
     * Returns the next ResultSet in a multi-resultset "chain", if any, 
     * null if none exists.
     * @return the next ResultSet
     */
    @Override
    public ResultSetInternalMethods getNextResultSet() {
        if (++current >= counts.length) {
            return null;
        } else {
            return this;
        }
    }

    /**
     * Clears the reference to the next result set in a multi-result set
     * "chain".
     */
    @Override
    public void clearNextResult() {
        // nothing to do
    }

    @Override
    public long getUpdateCount() {
        return counts[current];
    }

    @Override
    public long getUpdateID() {
        return 0;
    }

    @Override
    public boolean reallyResult() {
        return false;
    }

    @Override
    public void realClose(boolean arg0) throws SQLException {
    }
 
}
