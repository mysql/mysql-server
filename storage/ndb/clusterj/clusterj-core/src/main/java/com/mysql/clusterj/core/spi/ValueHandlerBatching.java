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

package com.mysql.clusterj.core.spi;


/** This interface handles retrieving parameter values from the parameterBindings
 * associated with a PreparedStatement or batchedBindValues from a ServerPreparedStatement.
 * It extends ValueHandler with methods to iterate through multiple parameter sets
 * and to get the number of statements in the batch.
 */
public interface ValueHandlerBatching extends ValueHandler {

    /**
     * Advance to the next parameter set. If successful, return true. If there are no more
     * parameter sets, return false.
     * @return true if positioned to a valid parameter set
     */
    public boolean next();

    /** Get the number of statements in the batch. This will be the number of times
     * the next method can be called before it returns false.
     * @return the number of statements in the batch
     */
    public int getNumberOfStatements();

}
