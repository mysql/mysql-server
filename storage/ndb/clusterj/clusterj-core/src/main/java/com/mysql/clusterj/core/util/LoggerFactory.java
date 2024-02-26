/*
   Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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

public interface LoggerFactory {

    /** Get an instance of the logger. The logger to get is based on the
     * package name of the class. If there is no logger for the package,
     * the parent package name is tried, recursively. If there is no logger
     * for the topmost package name, an exception is thrown.
     * @param cls the class for which to get the logger
     * @return the logger for the class
     */
    public Logger getInstance(Class<?> cls);

    /** Get an instance of the logger. The logger is configured
     * based on the name. The logger must already exist.
     * @param loggerName the name of the logger, normally the package name
     * @return the logger
     */
    public Logger getInstance(String loggerName);

    /** Register an instance of the logger.
     *
     * @param loggerName the name of the logger, normally the package name
     */
    public Logger registerLogger(String loggerName);
}
