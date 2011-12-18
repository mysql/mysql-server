/*
 *  Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.
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

    /** Load the ndbclient system library only once */
    static boolean ndbclientLoaded = false;

    static protected void loadSystemLibrary(String name) {
        // this is not thread-protected so it might be executed multiple times but no matter
        if (ndbclientLoaded) {
            return;
        }
        try {
            System.loadLibrary(name);
            // initialize the charset map
            Utility.getCharsetMap();
            ndbclientLoaded = true;
        } catch (Throwable e) {
            String path = getLoadLibraryPath();
            String message = local.message("ERR_Failed_Loading_Library",
                    name, path, e.getClass(), e.getLocalizedMessage());
            logger.fatal(message);
            throw new ClusterJFatalUserException(message, e);
        }
    }

    public ClusterConnection create(String connectString, int nodeId) {
        loadSystemLibrary("ndbclient");
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

    /**
     * @return the load library path or the Exception string
     */
    private static String getLoadLibraryPath() {
        String path;
        try {
            path = System.getProperty("java.library.path");
        } catch (Exception ex) {
            path = "<Exception: " + ex.getMessage() + ">";
        }
        return path;
    }

}
