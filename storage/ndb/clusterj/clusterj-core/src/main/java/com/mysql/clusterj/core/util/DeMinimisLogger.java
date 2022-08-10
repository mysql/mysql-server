/*
 *  Copyright (c) 2016, 2022, Oracle and/or its affiliates.
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

package com.mysql.clusterj.core.util;

public class DeMinimisLogger implements Logger {

    public DeMinimisLogger() {}

    public void detail(String message) {}
    public void debug(String message) {}
    public void trace(String message) {}
    public void info(String message) {System.out.println(message);}
    public void warn(String message) {System.err.println(message);}
    public void error(String message) {System.err.println(message);}
    public void fatal(String message) {System.err.println(message);}
    public boolean isDetailEnabled() {return false;}
    public boolean isDebugEnabled() {return false;}
    public boolean isTraceEnabled() {return false;}
    public boolean isInfoEnabled() {return false;}
}
