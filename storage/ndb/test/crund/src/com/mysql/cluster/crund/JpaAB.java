/*
  Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.

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

import java.util.Properties;
import java.util.Collection;
import java.util.List;
import java.util.ArrayList;

import javax.persistence.Persistence;
import javax.persistence.EntityManagerFactory;
import javax.persistence.EntityManager;
import javax.persistence.Query;
//import javax.persistence.TypedQuery; // XXX JPA2.0, requires OpenJPA >=2.0
import javax.persistence.FlushModeType;
//import javax.persistence.PersistenceContextType; // XXX only by injection

import com.mysql.cluster.crund.CrundDriver.XMode;

/**
 * The JPA benchmark implementation.
 */
public class JpaAB extends CrundLoad {

    // JPA settings
    protected String jdbcDriver;
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
    //protected String lmSuffix; // XXX check LockMode support in JPA
    //query.setHint(QueryHints.QUERY_TYPE, QueryType.ReadObject);
    //query.setHint(QueryHints.PESSIMISTIC_LOCK, PessimisticLock.LockNoWait);

    public JpaAB(CrundDriver driver) {
        super(driver);
    }

    static public void main(String[] args) {
        System.out.println("JpaAB.main()");
        CrundDriver.parseArguments(args);
        final CrundDriver driver = new CrundDriver();
        final CrundLoad load = new JpaAB(driver);
        driver.run();
        System.out.println();
        System.out.println("JpaAB.main(): done.");
    }

    // ----------------------------------------------------------------------
    // JPA intializers/finalizers
    // ----------------------------------------------------------------------

    protected void initProperties() {
        out.println();
        out.print("reading jpa properties ...");

        final StringBuilder msg = new StringBuilder();
        final Properties props = driver.props;

        // load the JDBC driver class
        jdbcDriver = props.getProperty("openjpa.ConnectionDriverName");
        if (jdbcDriver == null) {
            throw new RuntimeException("Missing property: "
                                       + "openjpa.ConnectionDriverName");
        }
        try {
            Class.forName(jdbcDriver);
        } catch (ClassNotFoundException e) {
            out.println("Cannot load JDBC driver '" + jdbcDriver
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
            driver.hasIgnoredSettings = true;
            out.println();
            out.print(msg.toString());
        }

        // have brokerFactory... initialized first
        final String c = ("ndb".equals(brokerFactory)
                          ? ("clusterj(" + ndbConnectString + ")")
                          : url);
        name = "jpa_" + c.substring(0, 10); // shortcut will do
    }

    protected void printProperties() {
        out.println();
        out.println("jpa settings ...");
        out.println("openjpa.ConnectionDriverName:   " + jdbcDriver);
        out.println("openjpa.ConnectionURL:          " + url);
        out.println("openjpa.ConnectionUserName:     \"" + user + "\"");
        out.println("openjpa.ConnectionPassword:     \"" + password + "\"");
        out.println("openjpa.ConnectionRetainMode:   " + connectionRetainMode);
        out.println("openjpa.BrokerFactory:          " + brokerFactory);
        out.println("openjpa.ndb.connectString:      " + ndbConnectString);
        out.println("openjpa.ndb.database:           " + ndbDatabase);
    }

    public void init() throws Exception {
        assert emf == null;
        super.init();
        out.println();
        out.print("creating JPA EMFactory ...");
        out.flush();
        // create EMF by standard API, which allows vendors to pool factories
        emf = Persistence.createEntityManagerFactory("crundjpa", driver.props);
        out.println("      [EMF: 1]");
    }

    public void close() throws Exception {
        assert emf != null;
        out.println();
        out.print("closing JPA EMFactory ...");
        out.flush();
        if (emf != null)
            emf.close();
        emf = null;
        out.println("       [ok]");
        super.close();
    }

    // ----------------------------------------------------------------------
    // JPA operations
    // ----------------------------------------------------------------------

    // current model assumption: relationships only 1:1 identity
    // (target id of a navigation operation is verified against source id)
    protected abstract class JpaOp extends Op {
        protected XMode xMode;

        public JpaOp(String name, XMode m) {
            super(name + "," + m);
            this.xMode = m;
        }

        public void init() {}

        public void close() {}
    };

    protected abstract class WriteOp extends JpaOp {
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
                for (int i = 0; i < n; i++) {
                    write(id[i]);
                    if (xMode == XMode.each)
                        em.flush();
                }
                commitTransaction();
                break;
            }
        }

        protected abstract void write(int id);
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
            if (xMode == XMode.bulk)
                name = "[" + name + "]";
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
                for (int i = 0; i < n; i++)
                    read(id[i]);
                commitTransaction();
                break;
            }
        }

        protected abstract void read(int id);
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
        public void run(int[] id) {
            final int n = id.length;
            switch (xMode) {
            case indy :
                q.setFlushMode(FlushModeType.COMMIT);
                for (int i = 0; i < n; i++) {
                    beginTransaction();
                    setParams(id[i]);
                    final int u = q.executeUpdate();
                    verify(1, u);
                    commitTransaction();
                }
                break;
            case each :
                q.setFlushMode(FlushModeType.AUTO);
            case bulk :
                q.setFlushMode(FlushModeType.COMMIT);
                beginTransaction();
                for (int i = 0; i < n; i++) {
                    setParams(id[i]);
                    final int u = q.executeUpdate();
                    verify(1, u);
                }
                commitTransaction();
                break;
            }
        }

        protected abstract void setParams(int id);
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
        public void run(int[] id) {
            final int n = id.length;
            q.setFlushMode(FlushModeType.COMMIT);
            switch (xMode) {
            case indy :
                for (int i = 0; i < n; i++) {
                    beginTransaction();
                    setParams(id[i]);
                    E e = (E)q.getSingleResult(); // XXX cast -> TypedQuery
                    assert e != null;
                    getValues(id[i], e);
                    commitTransaction();
                }
                break;
            case each :
                beginTransaction();
                for (int i = 0; i < n; i++) {
                    setParams(id[i]);
                    E e = (E)q.getSingleResult(); // XXX cast -> TypedQuery
                    assert e != null;
                    getValues(id[i], e);
                }
                commitTransaction();
                break;
            case bulk :
                beginTransaction();
                List<Integer> l = new ArrayList<Integer>();
                for (int i = 0; i < n; i++)
                    l.add(id[i]);
                setParams(l);

                List<E> es = q.getResultList();
                assert es != null;
                verify(n, es.size());
                for (int i = 0; i < n; i++)
                    getValues(id[i], es.get(i));
                commitTransaction();
                break;
            }
        }

        protected void setParams(Object o) {}

        protected void getValues(int id, E e) {}
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
                        final A o = new A();
                        o.setId(id);
                        setAttr(o, -id);
                        em.persist(o);
                    }
                });

            ops.add(
                new WriteOp("B_insAttr", xMode) {
                    protected void write(int id) {
                        final B o = new B();
                        o.setId(id);
                        setAttr(o, -id);
                        em.persist(o);
                    }
                });

            ops.add(
                new WriteOp("A_setAttr", xMode) {
                    protected void write(int id) {
                        // blind update
                        final A o = em.getReference(A.class, id);
                        assert o != null;
                        setAttr(o, id);
                    }
                });

            ops.add(
                new WriteOp("B_setAttr", xMode) {
                    protected void write(int id) {
                        // blind update
                        final B o = em.getReference(B.class, id);
                        assert o != null;
                        setAttr(o, id);
                    }
                });

            ops.add(
                new ReadOp("A_getAttr", xMode, "A") {
                    protected void read(int id) {
                        final A o = em.find(A.class, id); // eager load
                        verify(id, o.getId());
                        verifyAttr(id, o);
                    }
                });

            ops.add(
                new ReadOp("B_getAttr", xMode, "B") {
                    protected void read(int id) {
                        final B o = em.find(B.class, id); // eager load
                        verify(id, o.getId());
                        verifyAttr(id, o);
                    }
                });

            ops.add(
                new QReadOp<A>("A_getAttr_wherein", xMode,
                               "SELECT a FROM A a WHERE a.id IN (?1) ORDER BY a.id") {
                    protected void setParams(Object o) {
                        q.setParameter(1, o);
                    }

                    protected void getValues(int id, A a) {
                        assert a != null;
                        verify(id, a.getId());
                        verifyAttr(id, a);
                    }
                });

            ops.add(
                new QReadOp<B>("B_getAttr_wherein", xMode,
                               "SELECT b FROM B b WHERE b.id IN (?1) ORDER BY b.id") {
                    protected void setParams(Object o) {
                        q.setParameter(1, o);
                    }

                    protected void getValues(int id, B b) {
                        assert b != null;
                        verify(id, b.getId());
                        verifyAttr(id, b);
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
                            final B o = em.getReference(B.class, id);
                            assert o != null;
                            o.setCvarbinary_def(b);
                        }
                    });

                ops.add(
                    new ReadOp("B_getVarbin_" + l, xMode, "B") {
                        protected void read(int id) {
                            // lazy load
                            final B o = em.getReference(B.class, id);
                            assert o != null;
                            verify(b, o.getCvarbinary_def());
                        }
                    });

                ops.add(
                    new WriteOp("B_clearVarbin_" + l, xMode) {
                        protected void write(int id) {
                            // blind update
                            final B o = em.getReference(B.class, id);
                            assert o != null;
                            o.setCvarbinary_def(null);
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
                            final B o = em.getReference(B.class, id);
                            assert o != null;
                            o.setCvarchar_def(s);
                        }
                    });

                ops.add(
                    new ReadOp("B_getVarchar_" + l, xMode, "B") {
                        protected void read(int id) {
                            // lazy load
                            final B o = em.getReference(B.class, id);
                            assert o != null;
                            verify(s, o.getCvarchar_def());
                        }
                    });

                ops.add(
                    new WriteOp("B_clearVarchar_" + l, xMode) {
                        protected void write(int id) {
                            // blind update
                            final B o = em.getReference(B.class, id);
                            assert o != null;
                            o.setCvarchar_def(null);
                        }
                    });
            }

            ops.add(
                new WriteOp("B_setA", xMode) {
                    protected void write(int id) {
                        // blind update
                        final int aId = id;
                        final B b = em.getReference(B.class, id);
                        assert b != null;
                        // lazy load
                        final A a = em.getReference(A.class, aId);
                        assert a != null;
                        b.setA(a);
                    }
                });

            ops.add(
                new ReadOp("B_getA", xMode, "B") {
                    protected void read(int id) {
                        // lazy load
                        final B b = em.getReference(B.class, id);
                        assert b != null;
                        final A a = b.getA();
                        verify(id, a.getId());
                        verifyAttr(id, a);
                    }
                });

            ops.add(
                new ReadOp("A_getBs", xMode, "B") {
                    protected void read(int id) {
                        // lazy load
                        final A a = em.getReference(A.class, id);
                        assert a != null;
                        final Collection<B> bs = a.getBs();
                        verify(1, bs.size());
                        for (B b : bs) {
                            verify(id, b.getId());
                            verifyAttr(id, b);
                        }
                    }
                });

            ops.add(
                new WriteOp("B_clearA", xMode) {
                    protected void write(int id) {
                        // blind update
                        final B b = em.getReference(B.class, id);
                        assert b != null;
                        b.setA(null);
                    }
                });

            ops.add(
                new QWriteOp<B>("B_setA_where", xMode,
                                "UPDATE B o SET o.a = ?2 WHERE o.id = ?1") {
                    protected void setParams(int id) {
                        final int aId = id;
                        q.setParameter(1, id);
                        // lazy load
                        final A a = em.getReference(A.class, aId);
                        assert a != null;
                        q.setParameter(2, a);
                    }
                });

            ops.add(
                new QReadOp<A>("B_getAs_wherein", xMode,
                               "SELECT b.a FROM B b WHERE b.id IN (?1) ORDER BY b.a.id") {
                    protected void setParams(Object o) {
                        q.setParameter(1, o);
                    }

                    protected void getValues(int id, A a) {
                        assert a != null;
                        verify(id, a.getId());
                        verifyAttr(id, a);
                    }
                });

            ops.add(
                new QReadOp<B>("A_getBs_wherein", xMode,
                               "SELECT b FROM B b WHERE b.a.id IN (?1)") {
                    protected void setParams(Object o) {
                        q.setParameter(1, o);
                    }

                    protected void getValues(int id, B b) {
                        assert b != null;
                        verify(id, b.getId());
                        verifyAttr(id, b);
                    }
                });

            ops.add(
                new QWriteOp<B>("B_clearA_where", xMode,
                                "UPDATE B o SET o.a = NULL WHERE o.id = ?1") {
                    protected void setParams(int id) {
                        q.setParameter(1, id);
                    }
                });

            ops.add(
                new WriteOp("B_del", xMode) {
                    protected void write(int id) {
                        // blind delete
                        final B o = em.getReference(B.class, id);
                        assert o != null;
                        em.remove(o);
                    }
                });

            ops.add(
                new WriteOp("A_del", xMode) {
                    protected void write(int id) {
                        // blind delete
                        final A o = em.getReference(A.class, id);
                        assert o != null;
                        em.remove(o);
                    }
                });

            ops.add(
                new WriteOp("A_ins", xMode) {
                    protected void write(int id) {
                        final A o = new A();
                        o.setId(id);
                        em.persist(o);
                    }
                });

            ops.add(
                new WriteOp("B_ins", xMode) {
                    protected void write(int id) {
                        final B o = new B();
                        o.setId(id);
                        em.persist(o);
                    }
                });

            ops.add(
                new QueryOp("B_delAll", XMode.bulk,
                            "DELETE FROM B") {
                    public void run(int[] id) {
                        final int n = id.length;
                        beginTransaction();
                        final int d = q.executeUpdate();
                        verify(n, d);
                        commitTransaction();
                    }
                });

            ops.add(
                new QueryOp("A_delAll", XMode.bulk,
                            "DELETE FROM A") {
                    public void run(int[] id) {
                        final int n = id.length;
                        beginTransaction();
                        final int d = q.executeUpdate();
                        verify(n, d);
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

    protected void clearPersistenceContext() {
        //out.println("clearing persistence context ...");
        // as long as the EM was not created with a Tx PC scope, i.e.,
        //   em = emf.createEntityManager(PersistenceContextType.TRANSACTION)
        // caching of objects beyond Tx scope can be effectively prevented
        // by clearing the EM's PC
        em.clear();
    }

    // ----------------------------------------------------------------------

    protected void setAttr(A o, int id) {
        o.setCint(id);
        o.setClong((long)id);
        o.setCfloat((float)id);
        o.setCdouble((double)id);
    }

    protected void setAttr(B o, int id) {
        o.setCint(id);
        o.setClong((long)id);
        o.setCfloat((float)id);
        o.setCdouble((double)id);
    }

    protected void verifyAttr(int id, A o) {
        assert o != null;
        verify(id, o.getCint());
        verify(id, o.getClong());
        verify(id, o.getCfloat());
        verify(id, o.getCdouble());
    }

    protected void verifyAttr(int id, B o) {
        assert o != null;
        verify(id, o.getCint());
        verify(id, o.getClong());
        verify(id, o.getCfloat());
        verify(id, o.getCdouble());
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

    public void initConnection() {
        assert em == null;
        out.println();
        out.println("initializing JPA resources ...");

        out.print("creating JPA EntityManager ...");
        out.flush();
        // see: clearPersistenceContext()
        // Tx-scope EM supported by JPA only by container injection:
        // em = emf.createEntityManager(PersistenceContextType.TRANSACTION);
        em = emf.createEntityManager();
        delAllA = em.createQuery("DELETE FROM A");
        delAllB = em.createQuery("DELETE FROM B");
        out.println("  [EM: 1]");

        initOperations();
    }

    public void closeConnection() {
        assert em != null;
        out.println();
        out.println("releasing JPA resources ...");

        closeOperations();

        out.print("closing JPA EntityManager ...");
        out.flush();
        delAllB = null;
        delAllA = null;
        if (em != null)
            em.close();
        em = null;
        out.println("   [ok]");
    }

    public void clearData() {
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
}
