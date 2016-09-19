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
