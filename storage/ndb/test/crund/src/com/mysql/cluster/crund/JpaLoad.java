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

import java.util.Collection;
import java.util.Iterator;
import java.util.Arrays;
import java.util.List;
import java.util.ArrayList;

import javax.persistence.Persistence;
import javax.persistence.EntityManagerFactory;
import javax.persistence.EntityManager;
import javax.persistence.Query;
//import javax.persistence.TypedQuery; // XXX JPA2.0, requires OpenJPA >=2.0
import javax.persistence.FlushModeType;
import javax.persistence.PersistenceContextType;

/**
 * The JPA benchmark implementation.
 */
public class JpaLoad extends CrundDriver {

    // JPA settings
    protected String driver;
    protected String url;
    protected String user;
    protected String password;
    protected String connectionRetainMode;
    protected String brokerFactory;
    protected String ndbConnectString;
    protected String ndbDatabase;

    // JPA resources
    protected EntityManagerFactory emf;
    protected EntityManager em;
    protected Query delAllA;
    protected Query delAllB;

    // ----------------------------------------------------------------------
    // JPA intializers/finalizers
    // ----------------------------------------------------------------------

    protected void initProperties() {
        super.initProperties();

        out.print("setting jpa properties ...");

        final StringBuilder msg = new StringBuilder();
        final String eol = System.getProperty("line.separator");

        // load the JDBC driver class
        driver = props.getProperty("openjpa.ConnectionDriverName");
        if (driver == null) {
            throw new RuntimeException("Missing property: "
                                       + "openjpa.ConnectionDriverName");
        }
        try {
            Class.forName(driver);
        } catch (ClassNotFoundException e) {
            out.println("Cannot load JDBC driver '" + driver
                        + "' from classpath '"
                        + System.getProperty("java.class.path") + "'");
            throw new RuntimeException(e);
        }

        url = props.getProperty("openjpa.ConnectionURL");
        if (url == null) {
            throw new RuntimeException("Missing property: "
                                       + "openjpa.ConnectionURL");
        }

        user = props.getProperty("openjpa.ConnectionUserName");
        password = props.getProperty("openjpa.ConnectionPassword");

        connectionRetainMode = props.getProperty("openjpa.ConnectionRetainMode");
        if (connectionRetainMode == null) {
            throw new RuntimeException("Missing property: "
                                       + "openjpa.ConnectionRetainMode");
        }

        brokerFactory = props.getProperty("openjpa.BrokerFactory");
        ndbConnectString = props.getProperty("openjpa.ndb.connectString");
        ndbDatabase = props.getProperty("openjpa.ndb.database");
        if ("ndb".equals(brokerFactory)) {
            if (ndbConnectString == null) {
                throw new RuntimeException("Missing property: "
                                           + "openjpa.ndb.connectString");
            }
            if (ndbDatabase == null) {
                throw new RuntimeException("Missing property: "
                                           + "openjpa.ndb.database");
            }
        }

        if (msg.length() == 0) {
            out.println("      [ok]");
        } else {
            out.println();
            out.print(msg.toString());
        }

        // have brokerFactory... initialized first
        final String c =  ("ndb".equals(brokerFactory)
                           ? ("clusterj(" + ndbConnectString + ")")
                           : url);
        descr = "->jpa->" + c;
    }

    protected void printProperties() {
        super.printProperties();

        out.println();
        out.println("jpa settings ...");
        out.println("openjpa.ConnectionDriverName:   " + driver);
        out.println("openjpa.ConnectionURL:          " + url);
        out.println("openjpa.ConnectionUserName:     \"" + user + "\"");
        out.println("openjpa.ConnectionPassword:     \"" + password + "\"");
        out.println("openjpa.ConnectionRetainMode:   " + connectionRetainMode);
        out.println("openjpa.BrokerFactory:          " + brokerFactory);
        out.println("openjpa.ndb.connectString:      " + ndbConnectString);
        out.println("openjpa.ndb.database:           " + ndbDatabase);
    }

    protected void initLoad() throws Exception {
        // XXX support generic load class
        //super.init();

        out.println();
        out.print("creating JPA EMFactory ...");
        out.flush();
        // create EMF by standard API, which allows vendors to pool factories
        emf = Persistence.createEntityManagerFactory("crundjpa", props);
        out.println("      [EMF: 1]");
    }

    protected void closeLoad() throws Exception {
        out.println();
        out.print("closing JPA EMFactory ...");
        out.flush();
        if (emf != null)
            emf.close();
        emf = null;
        out.println("       [ok]");

        // XXX support generic load class
        //super.close();
    }

    // ----------------------------------------------------------------------
    // JPA operations
    // ----------------------------------------------------------------------

    // assumes PKs: 0..nOps, relationships: identity 1:1
    protected abstract class JpaOp extends Op {
        protected XMode xMode;

        public JpaOp(String name, XMode m) {
            super(name + (m == null ? "" : toStr(m)));
            this.xMode = m;
        }

        public void init() {}

        public void close() {}
    };

    protected abstract class WriteOp extends JpaOp {
        public WriteOp(String name, XMode m) {
            super(name, m);
        }

        public void run(int nOps) {
            switch (xMode) {
            case INDY :
                for (int i = 0; i < nOps; i++) {
                    beginTransaction();
                    write(i);
                    commitTransaction();
                }
                break;
            case EACH :
            case BULK :
                // Approach: control when persistent context is flushed,
                // i.e., at commit for 1 database roundtrip only.
                //
                // JPA's mechanisms for bulk updates are numerous but not all
                // are generically applicable here:
                // - JPA 1.0 FlushModeType.COMMIT with DML queries
                //   [this is approach is used for QReadOps below]
                // - JPA 1.0 queries with in-clause or broad where conditions
                // - JPA 1.0 relationship annotation parameters (CascadeType)
                // - vendor-specific batch writing properties
                // - vendor-specific persistent-context flush-mode properties
                beginTransaction();
                for (int i = 0; i < nOps; i++) {
                    write(i);
                    if (xMode == XMode.EACH)
                        em.flush();
                }
                commitTransaction();
                break;
            }
        }

        protected abstract void write(int i);
    }
    
    protected abstract class ReadOp extends JpaOp {
        final Query preload;

        public ReadOp(String name, XMode m, String entity) {
            super(name, m);
            preload = em.createQuery("SELECT o FROM " + entity + " o");
        }

        public void init() {
            super.init();
            // simulating generic bulk reads by preloading extent
            if (xMode == XMode.BULK)
                name = "[" + name + "]";
        }
        
        public void run(int nOps) {
            switch (xMode) {
            case INDY :
                for (int i = 0; i < nOps; i++) {
                    beginTransaction();
                    read(i);
                    commitTransaction();
                }
                break;
            case EACH :
                beginTransaction();
                for (int i = 0; i < nOps; i++)
                    read(i);
                commitTransaction();
                break;
            case BULK :
                // Approach: simulate generic bulk reads by preloading extent
                // into persistent context; subsequent reads/queries will then
                // execute without database roundtrips.
                //
                // JPA's mechanisms for bulk reads are numerous yet many
                // are not generically applicable here:
                // - JPA 1.0 where-in clause with list argument in queries
                //   [this is implemented as a separate operation below]
                // - JPA 1.0 join-fetch, where-exist clauses in JPQL queries
                // - JPA 1.0 relationship annotation (FetchType, CascadeType)
                // - JPA 2.0 caching annotation (@Cacheable)
                // - vendor-specific query, entity hints/annotations/properties
                // - vendor-specific caching, persistent-context properties
                beginTransaction();
                preload.getResultList();
                for (int i = 0; i < nOps; i++)
                    read(i);
                commitTransaction();
                break;
            }
        }

        protected abstract void read(int i);
    }

    protected abstract class QueryOp<E> extends JpaOp {
        protected String jpql;
        protected Query q;  // XXX use TypedQuery from JPA2.0

        public QueryOp(String name, XMode m, String jpql) {
            super(name, m);
            this.jpql = jpql;
        }

        public void init() {
            super.init();
            q = em.createQuery(jpql);
        }

        public void close() {
            q = null;
        }
    }

    protected abstract class QWriteOp<E> extends QueryOp<E> {
        public QWriteOp(String name, XMode m, String jpql) {
            super(name, m, jpql);
        }

        @SuppressWarnings("fallthrough")
        public void run(int nOps) {
            switch (xMode) {
            case INDY :
                q.setFlushMode(FlushModeType.COMMIT);
                for (int i = 0; i < nOps; i++) {
                    beginTransaction();
                    setParams(i);
                    final int u = q.executeUpdate();
                    verify(1, u);
                    commitTransaction();
                }
                break;
            case EACH :
                q.setFlushMode(FlushModeType.AUTO);
            case BULK :
                q.setFlushMode(FlushModeType.COMMIT);
                beginTransaction();
                for (int i = 0; i < nOps; i++) {
                    setParams(i);
                    final int u = q.executeUpdate();
                    verify(1, u);
                }
                commitTransaction();
                break;
            }
        }

        protected abstract void setParams(int i);
    }

    // assumes query with parametrized where-in clause for bulk reads
    protected abstract class QReadOp<E> extends QueryOp<E> {
        public QReadOp(String name, XMode m, String jpql) {
            super(name, m, jpql);
        }

        public void init() {
            super.init();
            assert jpql.contains("IN (?1)");
        }
        
        @SuppressWarnings("unchecked") // XXX warning -> TypedQuery
        public void run(int nOps) {
            q.setFlushMode(FlushModeType.COMMIT);
            switch (xMode) {
            case INDY :
                for (int i = 0; i < nOps; i++) {
                    beginTransaction();
                    setParams(i);
                    E e = (E)q.getSingleResult(); // XXX cast -> TypedQuery
                    assert e != null;
                    getValues(i, e);
                    commitTransaction();
                }
                break;
            case EACH :
                beginTransaction();
                for (int i = 0; i < nOps; i++) {
                    setParams(i);
                    E e = (E)q.getSingleResult(); // XXX cast -> TypedQuery
                    assert e != null;
                    getValues(i, e);
                }
                commitTransaction();
                break;
            case BULK :
                beginTransaction();
                List<Integer> l = new ArrayList<Integer>();
                for (int i = 0; i < nOps; i++)
                    l.add(i);
                setParams(l);

                List<E> es = q.getResultList();
                assert es != null;
                verify(nOps, es.size());
                for (int i = 0; i < nOps; i++)
                    getValues(i, es.get(i));
                commitTransaction();
                break;
            }
        }

        protected void setParams(Object o) {}

        protected void getValues(int i, E e) {}
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
                    protected void write(int i) {
                        final A o = new A();
                        o.setId(i);
                        setAttr(o, -i);
                        em.persist(o);
                    }
                });

            ops.add(
                new WriteOp("B_insAttr_", xMode) {
                    protected void write(int i) {
                        final B o = new B();
                        o.setId(i);
                        setAttr(o, -i);
                        em.persist(o);
                    }
                });

            ops.add(
                new WriteOp("A_setAttr_", xMode) {
                    protected void write(int i) {
                        // blind update
                        final A o = em.getReference(A.class, i);
                        assert o != null;
                        setAttr(o, i);
                    }
                });

            ops.add(
                new WriteOp("B_setAttr_", xMode) {
                    protected void write(int i) {
                        // blind update
                        final B o = em.getReference(B.class, i);
                        assert o != null;
                        setAttr(o, i);
                    }
                });

            ops.add(
                new ReadOp("A_getAttr_", xMode, "A") {
                    protected void read(int i) {
                        final A o = em.find(A.class, i); // eager load
                        verify(i, o.getId());
                        verifyAttr(i, o);
                    }
                });

            ops.add(
                new ReadOp("B_getAttr_", xMode, "B") {
                    protected void read(int i) {
                        final B o = em.find(B.class, i); // eager load
                        verify(i, o.getId());
                        verifyAttr(i, o);
                    }
                });

            ops.add(
                new QReadOp<A>("A_getAttr_wherein_", xMode,
                               "SELECT a FROM A a WHERE a.id IN (?1) ORDER BY a.id") {
                    protected void setParams(Object o) {
                        q.setParameter(1, o);
                    }
                    
                    protected void getValues(int i, A a) {
                        assert a != null;
                        verify(i, a.getId());
                        verifyAttr(i, a);
                    }
                });

            ops.add(
                new QReadOp<B>("B_getAttr_wherein_", xMode,
                               "SELECT b FROM B b WHERE b.id IN (?1) ORDER BY b.id") {
                    protected void setParams(Object o) {
                        q.setParameter(1, o);
                    }
                    
                    protected void getValues(int i, B b) {
                        assert b != null;
                        verify(i, b.getId());
                        verifyAttr(i, b);
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
                        protected void write(int i) {
                            // blind update
                            final B o = em.getReference(B.class, i);
                            assert o != null;
                            o.setCvarbinary_def(b);
                        }
                    });

                ops.add(
                    new ReadOp("B_getVarbin_" + l + "_", xMode, "B") {
                        protected void read(int i) {
                            // lazy load
                            final B o = em.getReference(B.class, i);
                            assert o != null;
                            verify(b, o.getCvarbinary_def());
                        }
                    });

                ops.add(
                    new WriteOp("B_clearVarbin_" + l + "_", xMode) {
                        protected void write(int i) {
                            // blind update
                            final B o = em.getReference(B.class, i);
                            assert o != null;
                            o.setCvarbinary_def(null);
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
                        protected void write(int i) {
                            // blind update
                            final B o = em.getReference(B.class, i);
                            assert o != null;
                            o.setCvarchar_def(s);
                        }
                    });

                ops.add(
                    new ReadOp("B_getVarchar_" + l + "_", xMode, "B") {
                        protected void read(int i) {
                            // lazy load
                            final B o = em.getReference(B.class, i);
                            assert o != null;
                            verify(s, o.getCvarchar_def());
                        }
                    });

                ops.add(
                    new WriteOp("B_clearVarchar_" + l + "_", xMode) {
                        protected void write(int i) {
                            // blind update
                            final B o = em.getReference(B.class, i);
                            assert o != null;
                            o.setCvarchar_def(null);
                        }
                    });
            }

            ops.add(
                new WriteOp("B_setA_", xMode) {
                    protected void write(int i) {
                        // blind update
                        final int aId = i;
                        final B b = em.getReference(B.class, i);
                        assert b != null;
                        // lazy load
                        final A a = em.getReference(A.class, aId);
                        assert a != null;
                        b.setA(a);
                    }
                });

            ops.add(
                new ReadOp("B_getA_", xMode, "B") {
                    protected void read(int i) {
                        // lazy load
                        final B b = em.getReference(B.class, i);
                        assert b != null;
                        final A a = b.getA();
                        verify(i, a.getId());
                        verifyAttr(i, a);
                    }
                });

            ops.add(
                new ReadOp("A_getBs_", xMode, "B") {
                    protected void read(int i) {
                        // lazy load
                        final A a = em.getReference(A.class, i);
                        assert a != null;
                        final Collection<B> bs = a.getBs();
                        verify(1, bs.size());
                        for (B b : bs) {
                            verify(i, b.getId());
                            verifyAttr(i, b);
                        }
                    }
                });

            ops.add(
                new WriteOp("B_clearA_", xMode) {
                    protected void write(int i) {
                        // blind update
                        final B b = em.getReference(B.class, i);
                        assert b != null;
                        b.setA(null);
                    }
                });

            ops.add(
                new QWriteOp<B>("B_setA_where_", xMode,
                                "UPDATE B o SET o.a = ?2 WHERE o.id = ?1") {
                    protected void setParams(int i) {
                        final int aId = i;
                        q.setParameter(1, i);
                        // lazy load
                        final A a = em.getReference(A.class, aId);
                        assert a != null;
                        q.setParameter(2, a);
                    }
                });

            ops.add(
                new QReadOp<A>("B_getAs_wherein_", xMode,
                               "SELECT b.a FROM B b WHERE b.id IN (?1) ORDER BY b.a.id") {
                    protected void setParams(Object o) {
                        q.setParameter(1, o);
                    }
                    
                    protected void getValues(int i, A a) {
                        assert a != null;
                        verify(i, a.getId());
                        verifyAttr(i, a);
                    }
                });

            ops.add(
                new QReadOp<B>("A_getBs_wherein_", xMode,
                               "SELECT b FROM B b WHERE b.a.id IN (?1)") {
                    protected void setParams(Object o) {
                        q.setParameter(1, o);
                    }
                    
                    protected void getValues(int i, B b) {
                        assert b != null;
                        verify(i, b.getId());
                        verifyAttr(i, b);
                    }
                });

            ops.add(
                new QWriteOp<B>("B_clearA_where_", xMode,
                                "UPDATE B o SET o.a = NULL WHERE o.id = ?1") {
                    protected void setParams(int i) {
                        q.setParameter(1, i);
                    }
                });

            ops.add(
                new WriteOp("B_del_", xMode) {
                    protected void write(int i) {
                        // blind delete
                        final B o = em.getReference(B.class, i);
                        assert o != null;
                        em.remove(o);
                    }
                });

            ops.add(
                new WriteOp("A_del_", xMode) {
                    protected void write(int i) {
                        // blind delete
                        final A o = em.getReference(A.class, i);
                        assert o != null;
                        em.remove(o);
                    }
                });

            ops.add(
                new WriteOp("A_ins_", xMode) {
                    protected void write(int i) {
                        final A o = new A();
                        o.setId(i);
                        em.persist(o);
                    }
                });
            
            ops.add(
                new WriteOp("B_ins_", xMode) {
                    protected void write(int i) {
                        final B o = new B();
                        o.setId(i);
                        em.persist(o);
                    }
                });            

            ops.add(
                new QueryOp("B_delAll", null,
                            "DELETE FROM B") {
                    public void run(int nOps) {
                        beginTransaction();
                        final int d = q.executeUpdate();
                        verify(nOps, d);
                        commitTransaction();
                    }
                });

            ops.add(
                new QueryOp("A_delAll", null,
                            "DELETE FROM A") {
                    public void run(int nOps) {
                        beginTransaction();
                        final int d = q.executeUpdate();
                        verify(nOps, d);
                        commitTransaction();
                    }
                });
        }

        // prepare queries
        for (Op o : ops)
            ((JpaOp)o).init();
        out.println("     [JpaOp: " + ops.size() + "]");
    }

    protected void closeOperations() {
        out.print("closing operations ...");
        out.flush();
        for (Op o : ops)
            ((JpaOp)o).close();
        ops.clear();
        out.println("          [ok]");
    }

    // ----------------------------------------------------------------------

    protected void setAttr(A o, int i) {
        o.setCint(i);
        o.setClong((long)i);
        o.setCfloat((float)i);
        o.setCdouble((double)i);
    }

    protected void setAttr(B o, int i) {
        o.setCint(i);
        o.setClong((long)i);
        o.setCfloat((float)i);
        o.setCdouble((double)i);
    }

    protected void verifyAttr(int i, A o) {
        assert o != null;
        verify(i, o.getCint());
        verify(i, o.getClong());
        verify(i, o.getCfloat());
        verify(i, o.getCdouble());
    }

    protected void verifyAttr(int i, B o) {
        assert o != null;
        verify(i, o.getCint());
        verify(i, o.getClong());
        verify(i, o.getCfloat());
        verify(i, o.getCdouble());
    }

    // ----------------------------------------------------------------------

    protected void beginTransaction() {
        em.getTransaction().begin();
    }

    protected void commitTransaction() {
        em.getTransaction().commit();
    }

    // ----------------------------------------------------------------------
    // JPA datastore operations
    // ----------------------------------------------------------------------

    protected void initConnection() {
        out.println();
        out.print("creating JPA EntityManager ...");
        out.flush();
        // see: clearPersistenceContext()
        // Tx-scope EM supported by JPA only by container injection:
        // em = emf.createEntityManager(PersistenceContextType.TRANSACTION);
        em = emf.createEntityManager();

        // XXX check query.setHint(...) for standardized optimizations, e.g.:
        //import org.eclipse.persistence.config.QueryHints;
        //import org.eclipse.persistence.config.QueryType;
        //import org.eclipse.persistence.config.PessimisticLock;
        //import org.eclipse.persistence.config.HintValues;
        //query.setHint(QueryHints.QUERY_TYPE, QueryType.ReadObject);
        //query.setHint(QueryHints.PESSIMISTIC_LOCK, PessimisticLock.LockNoWait);
        //query.setHint("eclipselink.bulk", "e.address");
        //query.setHint("eclipselink.join-fetch", "e.address");

        delAllA = em.createQuery("DELETE FROM A");
        delAllB = em.createQuery("DELETE FROM B");
        out.println("  [EM: 1]");
    }

    protected void closeConnection() {
        out.println();
        out.print("closing JPA EntityManager ...");
        out.flush();
        delAllB = null;
        delAllA = null;
        if (em != null)
            em.close();
        em = null;
        out.println("   [ok]");
    }

    protected void clearPersistenceContext() {
        //out.println("clearing persistence context ...");
        // as long as the EM was not created with a Tx PC scope, i.e.,
        //   em = emf.createEntityManager(PersistenceContextType.TRANSACTION)
        // caching of objects beyond Tx scope can be effectively prevented
        // by clearing the EM's PC
        em.clear();
    }

    protected void clearData() {
        out.print("deleting all objects ...");
        out.flush();

        em.getTransaction().begin();
        int delB = delAllB.executeUpdate();
        out.print("        [B: " + delB);
        out.flush();
        int delA = delAllA.executeUpdate();
        out.print(", A: " + delA);
        out.flush();
        em.getTransaction().commit();
        em.clear();

        out.println("]");
    }

    // ----------------------------------------------------------------------

    static public void main(String[] args) {
        System.out.println("JpaLoad.main()");
        parseArguments(args);
        new JpaLoad().run();
        System.out.println();
        System.out.println("JpaLoad.main(): done.");
    }
}
