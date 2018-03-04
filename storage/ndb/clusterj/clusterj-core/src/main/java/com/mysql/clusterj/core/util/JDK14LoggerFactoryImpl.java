/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj.core.util;

import java.util.HashMap;
import java.util.Map;
import java.util.logging.LogManager;

public class JDK14LoggerFactoryImpl implements LoggerFactory {

    /** The root logger name */
    public static final String CLUSTERJ_LOGGER = "com.mysql.clusterj.core";

    /** The metadata logger name */
    public static final String CLUSTERJ_METADATA_LOGGER = "com.mysql.clusterj.core.metadata";

    /** The util logger name */
    public static final String CLUSTERJ_UTIL_LOGGER = "com.mysql.clusterj.core.util";

    /** The query logger name */
    public static final String CLUSTERJ_QUERY_LOGGER = "com.mysql.clusterj.core.query";

    /** The global JDK14 LogManager */
    static final LogManager logManager = LogManager.getLogManager();

    /** The loggers in a map */
    static final Map<String, Logger> loggerMap = new HashMap<String, Logger>();

    /** The constructor */
    public JDK14LoggerFactoryImpl() {
        // configureJDK14Logger();
        // create all the known loggers for the core project
        registerLogger(CLUSTERJ_LOGGER);
        registerLogger(CLUSTERJ_METADATA_LOGGER);
        registerLogger(CLUSTERJ_QUERY_LOGGER);
        registerLogger(CLUSTERJ_UTIL_LOGGER);
    }

    public Logger registerLogger(String loggerName) {
        java.util.logging.Logger logger = java.util.logging.Logger.getLogger(loggerName);
        Logger result = new JDK14LoggerImpl(logger);
        loggerMap.put(loggerName, result);
        return result;
    }

    @SuppressWarnings("unchecked")
    public Logger getInstance(Class cls) {
        String loggerName = getPackageName(cls);
        return getInstance(loggerName);
    }

    public synchronized Logger getInstance(String loggerName) {
        Logger result = loggerMap.get(loggerName);
        if (result == null) {
            result = registerLogger(loggerName);
        }
        return result;
    }

    /**  
     * Returns the package portion of the specified class.
     * @param cls the class from which to extract the 
     * package 
     * @return package portion of the specified class
     */   
    final private static String getPackageName(Class<?> cls)
    {
        String className = cls.getName();
        int index = className.lastIndexOf('.');
        return ((index != -1) ? className.substring(0, index) : ""); // NOI18N
    }

}
