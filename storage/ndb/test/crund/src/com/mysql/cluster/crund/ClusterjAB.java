/*
  Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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

import java.util.Collection;
import java.util.Map;
import java.util.List;
import java.util.ArrayList;
import java.util.Properties;
import java.util.Set;
import java.util.TreeSet;
import java.util.Comparator;

// XXX ndbapi constant not in clusterj-api.jar
//import com.mysql.ndbjtie.ndbapi.NdbIndexScanOperation.NotSpecified.MaxRangeNo;

import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.Constants;
import com.mysql.clusterj.Query;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.SessionFactory;
import com.mysql.clusterj.LockMode;
import com.mysql.clusterj.query.QueryDomainType;
import com.mysql.clusterj.query.QueryBuilder;
import com.mysql.clusterj.query.Predicate;

import com.mysql.cluster.crund.CrundDriver.XMode;

/**
 * The ClusterJ benchmark implementation.
 */
public class ClusterjAB extends CrundLoad {

    // ClusterJ settings
    protected String mgmdConnect;

    // ClusterJ resources
    protected SessionFactory sessionFactory;
    protected Session session;

    public ClusterjAB(CrundDriver driver) {
        super(driver);
    }

    static public void main(String[] args) {
        System.out.println("ClusterjAB.main()");
        CrundDriver.parseArguments(args);
        final CrundDriver driver = new CrundDriver();
        final CrundLoad load = new ClusterjAB(driver);
        driver.run();
        System.out.println();
        System.out.println("ClusterjAB.main(): done.");
    }

    // ----------------------------------------------------------------------
    // ClusterJ intializers/finalizers
    // ----------------------------------------------------------------------

    @Override
    protected void initProperties() {
        out.println();
        out.print("reading ClusterJ properties ...");

        final StringBuilder msg = new StringBuilder();
        final Properties props = driver.props;

        // check required properties
        mgmdConnect
            = props.getProperty(Constants.PROPERTY_CLUSTER_CONNECTSTRING);

        if (msg.length() == 0) {
            out.println(" [ok]");
        } else {
            driver.hasIgnoredSettings = true;
            out.println();
            out.print(msg.toString());
        }

        name = "clusterj"; // shortcut will do, "(" + mgmdConnect + ")";
    }

    @Override
    protected void printProperties() {
        out.println();
        out.println("ClusterJ settings ...");
        out.println("ndb.mgmdConnect                 " + mgmdConnect);
        for (Map.Entry<Object,Object> e : driver.props.entrySet()) {
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
    public void init() throws Exception {
        assert sessionFactory == null;
        super.init();
        out.println();
        out.print("creating SessionFactory ...");
        out.flush();
        sessionFactory = ClusterJHelper.getSessionFactory(driver.props);
        out.println("     [SessionFactory: 1]");
    }

    @Override
    public void close() throws Exception {
        assert sessionFactory != null;
        out.println();
        out.print("closing SessionFactory ...");
        out.flush();
        if (sessionFactory != null)
            sessionFactory.close();
        sessionFactory = null;
        out.println("      [ok]");
        super.close();
    }

    // ----------------------------------------------------------------------
    // ClusterJ operations
    // ----------------------------------------------------------------------

    // current model assumption: relationships only 1:1 identity
    // (target id of a navigation operation is verified against source id)
    protected abstract class ClusterjOp extends Op {
        protected XMode xMode;

        public ClusterjOp(String name, XMode m) {
            super(name + "," + m);
            this.xMode = m;
        }

        public void init() {}

        public void close() {}
    };

    protected abstract class WriteOp extends ClusterjOp {
        public WriteOp(String name, XMode m) {
            super(name, m);
        }

        public void run(int[] id) {
            final int n = id.length;
            switch (xMode) {
            case indy :
                for (int i = 0; i < n; i++) {
                    beginTransaction();
                    write(id[i]);
                    commitTransaction();
                }
                break;
            case each :
            case bulk :
                // Approach: control when persistent context is flushed,
                // i.e., at commit for 1 database roundtrip only.
                beginTransaction();
                for (int i = 0; i < n; i++) {
                    write(id[i]);
                    if (xMode == XMode.each)
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

        public void run(int[] id) {
            final int n = id.length;
            switch (xMode) {
            case indy :
                for (int i = 0; i < n; i++) {
                    beginTransaction();
                    read(id[i]);
                    commitTransaction();
                }
                break;
            case each :
                beginTransaction();
                for (int i = 0; i < n; i++)
                    read(id[i]);
                commitTransaction();
                break;
            case bulk :
                beginTransaction();
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
        protected Comparator<I> cmp;

        public QReadOp(String name, XMode m, Comparator<I> cmp) {
            super(name, m);
            this.cmp = cmp;
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
            final int maxInListLength = 0x0fff;
            //assert maxInListLength == MaxRangeNo; // ndbjtie dependency

            for (int i = 0; i < n; i += maxInListLength) {
                final int k = n - i;
                final int l = (k > maxInListLength ? maxInListLength : k);
                final int m = i + l;
                assert m <= n;

                final List<Integer> p = new ArrayList<Integer>(m);
                for (int j = i; j < m; j++)
                    p.add(id[j]);
                setParams(p);

                final List<I> os = q.getResultList();
                assert os != null;
                verify(l, os.size());

                // sort results, absent of order-by support for queries
                if (cmp != null) {
                    final Set<I> oss = new TreeSet<I>(cmp);
                    oss.addAll(os);
                    os.clear();
                    os.addAll(oss);
                }
                
                for (int j = i, jj = 0; j < m; j++, jj++) {
                    final I o = os.get(jj);
                    assert o != null;
                    check(id[j], o);
                }
            }
        }

        protected abstract void setDOQuery();

        protected abstract void setParams(List<Integer> p);

        protected abstract void check(int id, I o);
    }

    protected void initOperations() {
        out.print("initializing operations ...");
        out.flush();

        for (XMode m : driver.xModes) {
            // inner classes can only refer to a constant
            final XMode xMode = m;
            final boolean setAttrs = true;

            ops.add(
                new WriteOp("A_insAttr", xMode) {
                    protected void write(int id) {
                        final IA o = session.newInstance(IA.class);
                        o.setId(id);
                        setAttr(o, -id);
                        session.persist(o);
                    }
                });

            ops.add(
                new WriteOp("B_insAttr", xMode) {
                    protected void write(int id) {
                        final IB o = session.newInstance(IB.class);
                        o.setId(id);
                        setAttr(o, -id);
                        session.persist(o);
                    }
                });

            ops.add(
                new WriteOp("A_setAttr", xMode) {
                    protected void write(int id) {
                        final IA o = session.newInstance(IA.class);
                        assert o != null;
                        o.setId(id);
                        setAttr(o, id);
                        session.updatePersistent(o);
                    }
                });

            ops.add(
                new WriteOp("B_setAttr", xMode) {
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
                new FetchOp<IA>("A_getAttr", xMode, IA.class) {
                    protected void check(int id, IA o) {
                        assert o != null;
                        verify(id, o.getId());
                        verifyAttr(id, o);
                    }
                });

            ops.add(
                new FetchOp<IB>("B_getAttr", xMode, IB.class) {
                    protected void check(int id, IB o) {
                        assert o != null;
                        verify(id, o.getId());
                        verifyAttr(id, o);
                    }
                });

            ops.add(
                new QReadOp<IA>("A_getAttr_wherein", xMode, cmpIA) {
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
                new QReadOp<IB>("B_getAttr_wherein", xMode, cmpIB) {
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
                if (l > driver.maxVarbinaryBytes)
                    break;

                ops.add(
                    new WriteOp("B_setVarbin_" + l, xMode) {
                        protected void write(int id) {
                            // blind update
                            final IB o = session.newInstance(IB.class);
                            o.setId(id);
                            o.setCvarbinary_def(b);
                            session.updatePersistent(o);
                        }
                    });

                ops.add(
                    new FetchOp<IB>("B_getVarbin_" + l,
                                    xMode, IB.class) {
                        protected void check(int id, IB o) {
                            assert o != null;
                            verify(b, o.getCvarbinary_def());
                        }
                    });

                ops.add(
                    new WriteOp("B_clearVarbin_" + l, xMode) {
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
                if (l > driver.maxVarcharChars)
                    break;

                ops.add(
                    new WriteOp("B_setVarchar_" + l, xMode) {
                        protected void write(int id) {
                            // blind update
                            final IB o = session.newInstance(IB.class);
                            o.setId(id);
                            o.setCvarchar_def(s);
                            session.updatePersistent(o);
                        }
                    });

                ops.add(
                    new FetchOp<IB>("B_getVarchar_" + l,
                                    xMode, IB.class) {
                        protected void check(int id, IB o) {
                            assert o != null;
                            verify(s, o.getCvarchar_def());
                        }
                    });

                ops.add(
                    new WriteOp("B_clearVarchar_" + l, xMode) {
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
                new WriteOp("B_setA", xMode) {
                    protected void write(int id) {
                        // blind update
                        final IB o = session.newInstance(IB.class);
                        o.setId(id);
                        o.setAid(id);
                        session.updatePersistent(o);
                    }
                });

            ops.add(
                new ReadOp("B_getA", xMode) {
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
                new QReadOp<IA>("B_getA_wherein", xMode, null) { // XXX cmp
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
                new QReadOp<IB>("A_getBs_wherein", xMode, cmpIB) {
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
                new WriteOp("B_clearA", xMode) {
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
                new WriteOp("B_del", xMode) {
                    protected void write(int id) {
                        // blind delete
                        final IB o = session.newInstance(IB.class);
                        o.setId(id);
                        session.remove(o);
                    }
                });

            ops.add(
                new WriteOp("A_del", xMode) {
                    protected void write(int id) {
                        // blind delete
                        final IA o = session.newInstance(IA.class);
                        o.setId(id);
                        session.remove(o);
                    }
                });

            ops.add(
                new WriteOp("A_ins", xMode) {
                    protected void write(int id) {
                        final IA o = session.newInstance(IA.class);
                        o.setId(id);
                        session.persist(o);
                    }
                });

            ops.add(
                new WriteOp("B_ins", xMode) {
                    protected void write(int id) {
                        final IB o = session.newInstance(IB.class);
                        o.setId(id);
                        session.persist(o);
                    }
                });

            ops.add(
                new ClusterjOp("B_delAll", XMode.bulk) {
                    public void run(int[] id) {
                        final int n = id.length;
                        beginTransaction();
                        final int d = session.deletePersistentAll(IB.class);
                        verify(n, d);
                        commitTransaction();
                    }
                });

            ops.add(
                new ClusterjOp("A_delAll", XMode.bulk) {
                    public void run(int[] id) {
                        final int n = id.length;
                        beginTransaction();
                        final int d = session.deletePersistentAll(IA.class);
                        verify(n, d);
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
        out.print("closing operations ...");
        out.flush();
        for (Op o : ops)
            ((ClusterjOp)o).close();
        ops.clear();
        out.println("          [ok]");
    }

    protected void clearPersistenceContext() {
        // nothing to do as we're not caching beyond Tx scope
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

    protected void verifyAttr(int id, IA o) {
        assert o != null;
        verify(id, o.getCint());
        verify(id, o.getClong());
        verify(id, o.getCfloat());
        verify(id, o.getCdouble());
    }

    protected void verifyAttr(int id, IB o) {
        assert o != null;
        verify(id, o.getCint());
        verify(id, o.getClong());
        verify(id, o.getCfloat());
        verify(id, o.getCdouble());
    }

    protected final Comparator<IA> cmpIA = new Comparator<IA>() {
        public int compare(IA o1, IA o2) {
            return (o1.getId() - o2.getId());
        }
    };
        
    protected final Comparator<IB> cmpIB = new Comparator<IB>() {
        public int compare(IB o1, IB o2) {
            return (o1.getId() - o2.getId());
        }
    };
        
    // ----------------------------------------------------------------------

    protected void beginTransaction() {
        session.currentTransaction().begin();
    }

    protected void commitTransaction() {
        session.currentTransaction().commit();
    }

    // ----------------------------------------------------------------------
    // ClusterJ datastore operations
    // ----------------------------------------------------------------------

    @Override
    public void initConnection() {
        assert session == null;
        out.println();
        out.println("initializing ClusterJ resources ...");

        out.print("creating ClusterJ Session ...");
        out.flush();
        session = sessionFactory.getSession();
        out.println("   [Session: 1]");

        out.print("setting session lock mode ...");
        out.flush();
        final LockMode lm;
        switch (driver.lockMode) {
        case none:
            lm = LockMode.READ_COMMITTED;
            break;
        case shared:
            lm = LockMode.SHARED;
            break;
        case exclusive:
            lm = LockMode.EXCLUSIVE;
            break;
        default:
            lm = null;
            assert false;
        }
        session.setLockMode(lm);
        out.println("   [ok: " + lm + "]");

        initOperations();
    }

    @Override
    public void closeConnection() {
        assert session != null;
        out.println();
        out.println("releasing ClusterJ resources ...");

        closeOperations();

        out.print("closing ClusterJ Session ...");
        out.flush();
        if (session != null)
            session.close();
        session = null;
        out.println("    [ok]");
    }

    @Override
    public void clearData() {
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
}
