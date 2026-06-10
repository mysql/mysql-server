/*
   Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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
 *
 * ClusterJTableException is used for reporting all "Table Not Found" conditions
 * in Cluster/J. This includes "Table Not Found" with no associated errors, as
 * when Cluster/J attempts to get metadata for a named table, but the named
 * table is not defined in NDB, and no other errors are reported. It also includes
 * "Table Not Found" with some additional error, such as a network error. The
 * presence of the accompanying error raises some uncertainty about whether the
 * named table may or may not exist in the database.
 *
 * ClusterJTableException is derived from class ClusterJDatastoreException, and
 * in both the error and no-error cases, ClusterJDatastoreException.tableNotFound()
 * will return true. In the case with an additional error, the method
 * ClusterJTableException.hasError() will also return true.
 *
 * ClusterJTableException also provides methods for reporting the name of the
 * table used in the search, and the elapsed time spent attempting to get the
 * table.
 *
 * Commonly, a Cluster/J application may expect a table to exist, but have
 * to wait some unspecified amount of time for some other system to create
 * the table. Waits of any duration can be implemented.
 *
 * Short waits can be managed by setting the SessionFactory property
 * com.mysql.clusterj.table.wait.msec to an integer value between 0 and 1000.
 * This value is used as a time limit in a sleep-and-retry loop inside
 * Cluster/J, and will be applied during every attempt to open a table.
 *
 * Longer waits can be managed by the application looping around a call such
 * as session.newInstance(), testing for ClusterJTableException, and then using
 * elapsedMillis() to account for the elapsed time reported by each exception.
 *
 * Before Cluster/J version 9.4.0, some "Table Not Found" conditions were
 * reported using ClusterJUserException, without any checks for an underlying
 * error, and others were reported with ClusterJDatastoreException.
 */

public class ClusterJTableException extends ClusterJDatastoreException {

    private static final long serialVersionUID = 5417828324982160757L;

    private final String tableName;

    public final long startNanos, endNanos; // from System.nanoTime()

    public boolean hasError() {
        return (code != 723);
    }

    public String getTableName() {
        return tableName;
    }

    public long elapsedNanos() {
        return endNanos - startNanos;
    }

    public int elapsedMicros() {
        return (int) (elapsedNanos() / 1000);
    }

    public int elapsedMillis() {
        return (int) (elapsedNanos() / 1000000);
    }

    public ClusterJTableException(String tableName, long start, long end, String msg,
                                  int code1, int code2, int status, int classify) {
        super(msg, code1, code2, status, classify);
        this.tableName = tableName;
        this.startNanos = start;
        this.endNanos = end;
    }

}
