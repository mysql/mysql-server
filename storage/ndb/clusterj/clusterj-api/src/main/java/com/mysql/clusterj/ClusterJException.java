/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj;

/**
 * ClusterJException is the base for all ClusterJ exceptions.
 * Applications can catch ClusterJException to be notified of
 * all ClusterJ reported issues.
 * <p>
 * Exceptions are in three general categories: User exceptions,
 * Datastore exceptions, and Internal exceptions.
 * <ul><li>User exceptions are caused by user error, for example
 * providing a connect string that refers to an unavailable
 * host or port.
 * <ul><li>If a user exception is detected during bootstrapping
 * (acquiring a SessionFactory), it is thrown as a fatal exception.
 * {@link ClusterJFatalUserException}
 * </li><li>If an exception is detected during initialization of a
 * persistent interface, for example annotating a column that
 * doesn't exist in the mapped table, it is reported as a user
 * exception. {@link ClusterJUserException}
 * </li></ul>
 * </li><li>Datastore exceptions report conditions that result
 * from datastore operations after bootstrapping. For example,
 * duplicate keys on insert, or record does not exist on delete.
 * {@link ClusterJDatastoreException}
 * </li><li>Internal exceptions report conditions that are caused
 * by errors in implementation. These exceptions should be reported
 * as bugs. {@link ClusterJFatalInternalException}
 * </li></ul>
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
