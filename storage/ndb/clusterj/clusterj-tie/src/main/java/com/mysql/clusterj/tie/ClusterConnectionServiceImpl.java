/*
 *  Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.
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

package com.mysql.clusterj.tie;

import com.mysql.clusterj.ClusterJFatalUserException;
import com.mysql.clusterj.core.store.ClusterConnection;
import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

/**
 *
 */
public class ClusterConnectionServiceImpl
        implements com.mysql.clusterj.core.store.ClusterConnectionService {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(ClusterConnectionServiceImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(ClusterConnectionServiceImpl.class);

    static {
        LoggerFactoryService.getFactory().registerLogger("com.mysql.clusterj.tie");
    }

    public ClusterConnection create(String connectString, int nodeId) {
        try {
            return new ClusterConnectionImpl(connectString, nodeId);
        } catch (ClusterJFatalUserException cjex) {
            throw cjex;
        } catch (Exception e) {
            String message = local.message("ERR_Connect", connectString, nodeId);
            logger.fatal(message);
            throw new ClusterJFatalUserException(message, e);
        }
    }

}
