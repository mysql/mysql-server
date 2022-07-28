/*
 *  Copyright (c) 2011, 2022, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is also distributed with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have included with MySQL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License, version 2.0, for more details.
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
