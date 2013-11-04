/*
  Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.cluster.crund;

import com.mysql.cluster.crund.CrundDriver.XMode;

abstract class CrundSLoad extends Load {
    // resources
    protected final CrundDriver driver;

    public CrundSLoad(CrundDriver driver) {
        this.driver = driver;
        driver.addLoad(this);
    }

    // ----------------------------------------------------------------------
    // intializers/finalizers
    // ----------------------------------------------------------------------

    abstract protected void initProperties();
    abstract protected void printProperties();

    public void init() throws Exception {
        initProperties();
        printProperties();
    }

    public void close() throws Exception {}

    // ----------------------------------------------------------------------
    // datastore operations
    // ----------------------------------------------------------------------

    abstract public void initConnection() throws Exception;
    abstract public void closeConnection() throws Exception;
    abstract public void clearData() throws Exception;

    // ----------------------------------------------------------------------
    // benchmark operations
    // ----------------------------------------------------------------------

    abstract protected void clearPersistenceContext();
    abstract protected void runInsert(XMode mode, int[] id) throws Exception;
    abstract protected void runLookup(XMode mode, int[] id) throws Exception;
    abstract protected void runUpdate(XMode mode, int[] id) throws Exception;
    abstract protected void runDelete(XMode mode, int[] id) throws Exception;

    // runs a sequence of benchmark operations
    public void runOperations(int nOps) throws Exception {
        final int[] id = new int[nOps];
        for (int i = 0; i < nOps; i++)
            id[i] = i * 2;

        for (XMode m : driver.xModes) {
            clearPersistenceContext();
            runInsert(m, id);
            clearPersistenceContext();
            runLookup(m, id);
            clearPersistenceContext();
            runUpdate(m, id);
            clearPersistenceContext();
            runDelete(m, id);
        }
        // XXX failing fast, not yet using driver.failOnError, driver.logError()
        //driver.abortIfErrors();
    }

    // ----------------------------------------------------------------------
    // helpers
    // ----------------------------------------------------------------------

    static protected final void verify(boolean cond) {
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
        if (exp == null ? act != null : !exp.equals(act))
            throw new RuntimeException("data verification failed:"
                                       + " expected = '" + exp + "'"
                                       + ", actual = '" + act + "'");
    }
}
