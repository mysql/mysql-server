/*
 *  Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.
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

package com.mysql.clusterj.core.util;

public class DeMinimisLoggerFactory implements LoggerFactory {

    private static Logger instance = new DeMinimisLogger();
    
    /** Get an instance of the logger. The logger to get is based on the
     * package name of the class. If there is no logger for the package,
     * the parent package name is tried, recursively. If there is no logger
     * for the topmost package name, an exception is thrown.
     * @param cls the class for which to get the logger
     * @return the logger for the class
     */
    public Logger getInstance(Class<?> cls) {
        return instance;
    }

    /** Get an instance of the logger. The logger is configured
     * based on the name. The logger must already exist.
     * @param loggerName the name of the logger, normally the package name
     * @return the logger
     */
    public Logger getInstance(String loggerName) {
        return instance;
    }

    /** Register an instance of the logger.
     *
     * @param loggerName the name of the logger, normally the package name
     */
    public Logger registerLogger(String loggerName) { return instance; }
}
