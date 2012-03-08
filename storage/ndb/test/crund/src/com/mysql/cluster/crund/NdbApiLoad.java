/* -*- mode: java; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.
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

    protected void initLoad() throws Exception {
        // XXX support generic load class
        //super.init();

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

    protected void closeLoad() throws Exception {
        // release NDB resources
        final int ret = ndbclose();
        if (ret != 0) {
            String msg = ("NdbApiLoad: failed closing NDBAPI;"
                          + " return value = " + ret);
            err.println(msg);
            throw new Exception(msg);
        }

        // XXX support generic load class
        //super.close();
    }

    // ----------------------------------------------------------------------
    // NDB API operations
    // ----------------------------------------------------------------------

    protected native void delAllA(int nOps, boolean batch);
    protected native void delAllB0(int nOps, boolean batch);
    protected native void insA(int nOps, boolean setAttrs, boolean batch);
    protected native void insB0(int nOps, boolean setAttrs, boolean batch);
    protected native void delAByPK(int nOps, boolean batch);
    protected native void delB0ByPK(int nOps, boolean batch);
    protected native void setAByPK(int nOps, boolean batch);
    protected native void setB0ByPK(int nOps, boolean batch);
    protected native void getAByPK_bb(int nOps, boolean batch);
    protected native void getB0ByPK_bb(int nOps, boolean batch);
    protected native void getAByPK_ar(int nOps, boolean batch);
    protected native void getB0ByPK_ar(int nOps, boolean batch);
    protected native void setVarbinary(int nOps, boolean batch, int length);
    protected native void getVarbinary(int nOps, boolean batch, int length);
    protected native void setVarchar(int nOps, boolean batch, int length);
    protected native void getVarchar(int nOps, boolean batch, int length);
    protected native void setB0ToA(int nOps, boolean batch);
    protected native void navB0ToA(int nOps, boolean batch);
    protected native void navB0ToAalt(int nOps, boolean batch);
    protected native void navAToB0(int nOps, boolean batch);
    protected native void navAToB0alt(int nOps, boolean batch);
    protected native void nullB0ToA(int nOps, boolean batch);

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
                    public void run(int nOps) {
                        insA(nOps, !setAttrs, batch);
                    }
                });

            ops.add(
                new Op("insB0" + (batch ? "_batch" : "")) {
                    public void run(int nOps) {
                        insB0(nOps, !setAttrs, batch);
                    }
                });

            ops.add(
                new Op("setAByPK" + (batch ? "_batch" : "")) {
                    public void run(int nOps) {
                        setAByPK(nOps, batch);
                    }
                });

            ops.add(
                new Op("setB0ByPK" + (batch ? "_batch" : "")) {
                    public void run(int nOps) {
                        setB0ByPK(nOps, batch);
                    }
                });

            ops.add(
                new Op("getAByPK_bb" + (batch ? "_batch" : "")) {
                    public void run(int nOps) {
                        getAByPK_bb(nOps, batch);
                    }
                });

            ops.add(
                new Op("getAByPK_ar" + (batch ? "_batch" : "")) {
                    public void run(int nOps) {
                        getAByPK_ar(nOps, batch);
                    }
                });

            ops.add(
                new Op("getB0ByPK_bb" + (batch ? "_batch" : "")) {
                    public void run(int nOps) {
                        getB0ByPK_bb(nOps, batch);
                    }
                });

            ops.add(
                new Op("getB0ByPK_ar" + (batch ? "_batch" : "")) {
                    public void run(int nOps) {
                        getB0ByPK_ar(nOps, batch);
                    }
                });

            for (int i = 1; i <= maxVarbinaryBytes; i *= 10) {
                final int length = i;

                ops.add(
                    new Op("setVarbinary" + length + (batch ? "_batch" : "")) {
                        public void run(int nOps) {
                            setVarbinary(nOps, batch, length);
                        }
                    });

                ops.add(
                    new Op("getVarbinary" + length + (batch ? "_batch" : "")) {
                        public void run(int nOps) {
                            getVarbinary(nOps, batch, length);
                        }
                    });

                ops.add(
                    new Op("clearVarbinary" + length + (batch ? "_batch" : "")) {
                        public void run(int nOps) {
                            setVarbinary(nOps, batch, 0);
                        }
                    });
            }

            for (int i = 1; i <= maxVarcharChars; i *= 10) {
                final int length = i;

                ops.add(
                    new Op("setVarchar" + length + (batch ? "_batch" : "")) {
                        public void run(int nOps) {
                            setVarchar(nOps, batch, length);
                        }
                    });

                ops.add(
                    new Op("getVarchar" + length + (batch ? "_batch" : "")) {
                        public void run(int nOps) {
                            getVarchar(nOps, batch, length);
                        }
                    });

                ops.add(
                    new Op("clearVarchar" + length + (batch ? "_batch" : "")) {
                        public void run(int nOps) {
                            setVarchar(nOps, batch, 0);
                        }
                    });

            }

            ops.add(
                new Op("setB0->A" + (batch ? "_batch" : "")) {
                    public void run(int nOps) {
                        setB0ToA(nOps, batch);
                    }
                });

            ops.add(
                new Op("navB0->A" + (batch ? "_batch" : "")) {
                    public void run(int nOps) {
                        navB0ToA(nOps, batch);
                    }
                });

            ops.add(
                new Op("navB0->A_alt" + (batch ? "_batch" : "")) {
                    public void run(int nOps) {
                        navB0ToAalt(nOps, batch);
                    }
                });

            ops.add(
                new Op("navA->B0" + (forceSend ? "_forceSend" : "")) {
                    public void run(int nOps) {
                        navAToB0(nOps, forceSend);
                    }
                });

            ops.add(
                new Op("navA->B0_alt" + (forceSend ? "_forceSend" : "")) {
                    public void run(int nOps) {
                        navAToB0alt(nOps, forceSend);
                    }
                });

            ops.add(
                new Op("nullB0->A" + (batch ? "_batch" : "")) {
                    public void run(int nOps) {
                        nullB0ToA(nOps, batch);
                    }
                });

            ops.add(
                new Op("delB0ByPK" + (batch ? "_batch" : "")) {
                    public void run(int nOps) {
                        delB0ByPK(nOps, batch);
                    }
                });

            ops.add(
                new Op("delAByPK" + (batch ? "_batch" : "")) {
                    public void run(int nOps) {
                        delAByPK(nOps, batch);
                    }
                });

            ops.add(
                new Op("insA_attr" + (batch ? "_batch" : "")) {
                    public void run(int nOps) {
                        insA(nOps, setAttrs, batch);
                    }
                });

            ops.add(
                new Op("insB0_attr" + (batch ? "_batch" : "")) {
                    public void run(int nOps) {
                        insB0(nOps, setAttrs, batch);
                    }
                });

            ops.add(
                new Op("delAllB0" + (batch ? "_batch" : "")) {
                    public void run(int nOps) {
                        delAllB0(nOps, batch);
                    }
                });

            ops.add(
                new Op("delAllA" + (batch ? "_batch" : "")) {
                    public void run(int nOps) {
                        delAllA(nOps, batch);
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
