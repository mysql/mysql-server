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

import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.Constants;
import com.mysql.clusterj.Query;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.SessionFactory;
import com.mysql.clusterj.query.QueryDomainType;
import com.mysql.clusterj.query.QueryBuilder;
import com.mysql.clusterj.query.Predicate;

import java.util.Collection;
import java.util.Iterator;
import java.util.Map;
import java.util.List;
import java.util.ArrayList;

/**
 * The ClusterJ benchmark implementation.
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

        beginTransaction();
        final int delB = session.deletePersistentAll(IB.class);
        out.print("        [B: " + delB);
        out.flush();
        final int delA = session.deletePersistentAll(IA.class);
        out.print(", A: " + delA);
        out.flush();
        commitTransaction();

        out.println("]");
    }

    // ----------------------------------------------------------------------
    // ClusterJ operations
    // ----------------------------------------------------------------------

    // assumes PKs: 0..nOps, relationships: identity 1:1
    protected abstract class ClusterjOp extends Op {
        protected XMode xMode;

        public ClusterjOp(String name, XMode m) {
            super(name + (m == null ? "" : toStr(m)));
            this.xMode = m;
        }

        public void init() {}

        public void close() {}
    };

    protected abstract class WriteOp extends ClusterjOp {
        public WriteOp(String name, XMode m) {
            super(name, m);
        }

        public void run(int nOps) {
            switch (xMode) {
            case INDY :
                for (int id = 0; id < nOps; id++) {
                    beginTransaction();
                    write(id);
                    commitTransaction();
                }
                break;
            case EACH :
            case BULK :
                // Approach: control when persistent context is flushed,
                // i.e., at commit for 1 database roundtrip only.
                beginTransaction();
                for (int id = 0; id < nOps; id++) {
                    write(id);
                    if (xMode == XMode.EACH)
                        session.flush();

                }
                commitTransaction();
                break;
            }
        }

        protected abstract void write(int id);
    }

    protected abstract class ReadOp extends ClusterjOp {
        public ReadOp(String name, XMode m) {
            super(name, m);
        }

        public void run(int nOps) {
            switch (xMode) {
            case INDY :
                for (int id = 0; id < nOps; id++) {
                    beginTransaction();
                    read(id);
                    commitTransaction();
                }
                break;
            case EACH :
                beginTransaction();
                for (int id = 0; id < nOps; id++)
                    read(id);
                commitTransaction();
                break;
            case BULK :
                beginTransaction();
                final int[] id = new int[nOps];
                for (int i = 0; i < nOps; i++)
                    id[i] = i;
                read(id);
                commitTransaction();
                break;
            }
        }

        protected abstract void read(int id);

        protected abstract void read(int[] id);
    }

    protected abstract class FetchOp<I> extends ReadOp {
        protected final Class<I> cls;

        public FetchOp(String name, XMode m, Class<I> cls) {
            super(name, m);
            this.cls = cls;
        }

        protected void read(int id) {
            final I o = session.find(cls, id);
            assert o != null;
            check(id, o);
        }

        protected void read(int[] id) {
            final int n = id.length;
            final List<I> os = new ArrayList<I>(n);
            for (int i = 0; i < n; i++)
                os.add(session.newInstance(cls, id[i]));
            session.load(os);
            session.flush(); // needed
            for (int i = 0; i < n; i++) {
                final I o = os.get(i);
                assert o != null;
                check(id[i], o);
            }
        }

        protected abstract void check(int id, I o);
    }

    // assumes query with parametrized where-in clause for bulk reads
    protected abstract class QReadOp<I> extends ReadOp {
        protected QueryBuilder qbld;
        protected QueryDomainType<I> dobj;
        protected Query<I> q;

        public QReadOp(String name, XMode m) {
            super(name, m);
        }

        public void init() {
            super.init();
            qbld = session.getQueryBuilder();
            setDOQuery();
            q = session.createQuery(dobj);
        }

        public void close() {
            // query, domain object, builder not Closable
            super.close();
        }

        protected void read(int id) {
            int[] ids = new int[1];
            ids[0] = id;
            read(ids);
        }

        protected void read(int[] id) {
            final int n = id.length;
            final List<Integer> p = new ArrayList<Integer>(n);
            for (int i = 0; i < n; i++)
                p.add(id[i]);
            setParams(p);

            final List<I> os = q.getResultList();
            assert os != null;
            verify(n, os.size());
            for (int i = 0; i < n; i++) {
                final I o = os.get(i);
                assert o != null;
                check(id[i], o);
            }
        }

        protected abstract void setDOQuery();

        protected abstract void setParams(List<Integer> p);

        protected abstract void check(int id, I o);
    }

    protected void initOperations() {
        out.print("initializing operations ...");
        out.flush();

        for (XMode m : xMode) {
            // inner classes can only refer to a constant
            final XMode xMode = m;
            final boolean setAttrs = true;

            ops.add(
                new WriteOp("A_insAttr_", xMode) {
                    protected void write(int id) {
                        final IA o = session.newInstance(IA.class);
                        o.setId(id);
                        setAttr(o, -id);
                        session.persist(o);
                    }
                });
            
            ops.add(
                new WriteOp("B_insAttr_", xMode) {
                    protected void write(int id) {
                        final IB o = session.newInstance(IB.class);
                        o.setId(id);
                        setAttr(o, -id);
                        session.persist(o);
                    }
                });
            
            ops.add(
                new WriteOp("A_setAttr_", xMode) {
                    protected void write(int id) {
                        final IA o = session.newInstance(IA.class);
                        assert o != null;
                        o.setId(id);
                        setAttr(o, id);
                        session.updatePersistent(o);
                    }
                });

            ops.add(
                new WriteOp("B_setAttr_", xMode) {
                    protected void write(int id) {
                        // blind update
                        final IB o = session.newInstance(IB.class);
                        assert o != null;
                        o.setId(id);
                        setAttr(o, id);
                        session.updatePersistent(o);
                    }
                });

            ops.add(
                new FetchOp<IA>("A_getAttr_", xMode, IA.class) {
                    protected void check(int id, IA o) {
                        assert o != null;
                        verify(id, o.getId());
                        verifyAttr(id, o);
                    }
                });

            ops.add(
                new FetchOp<IB>("B_getAttr_", xMode, IB.class) {
                    protected void check(int id, IB o) {
                        assert o != null;
                        verify(id, o.getId());
                        verifyAttr(id, o);
                    }
                });

            ops.add(
                new QReadOp<IA>("A_getAttr_wherein_", xMode) {
                    protected void setDOQuery() {
                        dobj = qbld.createQueryDefinition(IA.class);
                        dobj.where(dobj.get("id").in(dobj.param("id")));
                    }

                    protected void setParams(List<Integer> p) {
                        q.setParameter("id", p);
                    }

                    protected void check(int id, IA o) {
                        assert o != null;
                        verify(id, o.getId());
                        verifyAttr(id, o);
                    }
                });

            ops.add(
                new QReadOp<IB>("B_getAttr_wherein_", xMode) {
                    protected void setDOQuery() {
                        dobj = qbld.createQueryDefinition(IB.class);
                        dobj.where(dobj.get("id").in(dobj.param("id")));
                    }

                    protected void setParams(List<Integer> p) {
                        q.setParameter("id", p);
                    }

                    protected void check(int id, IB o) {
                        assert o != null;
                        verify(id, o.getId());
                        verifyAttr(id, o);
                    }
                });

            for (int i = 0; i < bytes.length; i++) {
                // inner classes can only refer to a constant
                final byte[] b = bytes[i];
                final int l = b.length;
                if (l > maxVarbinaryBytes)
                    break;

                ops.add(
                    new WriteOp("B_setVarbin_" + l + "_", xMode) {
                        protected void write(int id) {
                            // blind update
                            final IB o = session.newInstance(IB.class);
                            o.setId(id);
                            o.setCvarbinary_def(b);
                            session.updatePersistent(o);
                        }
                    });

                ops.add(
                    new FetchOp<IB>("B_getVarbin_" + l + "_",
                                    xMode, IB.class) {
                        protected void check(int id, IB o) {
                            assert o != null;
                            verify(b, o.getCvarbinary_def());
                        }
                    });

                ops.add(
                    new WriteOp("B_clearVarbin_" + l + "_", xMode) {
                        protected void write(int id) {
                            // blind update
                            final IB o = session.newInstance(IB.class);
                            o.setId(id);
                            o.setCvarbinary_def(null);
                            session.updatePersistent(o);
                        }
                    });
            }

            for (int i = 0; i < strings.length; i++) {
                // inner classes can only refer to a constant
                final String s = strings[i];
                final int l = s.length();
                if (l > maxVarcharChars)
                    break;

                ops.add(
                    new WriteOp("B_setVarchar_" + l + "_", xMode) {
                        protected void write(int id) {
                            // blind update
                            final IB o = session.newInstance(IB.class);
                            o.setId(id);
                            o.setCvarchar_def(s);
                            session.updatePersistent(o);
                        }
                    });

                ops.add(
                    new FetchOp<IB>("B_getVarchar_" + l + "_",
                                    xMode, IB.class) {
                        protected void check(int id, IB o) {
                            assert o != null;
                            verify(s, o.getCvarchar_def());
                        }
                    });

                ops.add(
                    new WriteOp("B_clearVarchar_" + l + "_", xMode) {
                        protected void write(int id) {
                            // blind update
                            final IB o = session.newInstance(IB.class);
                            o.setId(id);
                            o.setCvarchar_def(null);
                            session.updatePersistent(o);
                        }
                    });
            }

            ops.add(
                new WriteOp("B_setA_", xMode) {
                    protected void write(int id) {
                        // blind update
                        final IB o = session.newInstance(IB.class);
                        o.setId(id);
                        o.setAid(id);
                        session.updatePersistent(o);
                    }
                });

            ops.add(
                new ReadOp("B_getA_", xMode) {
                    protected void read(int id) {
                        final IB b = session.find(IB.class, id);
                        assert b != null;
                        
                        int aId = b.getAid();
                        final IA a = session.find(IA.class, aId);
                        assert a != null;
                        verify(id, a.getId());
                        verifyAttr(id, a);
                    }

                    protected void read(int[] id) {
                        final Session s = session; // shortcut
                        final int n = id.length;
                        final IB[] bs = new IB[n];
                        for (int i = 0; i < n; i++)
                            bs[i] = s.newInstance(IB.class, id[i]);
                        session.load(bs);
                        session.flush(); // needed

                        final IA[] as = new IA[n];
                        for (int i = 0; i < n; i++)
                            as[i] = s.newInstance(IA.class, bs[i].getAid());
                        session.load(as);
                        session.flush(); // needed

                        for (int i = 0; i < n; i++) {
                            final IA a = as[i];
                            assert a != null;
                            verify(id[i], a.getId());
                            verifyAttr(id[i], a);
                        }
                    }
                });

            ops.add(
                new QReadOp<IA>("B_getA_wherein_", xMode) {
                    protected QueryDomainType<IB> dobjB;

                    public void init() {
                        // XXX skip this operation, requires ClusterJ support
                        // for equal-comparison on properties
                        name = null;
                    }

                    protected void setDOQuery() {
                        dobj = qbld.createQueryDefinition(IA.class);
                        dobjB = qbld.createQueryDefinition(IB.class);
                        // XXX equal-comparison of properties not implemented
                        // [java] com.mysql.clusterj.ClusterJUserException: Operation equal is implemented only for parameters.
                        // [java] at com.mysql.clusterj.core.query.PropertyImpl.equal(PropertyImpl.java:88)
                        Predicate p0 = dobj.get("id").equal(dobjB.get("aid"));
                        Predicate p1 = dobjB.get("id").in(dobjB.param("id"));
                        dobj.where(p0.and(p1));
                    }

                    protected void setParams(List<Integer> p) {
                        q.setParameter("id", p);
                    }

                    protected void check(int id, IA o) {
                        assert o != null;
                        verify(id, o.getId());
                        verifyAttr(id, o);
                    }
                });

            ops.add(
                new QReadOp<IB>("A_getBs_wherein_", xMode) {
                    protected void setDOQuery() {
                        dobj = qbld.createQueryDefinition(IB.class);
                        dobj.where(dobj.get("aid").in(dobj.param("id")));
                    }

                    protected void setParams(List<Integer> p) {
                        q.setParameter("id", p);
                    }

                    protected void check(int id, IB o) {
                        assert o != null;
                        verify(id, o.getId());
                        verifyAttr(id, o);
                    }
                });

            ops.add(
                new WriteOp("B_clearA_", xMode) {
                    protected void write(int id) {
                        // blind update
                        final IB o = session.newInstance(IB.class);
                        o.setId(id);
                        final int aId = -1;
                        o.setAid(aId);
                        session.updatePersistent(o);
                    }
                });

            ops.add(
                new WriteOp("B_del_", xMode) {
                    protected void write(int id) {
                        // blind delete
                        final IB o = session.newInstance(IB.class);
                        o.setId(id);
                        session.remove(o);
                    }
                });

            ops.add(
                new WriteOp("A_del_", xMode) {
                    protected void write(int id) {
                        // blind delete
                        final IA o = session.newInstance(IA.class);
                        o.setId(id);
                        session.remove(o);
                    }
                });
            
            ops.add(
                new WriteOp("A_ins_", xMode) {
                    protected void write(int id) {
                        final IA o = session.newInstance(IA.class);
                        o.setId(id);
                        session.persist(o);
                    }
                });
            
            ops.add(
                new WriteOp("B_ins_", xMode) {
                    protected void write(int id) {
                        final IB o = session.newInstance(IB.class);
                        o.setId(id);
                        session.persist(o);
                    }
                });
            
            ops.add(
                new ClusterjOp("B_delAll", null) {
                    public void run(int nOps) {
                        beginTransaction();
                        final int d = session.deletePersistentAll(IB.class);
                        verify(nOps, d);
                        commitTransaction();
                    }
                });

            ops.add(
                new ClusterjOp("A_delAll", null) {
                    public void run(int nOps) {
                        beginTransaction();
                        final int d = session.deletePersistentAll(IA.class);
                        verify(nOps, d);
                        commitTransaction();
                    }
                });
        }

        // prepare queries
        for (Op o : ops)
            ((ClusterjOp)o).init();
        out.println("     [ClusterjOp: " + ops.size() + "]");
    }

    protected void closeOperations() {
        out.println();
        out.print("closing operations ...");
        out.flush();
        for (Op o : ops)
            ((ClusterjOp)o).close();
        ops.clear();
        out.println("          [ok]");
    }

    // ----------------------------------------------------------------------

    protected void setAttr(IA o, int id) {
        o.setCint(id);
        o.setClong((long)id);
        o.setCfloat((float)id);
        o.setCdouble((double)id);
    }

    protected void setAttr(IB o, int id) {
        o.setCint(id);
        o.setClong((long)id);
        o.setCfloat((float)id);
        o.setCdouble((double)id);
    }

    protected void verifyAttr(int i, IA o) {
        assert o != null;
        verify(i, o.getCint());
        verify(i, o.getClong());
        verify(i, o.getCfloat());
        verify(i, o.getCdouble());
    }

    protected void verifyAttr(int i, IB o) {
        assert o != null;
        verify(i, o.getCint());
        verify(i, o.getClong());
        verify(i, o.getCfloat());
        verify(i, o.getCdouble());
    }

    // ----------------------------------------------------------------------

    protected void beginTransaction() {
        session.currentTransaction().begin();
    }

    protected void commitTransaction() {
        session.currentTransaction().commit();
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
