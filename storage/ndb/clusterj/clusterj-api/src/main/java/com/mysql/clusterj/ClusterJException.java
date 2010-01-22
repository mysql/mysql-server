/*
   Copyright (C) 2009 Sun Microsystems Inc.
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

package com.mysql.clusterj;

/**
 * ClusterJException is the base for all ClusterJ exceptions.
 */
public class ClusterJException extends RuntimeException {

    private static final long serialVersionUID = 3803389948396170712L;

    public ClusterJException(String message) {
        super(message);
    }

    public ClusterJException(String message, Throwable t) {
        super(message + " Caused by " + t.getClass().getName() + ":" + t.getMessage(), t);
    }

    public ClusterJException(Throwable t) {
        super(t);
    }

    @Override
    public synchronized void printStackTrace(java.io.PrintStream s) {
        synchronized (s) {
            super.printStackTrace(s);
            Throwable cause = getCause();
            if (cause != null) {
                getCause().printStackTrace(s);
            }
        }
    }
}
