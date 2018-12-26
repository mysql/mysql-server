/*
  Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.cluster.crund;

import java.nio.CharBuffer;

import java.util.List;
import java.util.Arrays;
import java.util.ArrayList;

abstract public class CrundLoad extends Load {
    // resources
    protected final CrundDriver driver;

    public CrundLoad(CrundDriver driver) {
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

    abstract protected void initOperations() throws Exception;
    abstract protected void closeOperations() throws Exception;
    abstract protected void clearPersistenceContext() throws Exception;

    // a benchmark operation
    protected abstract class Op {
        protected String name;

        public Op(String name) { this.name = name; }

        public String getName() { return name; }

        public abstract void run(int[] id) throws Exception;
    };

    // the sequence of benchmark operations
    protected final List<Op> ops = new ArrayList<Op>();

    // runs a sequence of benchmark operations
    public void runOperations(int nOps) throws Exception {
        final int[] id = new int[nOps];
        for (int i = 0; i < nOps; i++)
            id[i] = i * 2;

        for (Op op : ops) {
            // clear any data/result caches before the next transaction
            clearPersistenceContext();
            runOperation(op, id);
        }

        driver.abortIfErrors();
    }

    // runs a benchmark operation
    protected void runOperation(Op op, int[] id) throws Exception {
        final String on = op.getName();
        if (on == null)
            return;

        if (!excludedOperation(on)) {
            driver.beginOp(on);
            try {
                op.run(id);
            } catch (Exception e) {
                driver.logError(getName(), op.getName(), e);
            }
            driver.finishOp(on, id.length);
        }
    }

    // skip operation if excluded or not in non-empty includes list
    protected boolean excludedOperation(String name) {
        for (String r : driver.exclude)
            if (name.matches(r)) {
                //out.println("*** exclude " + name + ": match (" + r + ")");
                return true;
            }

        if (driver.include.isEmpty()) {
            //out.println("*** include " + name + ": empty includes");
            return false;
        }

        for (String r : driver.include)
            if (name.matches(r)) {
                //out.println("*** include " + name + ": match (" + r + ")");
                return false;
            }

        //out.println("*** exclude " + name + ": non-match includes");
        return true;
    }

    // ----------------------------------------------------------------------
    // helpers
    // ----------------------------------------------------------------------

    static protected final void verify(boolean cond) {
        if (!cond)
            throw new RuntimeException("data verification failed.");
    }

    static protected final void verify(int exp, long act) {
        if (act != exp)
            throw new RuntimeException("data verification failed:"
                                       + " expected = '" + exp + "'"
                                       + ", actual = '" + act + "'");
    }

    static protected final void verify(int exp, double act) {
        if (act != exp)
            throw new RuntimeException("data verification failed:"
                                       + " expected = '" + exp + "'"
                                       + ", actual = '" + act + "'");
    }

    static protected final void verify(byte[] exp, byte[] act) {
        if (!Arrays.equals(exp, act))
            throw new RuntimeException("data verification failed:"
                                       + " expected = " + Arrays.toString(exp)
                                       + ""
                                       + ", actual = " + Arrays.toString(act));
    }

    static protected final void verify(String exp, String act) {
        if (exp == null ? act != null : !exp.equals(act))
            throw new RuntimeException("data verification failed:"
                                       + " expected = '" + exp + "'"
                                       + ", actual = '" + act + "'");
    }

    static protected final void verify(CharBuffer exp, CharBuffer act) {
        if (exp == null ? act != null : !exp.equals(act)) {
            throw new RuntimeException("data verification failed:"
                                       + " expected = '" + exp + "'"
                                       + ", actual = '" + act + "'");
        }
    }

/*
    static protected final <T> void verify(T exp, T act) {
        if (exp == null ? act != null : !exp.equals(act))
            throw new RuntimeException("data verification failed:"
                                       + " expected = '" + exp + "'"
                                       + ", actual = '" + act + "'");
    }
*/

    static final protected String myString(int n) {
        final StringBuilder s = new StringBuilder();
        switch (n) {
        case 1:
            s.append('i');
            break;
        case 2:
            //for (int i = 0; i < 10; i++) s.append('x');
            s.append("0123456789");
            break;
        case 3:
            for (int i = 0; i < 100; i++) s.append('c');
            break;
        case 4:
            for (int i = 0; i < 1000; i++) s.append('m');
            break;
        case 5:
            for (int i = 0; i < 10000; i++) s.append('X');
            break;
        case 6:
            for (int i = 0; i < 100000; i++) s.append('C');
            break;
        case 7:
            for (int i = 0; i < 1000000; i++) s.append('M');
            break;
        default:
            throw new IllegalArgumentException("unsupported 10**n = " + n);
        }
        return s.toString();
    }

    static final protected byte[] myBytes(String s) {
        final char[] c = s.toCharArray();
        final int n = c.length;
        final byte[] b = new byte[n];
        for (int i = 0; i < n; i++) b[i] = (byte)c[i];
        return b;
    }

    // some string and byte constants
    static final protected String string1 = myString(1);
    static final protected String string2 = myString(2);
    static final protected String string3 = myString(3);
    static final protected String string4 = myString(4);
    static final protected String string5 = myString(5);
    static final protected String string6 = myString(6);
    static final protected String string7 = myString(7);
    static final protected byte[] bytes1 = myBytes(string1);
    static final protected byte[] bytes2 = myBytes(string2);
    static final protected byte[] bytes3 = myBytes(string3);
    static final protected byte[] bytes4 = myBytes(string4);
    static final protected byte[] bytes5 = myBytes(string5);
    static final protected byte[] bytes6 = myBytes(string6);
    static final protected byte[] bytes7 = myBytes(string7);
    static final protected String[] strings
        = { string1, string2, string3, string4, string5, string6, string7 };
    static final protected byte[][] bytes
        = { bytes1, bytes2, bytes3, bytes4, bytes5, bytes6, bytes7 };
}
