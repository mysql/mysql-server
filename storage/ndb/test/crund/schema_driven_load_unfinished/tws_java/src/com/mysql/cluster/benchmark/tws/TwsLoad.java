/* -*- mode: java; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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

package com.mysql.cluster.benchmark.tws;

import java.io.PrintWriter;


abstract class TwsLoad {

    // console
    static protected final PrintWriter out = TwsDriver.out;
    static protected final PrintWriter err = TwsDriver.err;

    // resources
    protected final TwsDriver driver;
    protected String descr;

    protected MetaData metaData;

    protected static String fixedStr = "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx"
            + "xxxxxxxxxx";

    public TwsLoad(TwsDriver driver, MetaData md) {
        this.driver = driver;
        this.metaData = md;
    }

    // ----------------------------------------------------------------------
    // intializers/finalizers
    // ----------------------------------------------------------------------

    abstract protected void initProperties();
    abstract protected void printProperties();

    public String getDescriptor() {
        return descr;
    }

    public void init() throws Exception {
        initProperties();
        printProperties();
    }

    public void close() throws Exception {
    }

    public MetaData getMetaData() {
        return metaData;
    }

    // ----------------------------------------------------------------------
    // benchmark operations
    // ----------------------------------------------------------------------

    abstract public void runOperations() throws Exception;

    // reports an error if a condition is not met
    static protected final void verify(boolean cond) {
        //assert (cond);
        if (!cond)
            throw new RuntimeException("data verification failed.");
    }

    static protected final void verify(int exp, int act) {
        if (exp != act)
            throw new RuntimeException("data verification failed:"
                                       + " expected = " + exp
                                       + ", actual = " + act);
    }

    static protected final void verify(String exp, String act) {
        if ((exp == null && act != null)
            || (exp != null && !exp.equals(act)))
            throw new RuntimeException("data verification failed:"
                                       + " expected = '" + exp + "'"
                                       + ", actual = '" + act + "'");
    }

    // ----------------------------------------------------------------------
    // datastore operations
    // ----------------------------------------------------------------------

    abstract public void initConnection() throws Exception;
    abstract public void closeConnection() throws Exception;
    //abstract public void clearPersistenceContext() throws Exception;
    //abstract public void clearData() throws Exception;
}
