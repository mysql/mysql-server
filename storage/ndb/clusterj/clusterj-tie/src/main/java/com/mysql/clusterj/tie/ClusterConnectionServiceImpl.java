/*
 *  Copyright (c) 2010, 2023, Oracle and/or its affiliates.
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
import com.mysql.ndbjtie.ndbapi.Ndb;
import com.mysql.ndbjtie.ndbapi.NdbDictionary;
import com.mysql.ndbjtie.ndbapi.NdbOperation;
import com.mysql.ndbjtie.ndbapi.NdbScanOperation;

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

    /** Size of OperationOptions, needed for some ndb apis */
    static int SIZEOF_OPERATION_OPTIONS;

    /** Size of PartitionSpec, needed for some ndb apis */
    static int SIZEOF_PARTITION_SPEC;

    /** Size of RecordSpecification, needed for some ndb apis */
    static int SIZEOF_RECORD_SPECIFICATION;

    /** Size of ScanOptions, needed for some ndb apis */
    static int SIZEOF_SCAN_OPTIONS;

    static protected void loadSystemLibrary(String name) {
        // this is not thread-protected so it might be executed multiple times but no matter
        if (ndbclientLoaded) {
            return;
        }
        try {
            System.loadLibrary(name);
            // initialize the charset map
            Utility.getCharsetMap();
            // get the size information for Ndb structs as needed by some ndb apis
            SIZEOF_OPERATION_OPTIONS = NdbOperation.OperationOptions.size();
            SIZEOF_PARTITION_SPEC = Ndb.PartitionSpec.size();
            SIZEOF_RECORD_SPECIFICATION = NdbDictionary.RecordSpecification.size();
            SIZEOF_SCAN_OPTIONS = NdbScanOperation.ScanOptions.size();
            ndbclientLoaded = true;
        } catch (Throwable e) {
            String path = getLoadLibraryPath();
            String message = local.message("ERR_Failed_Loading_Library",
                    name, path, e.getClass(), e.getLocalizedMessage());
            logger.fatal(message);
            throw new ClusterJFatalUserException(message, e);
        }
    }

    public ClusterConnection create(String connectString, int nodeId, int connectTimeoutMgm) {
        loadSystemLibrary("ndbclient");
        try {
            return new ClusterConnectionImpl(connectString, nodeId, connectTimeoutMgm);
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
