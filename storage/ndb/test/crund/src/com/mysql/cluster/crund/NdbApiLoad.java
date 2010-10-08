/* -*- mode: java; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (C) 2008 MySQL
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

package com.mysql.cluster.crund;

public class NdbApiLoad extends NdbBase {

    // ----------------------------------------------------------------------
    // NDB API intializers/finalizers
    // ----------------------------------------------------------------------

    protected void initProperties() {
        super.initProperties();
        descr = "->ndbapi(" + mgmdConnect + ")";
    }

    // initializes/finalizes the NDB benchmark class
    // a non-zero value in case of an error, and zero otherwise
    static protected native int ndbinit(String mgmd_host_portno);
    static protected native int ndbclose();

    protected void init() throws Exception {
        super.init();

        // load dependent libs first
        out.println();
        loadSystemLibrary("ndbclient");
        loadSystemLibrary("crundndb");

        // initialize NDB resources
        final int ret = ndbinit(mgmdConnect);
        if (ret != 0) {
            String msg = ("NdbApiLoad: failed initializing NDBAPI;"
                          + " return value = " + ret);
            err.println(msg);
            throw new Exception(msg);
        }
    }

    protected void close() throws Exception {
        // release NDB resources
        final int ret = ndbclose();
        if (ret != 0) {
            String msg = ("NdbApiLoad: failed closing NDBAPI;"
                          + " return value = " + ret);
            err.println(msg);
            throw new Exception(msg);
        }
        super.close();
    }

    // ----------------------------------------------------------------------
    // NDB API operations
    // ----------------------------------------------------------------------

    protected native void delAllA(int countA, int countB,
                                  boolean batch);
    protected native void delAllB0(int countA, int countB,
                                   boolean batch);
    protected native void insA(int countA, int countB,
                               boolean setAttrs, boolean batch);
    protected native void insB0(int countA, int countB,
                               boolean setAttrs, boolean batch);
    protected native void delAByPK(int countA, int countB,
                                   boolean batch);
    protected native void delB0ByPK(int countA, int countB,
                                    boolean batch);
    protected native void setAByPK(int countA, int countB,
                                   boolean batch);
    protected native void setB0ByPK(int countA, int countB,
                                    boolean batch);
    protected native void getAByPK_bb(int countA, int countB,
                                      boolean batch);
    protected native void getB0ByPK_bb(int countA, int countB,
                                       boolean batch);
    protected native void getAByPK_ar(int countA, int countB,
                                      boolean batch);
    protected native void getB0ByPK_ar(int countA, int countB,
                                       boolean batch);
    protected native void setVarbinary(int countA, int countB,
                                       boolean batch, int length);
    protected native void getVarbinary(int countA, int countB,
                                       boolean batch, int length);
    protected native void setVarchar(int countA, int countB,
                                     boolean batch, int length);
    protected native void getVarchar(int countA, int countB,
                                     boolean batch, int length);
    protected native void setB0ToA(int countA, int countB,
                                   boolean batch);
    protected native void navB0ToA(int countA, int countB,
                                   boolean batch);
    protected native void navB0ToAalt(int countA, int countB,
                                      boolean batch);
    protected native void navAToB0(int countA, int countB,
                                   boolean batch);
    protected native void navAToB0alt(int countA, int countB,
                                      boolean batch);
    protected native void nullB0ToA(int countA, int countB,
                                    boolean batch);

    protected void initOperations() {
        out.print("initializing operations ...");
        out.flush();

        for (boolean f = false, done = false; !done; done = f, f = true) {
            // inner classes can only refer to a constant
            final boolean batch = f;
            final boolean forceSend = f;
            final boolean setAttrs = true;

            ops.add(
                new Op("insA" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        insA(countA, countB, !setAttrs, batch);
                    }
                });

            ops.add(
                new Op("insB0" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        insB0(countA, countB, !setAttrs, batch);
                    }
                });

            ops.add(
                new Op("setAByPK" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        setAByPK(countA, countB, batch);
                    }
                });

            ops.add(
                new Op("setB0ByPK" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        setB0ByPK(countA, countB, batch);
                    }
                });

            ops.add(
                new Op("getAByPK_bb" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        getAByPK_bb(countA, countB, batch);
                    }
                });

            ops.add(
                new Op("getAByPK_ar" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        getAByPK_ar(countA, countB, batch);
                    }
                });

            ops.add(
                new Op("getB0ByPK_bb" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        getB0ByPK_bb(countA, countB, batch);
                    }
                });

            ops.add(
                new Op("getB0ByPK_ar" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        getB0ByPK_ar(countA, countB, batch);
                    }
                });

            for (int i = 1; i <= maxVarbinaryBytes; i *= 10) {
                final int length = i;

                ops.add(
                    new Op("setVarbinary" + length + (batch ? "_batch" : "")) {
                        public void run(int countA, int countB) {
                            setVarbinary(countA, countB, batch, length);
                        }
                    });

                ops.add(
                    new Op("getVarbinary" + length + (batch ? "_batch" : "")) {
                        public void run(int countA, int countB) {
                            getVarbinary(countA, countB, batch, length);
                        }
                    });

                ops.add(
                    new Op("clearVarbinary" + length + (batch ? "_batch" : "")) {
                        public void run(int countA, int countB) {
                            setVarbinary(countA, countB, batch, 0);
                        }
                    });
            }

            for (int i = 1; i <= maxVarcharChars; i *= 10) {
                final int length = i;

                ops.add(
                    new Op("setVarchar" + length + (batch ? "_batch" : "")) {
                        public void run(int countA, int countB) {
                            setVarchar(countA, countB, batch, length);
                        }
                    });

                ops.add(
                    new Op("getVarchar" + length + (batch ? "_batch" : "")) {
                        public void run(int countA, int countB) {
                            getVarchar(countA, countB, batch, length);
                        }
                    });

                ops.add(
                    new Op("clearVarchar" + length + (batch ? "_batch" : "")) {
                        public void run(int countA, int countB) {
                            setVarchar(countA, countB, batch, 0);
                        }
                    });

            }

            ops.add(
                new Op("setB0->A" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        setB0ToA(countA, countB, batch);
                    }
                });

            ops.add(
                new Op("navB0->A" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        navB0ToA(countA, countB, batch);
                    }
                });

            ops.add(
                new Op("navB0->A_alt" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        navB0ToAalt(countA, countB, batch);
                    }
                });

            ops.add(
                new Op("navA->B0" + (forceSend ? "_forceSend" : "")) {
                    public void run(int countA, int countB) {
                        navAToB0(countA, countB, forceSend);
                    }
                });

            ops.add(
                new Op("navA->B0_alt" + (forceSend ? "_forceSend" : "")) {
                    public void run(int countA, int countB) {
                        navAToB0alt(countA, countB, forceSend);
                    }
                });

            ops.add(
                new Op("nullB0->A" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        nullB0ToA(countA, countB, batch);
                    }
                });

            ops.add(
                new Op("delB0ByPK" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        delB0ByPK(countA, countB, batch);
                    }
                });

            ops.add(
                new Op("delAByPK" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        delAByPK(countA, countB, batch);
                    }
                });

            ops.add(
                new Op("insA_attr" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        insA(countA, countB, setAttrs, batch);
                    }
                });

            ops.add(
                new Op("insB0_attr" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        insB0(countA, countB, setAttrs, batch);
                    }
                });

            ops.add(
                new Op("delAllB0" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        delAllB0(countA, countB, batch);
                    }
                });

            ops.add(
                new Op("delAllA" + (batch ? "_batch" : "")) {
                    public void run(int countA, int countB) {
                        delAllA(countA, countB, batch);
                    }
                });

        }

        out.println("     [Op: " + ops.size() + "]");
    }

    protected void closeOperations() {
        out.print("closing operations ...");
        out.flush();
        ops.clear();
        out.println("          [ok]");
    }

    // ----------------------------------------------------------------------
    // NDB API datastore operations
    // ----------------------------------------------------------------------

    protected native void initConnection(String catalog,
                                         String schema,
                                         int defaultLockMode);
    protected void initConnection() {
        // XXX add lockMode property to CrundDriver, switch then here
        final int LM_Read = 0;
        final int LM_Exclusive = 1;
        final int LM_CommittedRead = 2;
        initConnection(catalog, schema, LM_CommittedRead);
    }
    protected native void closeConnection();
    protected native void clearData();

    // ----------------------------------------------------------------------

    static public void main(String[] args) {
        System.out.println("NdbApiLoad.main()");
        parseArguments(args);
        new NdbApiLoad().run();
        System.out.println();
        System.out.println("NdbApiLoad.main(): done.");
    }
}
