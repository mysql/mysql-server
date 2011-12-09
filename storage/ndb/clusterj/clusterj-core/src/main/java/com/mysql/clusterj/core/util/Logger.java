/*
   Copyright 2010 Sun Microsystems, Inc.
   All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj.core.util;

public interface Logger {

    void detail(String message);
    void debug(String message);
    void trace(String message);
    void info(String message);
    void warn(String message);
    void error(String message);
    void fatal(String message);
    boolean isDetailEnabled();
    boolean isDebugEnabled();
    boolean isTraceEnabled();
    boolean isInfoEnabled();
}
