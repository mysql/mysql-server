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

import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.Constants;
import com.mysql.clusterj.Query;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.SessionFactory;
import com.mysql.clusterj.query.QueryDomainType;
import com.mysql.clusterj.query.QueryBuilder;

import java.util.Collection;
import java.util.Iterator;
import java.util.Arrays;
import java.util.Map;

/**
 * A benchmark implementation against a ClusterJ database.
 */
public class ClusterjLoad extends CrundDriver {

    // ClusterJ settings
    protected String mgmdConnect;

    // ClusterJ resources
    protected SessionFactory sessionFactory;
    protected Session session;

    // ----------------------------------------------------------------------
    // ClusterJ intializers/finalizers
    // ----------------------------------------------------------------------

    @Override
    protected void initProperties() {
        super.initProperties();

        out.print("setting clusterj properties ...");

        final StringBuilder msg = new StringBuilder();

        // check required properties
        mgmdConnect
            = props.getProperty(Constants.PROPERTY_CLUSTER_CONNECTSTRING);

        if (msg.length() == 0) {
            out.println(" [ok]");
        } else {
            out.println();
            out.print(msg.toString());
        }

        // have mgmdConnect initialized first
        descr = "->clusterj(" + mgmdConnect + ")";
    }

    @Override
    protected void printProperties() {
        super.printProperties();

        out.println();
        out.println("clusterj settings ...");
        out.println("ndb.mgmdConnect                 " + mgmdConnect);
        for (Iterator<Map.Entry<Object,Object>> i
                 = props.entrySet().iterator(); i.hasNext();) {
            Map.Entry<Object,Object> e = i.next();
            final String k = (String)e.getKey();
            if (k.startsWith("com.mysql.clusterj")) {
                final StringBuilder s = new StringBuilder("..");
                s.append(k, 18, k.length());
                while (s.length() < 31) s.append(' ');
                out.println(s + " " + e.getValue());
            }
        }
    }

    @Override
    protected void initLoad() throws Exception {
        // XXX support generic load class
        //super.init();

        out.println();
        out.print("creating SessionFactory ...");
        out.flush();
        sessionFactory = ClusterJHelper.getSessionFactory(props);
        out.println("     [SessionFactory: 1]");
    }

    @Override
    protected void closeLoad() throws Exception {
        out.println();
        out.print("closing SessionFactory ...");
        out.flush();
        if (sessionFactory != null)
            sessionFactory.close();
        sessionFactory = null;
        out.println("      [ok]");

        // XXX support generic load class
        //super.close();
    }

    // ----------------------------------------------------------------------
    // ClusterJ operations
    // ----------------------------------------------------------------------

    protected abstract class ClusterjOp extends Op {
        public ClusterjOp(String name) {
            super(name);
        }

        public void init() {}

        public void close() {}
    };

    protected int checkFields(IA o) {
        final int cint = o.getCint();
        final long clong = o.getClong();
        verify(clong == cint);
        final float cfloat = o.getCfloat();
        verify(cfloat == cint);
        final double cdouble = o.getCdouble();
        verify(cdouble == cint);
        return cint;
    }

    protected int checkFields(IB0 o) {
        final int cint = o.getCint();
        final long clong = o.getClong();
        verify(clong == cint);
        final float cfloat = o.getCfloat();
        verify(cfloat == cint);
        final double cdouble = o.getCdouble();
        verify(cdouble == cint);
        return cint;
    }

    protected void initOperations() {
        out.print("initializing operations ...");
        out.flush();

        for (CrundDriver.XMode m : xMode) {
            // inner classes can only refer to a constant
            final CrundDriver.XMode mode = m;

        ops.add(
            new ClusterjOp("insA_" + mode.toString().toLowerCase()) {
                public void run(int nOps) {
                    if (mode != CrundDriver.XMode.INDY) {
                        beginTransaction();
                    }
                    for (int i = 0; i < nOps; i++) {
                        final IA o = session.newInstance(IA.class);
                        assert o != null;
                        o.setId(i);
                        session.persist(o);
                        if (mode == CrundDriver.XMode.EACH) {
                            session.flush();
                        }
                    }
                    if (mode != CrundDriver.XMode.INDY) {
                        commitTransaction();
                    }
                }
            });

        ops.add(
            new ClusterjOp("insB0_" + mode.toString().toLowerCase()) {
                public void run(int nOps) {
                    if (mode != CrundDriver.XMode.INDY) {
                        beginTransaction();
                    }
                    for (int i = 0; i < nOps; i++) {
                        final IB0 o = session.newInstance(IB0.class);
                        assert o != null;
                        o.setId(i);
                        o.setCvarbinary_def(null);
                        session.persist(o);
                        if (mode == CrundDriver.XMode.EACH) {
                            session.flush();
                        }
                    }
                    if (mode != CrundDriver.XMode.INDY) {
                        commitTransaction();
                    }
                }
            });

        ops.add(
            new ClusterjOp("setAByPK_" + mode.toString().toLowerCase()) {
                public void run(int nOps) {
                    if (mode != CrundDriver.XMode.INDY) {
                        beginTransaction();
                    }
                   for (int i = 0; i < nOps; i++) {
                        // blind update
                        final IA o = session.newInstance(IA.class);
                        o.setId(i);
                        assert o != null;
                        o.setCint(i);
                        o.setClong((long)i);
                        o.setCfloat((float)i);
                        o.setCdouble((double)i);
                        session.updatePersistent(o);
                        if (mode == CrundDriver.XMode.EACH) {
                            session.flush();
                        }
                    }
                   if (mode != CrundDriver.XMode.INDY) {
                       commitTransaction();
                   }
                }
            });

        ops.add(
            new ClusterjOp("setB0ByPK_" + mode.toString().toLowerCase()) {
                public void run(int nOps) {
                    if (mode != CrundDriver.XMode.INDY) {
                        beginTransaction();
                    }
                    for (int i = 0; i < nOps; i++) {
                        // blind update
                        final IB0 o = session.newInstance(IB0.class);
                        o.setId(i);
                        assert o != null;
                        o.setCint(i);
                        o.setClong((long)i);
                        o.setCfloat((float)i);
                        o.setCdouble((double)i);
                        session.updatePersistent(o);
                        if (mode == CrundDriver.XMode.EACH) {
                            session.flush();
                        }
                    }
                    if (mode != CrundDriver.XMode.INDY) {
                        commitTransaction();
                    }
                }
            });

        ops.add(
            new ClusterjOp("getAByPK_" + mode.toString().toLowerCase()) {
                public void run(int nOps) {
                    if (mode != CrundDriver.XMode.INDY) {
                        beginTransaction();
                    }
                    if (mode != CrundDriver.XMode.BULK) {
                        for (int i = 0; i < nOps; i++) {
                            final IA o = session.find(IA.class, i);
                            assert o != null;
                            final int id = o.getId();
                            verify(id == i);
                            final int j = checkFields(o);
                            verify(j == id);
                        }
                    } else {
                        IA[] objs = new IA[nOps];
                        for (int i = 0; i < nOps; ++i) {
                            final IA o = session.newInstance(IA.class, i);
                            objs[i] =o;
                        }
                        session.load(objs);
                        session.flush();
                        for (int i = 0; i < nOps; ++i) {
                            IA o = objs[i];
                            final int id = o.getId();
                            verify(id == i);
                            final int j = checkFields(o);
                            verify (j == id);
                        }
                    }
                    if (mode != CrundDriver.XMode.INDY) {
                        commitTransaction();
                    }
                }
            });

        ops.add(
            new ClusterjOp("getB0ByPK_" + mode.toString().toLowerCase()) {
                public void run(int nOps) {
                    if (mode != CrundDriver.XMode.INDY) {
                        beginTransaction();
                    }
                    if (mode != CrundDriver.XMode.BULK) {
                        for (int i = 0; i < nOps; i++) {
                            final IB0 o = session.find(IB0.class, i);
                            assert o != null;
                            final int id = o.getId();
                            verify(id == i);
                            final int j = checkFields(o);
                            verify(j == id);
                        }
                    } else {
                        IB0[] objs = new IB0[nOps];
                        for (int i = 0; i < nOps; ++i) {
                            final IB0 o = session.newInstance(IB0.class, i);
                            objs[i] =o;
                        }
                        session.load(objs);
                        session.flush();
                        for (int i = 0; i < nOps; ++i) {
                            IB0 o = objs[i];
                            final int id = o.getId();
                            verify(id == i);
                            final int j = checkFields(o);
                            verify (j == id);
                        }
                    }
                    if (mode != CrundDriver.XMode.INDY) {
                        commitTransaction();
                    }
                }
            });

        for (int i = 0, l = 1; l <= maxVarbinaryBytes; l *= 10, i++) {
            final byte[] b = bytes[i];
            assert l == b.length;

            ops.add(
                new ClusterjOp("setVarbinary" + l + "_" + mode.toString().toLowerCase()) {
                    public void run(int nOps) {
                        if (mode != CrundDriver.XMode.INDY) {
                            beginTransaction();
                        }
                        for (int i = 0; i < nOps; i++) {
                            // blind update
                            final IB0 o = session.newInstance(IB0.class);
                            o.setId(i);
                            assert o != null;
                            o.setCvarbinary_def(b);
                            session.updatePersistent(o);
                            if (mode == CrundDriver.XMode.EACH) {
                                session.flush();
                            }
                        }
                        if (mode != CrundDriver.XMode.INDY) {
                            commitTransaction();
                        }
                    }
                });

            ops.add(
                new ClusterjOp("getVarbinary" + l + "_" + mode.toString().toLowerCase()) {

                    // TODO implement BULK using session.load
                    public void run(int nOps) {
                        if (mode != CrundDriver.XMode.INDY) {
                            beginTransaction();
                        }
                        for (int i = 0; i < nOps; i++) {
                            final IB0 o = session.find(IB0.class, i);
                            assert o != null;
                            verify(Arrays.equals(b, o.getCvarbinary_def()));
                        }
                        if (mode != CrundDriver.XMode.INDY) {
                            commitTransaction();
                        }
                   }
                });

            ops.add(
                new ClusterjOp("clearVarbinary" + l + "_" + mode.toString().toLowerCase()) {
                    public void run(int nOps) {
                        if (mode != CrundDriver.XMode.INDY) {
                            beginTransaction();
                        }
                        for (int i = 0; i < nOps; i++) {
                            // blind update
                            final IB0 o = session.newInstance(IB0.class);
                            o.setId(i);
                            assert o != null;
                            o.setCvarbinary_def(null);
                            session.updatePersistent(o);
                            if (mode == CrundDriver.XMode.EACH) {
                                session.flush();
                            }
                        }
                        if (mode != CrundDriver.XMode.INDY) {
                            commitTransaction();
                        }
                    }
                });
        }

        for (int i = 0, l = 1; l <= maxVarcharChars; l *= 10, i++) {
            final String s = strings[i];
            assert l == s.length();

            ops.add(
                new ClusterjOp("setVarchar" + l + "_" + mode.toString().toLowerCase()) {
                    public void run(int nOps) {
                        if (mode != CrundDriver.XMode.INDY) {
                            beginTransaction();
                        }
                        for (int i = 0; i < nOps; i++) {
                            // blind update
                            final IB0 o = session.newInstance(IB0.class);
                            o.setId(i);
                            assert o != null;
                            o.setCvarchar_def(s);
                            session.updatePersistent(o);
                            if (mode == CrundDriver.XMode.EACH) {
                                session.flush();
                            }
                        }
                        if (mode != CrundDriver.XMode.INDY) {
                            commitTransaction();
                        }
                    }
                });

            ops.add(
                new ClusterjOp("getVarchar" + l + "_" + mode.toString().toLowerCase()) {

                    // TODO implement BULK using session.load
                    public void run(int nOps) {
                        if (mode != CrundDriver.XMode.INDY) {
                            beginTransaction();
                        }
                        for (int i = 0; i < nOps; i++) {
                            final IB0 o = session.find(IB0.class, i);
                            assert o != null;
                            verify(s.equals(o.getCvarchar_def()));
                        }
                        if (mode != CrundDriver.XMode.INDY) {
                            commitTransaction();
                        }
                    }
                });

            ops.add(
                new ClusterjOp("clearVarchar" + l + "_" + mode.toString().toLowerCase()) {
                    public void run(int nOps) {
                        if (mode != CrundDriver.XMode.INDY) {
                            beginTransaction();
                        }
                        for (int i = 0; i < nOps; i++) {
                            // blind update
                            final IB0 o = session.newInstance(IB0.class);
                            o.setId(i);
                            assert o != null;
                            o.setCvarchar_def(null);
                            session.updatePersistent(o);
                            if (mode == CrundDriver.XMode.EACH) {
                                session.flush();
                            }
                        }
                        if (mode != CrundDriver.XMode.INDY) {
                            commitTransaction();
                        }
                    }
                });
        }

        ops.add(
            new ClusterjOp("setB0->A_" + mode.toString().toLowerCase()) {
                public void run(int nOps) {
                    if (mode != CrundDriver.XMode.INDY) {
                        beginTransaction();
                    }
                    for (int i = 0; i < nOps; i++) {
                        // blind update
                        final IB0 b0 = session.newInstance(IB0.class);
                        b0.setId(i);
                        assert b0 != null;
                        final int aId = i % nOps;
                        b0.setAid(aId);
                        session.updatePersistent(b0);
                        if (mode == CrundDriver.XMode.EACH) {
                            session.flush();
                        }
                    }
                    if (mode != CrundDriver.XMode.INDY) {
                        commitTransaction();
                    }
                }
            });

        ops.add(
            new ClusterjOp("navB0->A") {
                public void run(int nOps) {
                    if (mode != CrundDriver.XMode.INDY) {
                        beginTransaction();
                    }
                    for (int i = 0; i < nOps; i++) {
                        final IB0 b0 = session.find(IB0.class, i);
                        assert b0 != null;
                        int aid = b0.getAid();
                        final IA a = session.find(IA.class, aid);
                        assert a != null;
                        final int id = a.getId();
                        verify(id == i % nOps);
                        final int j = checkFields(a);
                        verify(j == id);
                    }
                    if (mode != CrundDriver.XMode.INDY) {
                        commitTransaction();
                    }
                }
            });

        ops.add(
            new ClusterjOp("navA->B0") {
                protected Query<IB0> query;

                public void init() {
                    super.init();
                }

                public void close() {
                    // XXX there's no close() on query, dobj, builder?
                    super.close();
                }

                public void run(int nOps) {
                    if (mode != CrundDriver.XMode.INDY) {
                        beginTransaction();
                    }
                    // QueryBuilder is the sessionFactory for queries
                    final QueryBuilder builder
                        = session.getQueryBuilder();
                    // QueryDomainType is the main interface
                    final QueryDomainType<IB0> dobj
                        = builder.createQueryDefinition(IB0.class);
                    // filter by aid
                    dobj.where(dobj.get("aid").equal(dobj.param("aid")));
                    query = session.createQuery(dobj);
                    for (int i = 0; i < nOps; i++) {
                        // find B0s by query
                        query.setParameter("aid", i);
                        // fetch results
                        Collection<IB0> b0s = query.getResultList();
                        assert b0s != null;

                        // check query results
                        verify(b0s.size() > 0);
                        for (IB0 b0 : b0s) {
                            assert b0 != null;
                            final int id = b0.getId();
                            verify(id % nOps == i);
                            final int j = checkFields(b0);
                            verify(j == id);
                        }
                    }
                    if (mode != CrundDriver.XMode.INDY) {
                        commitTransaction();
                    }
                }
            });

        ops.add(
            new ClusterjOp("nullB0->A") {
                public void run(int nOps) {
                    if (mode != CrundDriver.XMode.INDY) {
                        beginTransaction();
                    }
                    for (int i = 0; i < nOps; i++) {
                        // blind update
                        final IB0 b0 = session.newInstance(IB0.class);
                        b0.setId(i);
                        assert b0 != null;
                        b0.setAid(0);
                        session.updatePersistent(b0);
                        if (mode == CrundDriver.XMode.EACH) {
                            session.flush();
                        }
                    }
                    if (mode != CrundDriver.XMode.INDY) {
                        commitTransaction();
                    }
                }
            });

        ops.add(
            new ClusterjOp("delB0ByPK_" + mode.toString().toLowerCase()) {
                public void run(int nOps) {
                    if (mode != CrundDriver.XMode.INDY) {
                        beginTransaction();
                    }
                    for (int i = 0; i < nOps; i++) {
                        // blind delete
                        final IB0 o = session.newInstance(IB0.class);
                        assert o != null;
                        o.setId(i);
                        session.remove(o);
                        if (mode == CrundDriver.XMode.EACH) {
                            session.flush();
                        }
                    }
                    if (mode != CrundDriver.XMode.INDY) {
                        commitTransaction();
                    }
                }
            });

        ops.add(
            new ClusterjOp("delAByPK_" + mode.toString().toLowerCase()) {
                public void run(int nOps) {
                    if (mode != CrundDriver.XMode.INDY) {
                        beginTransaction();
                    }
                    for (int i = 0; i < nOps; i++) {
                        // blind delete
                        final IA o = session.newInstance(IA.class);
                        assert o != null;
                        o.setId(i);
                        session.remove(o);
                        if (mode == CrundDriver.XMode.EACH) {
                            session.flush();
                        }
                    }
                    if (mode != CrundDriver.XMode.INDY) {
                        commitTransaction();
                    }
                }
            });

        ops.add(
            new ClusterjOp("insAattr_" + mode.toString().toLowerCase()) {
                public void run(int nOps) {
                    if (mode != CrundDriver.XMode.INDY) {
                        beginTransaction();
                    }
                    for (int i = 0; i < nOps; i++) {
                        final IA o = session.newInstance(IA.class);
                        assert o != null;
                        o.setId(i);
                        o.setCint(-i);
                        o.setClong((long)-i);
                        o.setCfloat((float)-i);
                        o.setCdouble((double)-i);
                        session.persist(o);
                        if (mode == CrundDriver.XMode.EACH) {
                            session.flush();
                        }
                    }
                    if (mode != CrundDriver.XMode.INDY) {
                        commitTransaction();
                    }
                }
            });

        ops.add(
            new ClusterjOp("insB0attr_" + mode.toString().toLowerCase()) {
                public void run(int nOps) {
                    if (mode != CrundDriver.XMode.INDY) {
                        beginTransaction();
                    }
                    for (int i = 0; i < nOps; i++) {
                        final IB0 o = session.newInstance(IB0.class);
                        assert o != null;
                        o.setId(i);
                        o.setCint(-i);
                        o.setClong((long)-i);
                        o.setCfloat((float)-i);
                        o.setCdouble((double)-i);
                        o.setCvarbinary_def(null);
                        session.persist(o);
                        if (mode == CrundDriver.XMode.EACH) {
                            session.flush();
                        }
                    }
                    if (mode != CrundDriver.XMode.INDY) {
                        commitTransaction();
                    }
                }
            });

        ops.add(
            new ClusterjOp("delAllB0_" + mode.toString().toLowerCase()) {
                public void run(int nOps) {
                    if (mode != CrundDriver.XMode.INDY) {
                        beginTransaction();
                    }
                    int del = session.deletePersistentAll(IB0.class);
                    assert del == nOps;
                    if (mode != CrundDriver.XMode.INDY) {
                        commitTransaction();
                    }
                }
            });

        ops.add(
            new ClusterjOp("delAllA_" + mode.toString().toLowerCase()) {
                public void run(int nOps) {
                    if (mode != CrundDriver.XMode.INDY) {
                        beginTransaction();
                    }
                    int del = session.deletePersistentAll(IA.class);
                    assert del == nOps;
                    if (mode != CrundDriver.XMode.INDY) {
                        commitTransaction();
                    }
                }
            });

        }
        // prepare queries
        for (Iterator<CrundDriver.Op> i = ops.iterator(); i.hasNext();) {
            ((ClusterjOp)i.next()).init();
        }

        out.println("     [ClusterjOp: " + ops.size() + "]");
    }

    protected void closeOperations() {
        out.print("closing operations ...");
        out.flush();

        // close all queries
        for (Iterator<CrundDriver.Op> i = ops.iterator(); i.hasNext();) {
            ((ClusterjOp)i.next()).close();
        }
        ops.clear();

        out.println("          [ok]");
    }

    protected void beginTransaction() {
        session.currentTransaction().begin();
    }

    protected void commitTransaction() {
        session.currentTransaction().commit();
    }

    // ----------------------------------------------------------------------
    // ClusterJ datastore operations
    // ----------------------------------------------------------------------

    protected void initConnection() {
        out.println();
        out.print("creating ClusterJ Session ...");
        out.flush();
        session = sessionFactory.getSession();
        out.println("   [Session: 1]");
    }

    protected void closeConnection() {
        out.println();
        out.print("closing ClusterJ Session ...");
        out.flush();
        if (session != null)
            session.close();
        session = null;
        out.println("    [ok]");
    }

    protected void clearPersistenceContext() {
        // nothing to do as long as we're not caching beyond Tx scope
    }

    protected void clearData() {
        out.print("deleting all objects ...");
        out.flush();

        session.currentTransaction().begin();
        int delB0 = session.deletePersistentAll(IB0.class);
        out.print("        [B0: " + delB0);
        out.flush();
        int delA = session.deletePersistentAll(IA.class);
        out.print(", A: " + delA);
        out.flush();
        session.currentTransaction().commit();

        out.println("]");
    }

    // ----------------------------------------------------------------------

    static public void main(String[] args) {
        System.out.println("ClusterjLoad.main()");
        parseArguments(args);
        new ClusterjLoad().run();
        System.out.println();
        System.out.println("ClusterjLoad.main(): done.");
    }
}
