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

import com.mysql.clusterj.core.spi.ValueHandler;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/** This interface handles retrieving parameter values from the parameterBindings
 * associated with a PreparedStatement or batchedBindValues from a ServerPreparedStatement.
 */
public interface ValueHandlerBatching extends ValueHandler {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(ValueHandlerBatching.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(ValueHandlerBatching.class);

    /**
     * Advance to the next parameter set. If successful, return true. If there are no more
     * parameter sets, return false.
     * @return true if positioned to a valid parameter set
     */
    public boolean next();

    public int getNumberOfStatements();

}
