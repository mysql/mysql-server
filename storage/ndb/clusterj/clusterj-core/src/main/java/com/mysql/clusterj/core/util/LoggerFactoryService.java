/*
   Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.

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

import com.mysql.clusterj.Constants;

public class LoggerFactoryService {

    /** The factory for creating logger instances. The default is to use
     * the JDK14 logging package. If other loggers are desired, change the
     * environment variable to use a different logger factory. To avoid most
     * logging overhead, specify
     * export CLUSTERJ_LOGGER_FACTORY=com.mysql.clusterj.core.util.DeMinimisLoggerFactory
     */
    private static LoggerFactory instance = new JDK14LoggerFactoryImpl();

    static {
        String loggerFactoryName = System.getenv(Constants.ENV_CLUSTERJ_LOGGER_FACTORY_NAME);
        if (loggerFactoryName != null) {
            try {
                instance = LoggerFactory.class.cast(
                        LoggerFactory.class.forName(loggerFactoryName)
                        .newInstance());
            } catch (Throwable t) {
                System.err.println("ClusterJ user exception: could not load class " + loggerFactoryName);
            }
        }
    }

    /** The singleton logger factory.
     * 
     * @return the factory
     */
    public static LoggerFactory getFactory() {
        return instance;
    }

}
