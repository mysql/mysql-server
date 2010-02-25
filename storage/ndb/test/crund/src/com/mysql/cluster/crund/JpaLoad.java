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

import java.util.Collection;
import java.util.Iterator;
import java.util.Arrays;
import java.util.ArrayList;

import javax.persistence.Persistence;
import javax.persistence.EntityManagerFactory;
import javax.persistence.EntityManager;
import javax.persistence.Query;
import javax.persistence.PersistenceContextType;


/**
 * A benchmark implementation against a JPA-mapped database.
 */
public class JpaLoad extends Driver {

    // JPA database connection
    protected String driver;
    protected String url;
    protected String connectionRetainMode;
    protected String ndbConnectString;
    protected String brokerFactory;
    protected EntityManagerFactory emf;
    protected EntityManager em;
    protected Query delAllA;
    protected Query delAllB0;

    protected abstract class JpaOp extends Op {
        public JpaOp(String name) {
            super(name);
        }

        public void init() {}

        public void close() {}
    };

    protected void initProperties() {
        super.initProperties();

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
        
        connectionRetainMode = props.getProperty("openjpa.ConnectionRetainMode");
        if (connectionRetainMode == null) {
            throw new RuntimeException("Missing property: "
                                       + "openjpa.ConnectionRetainMode");
        }

        ndbConnectString = props.getProperty("openjpa.ndb.connectString");
        if (url == null) {
            throw new RuntimeException("Missing property: "
                                       + "openjpa.ndb.connectString");
        }

        brokerFactory = props.getProperty("openjpa.BrokerFactory");
        if (url == null) {
            throw new RuntimeException("Missing property: "
                                       + "openjpa.BrokerFactory");
        }

        descr = "->JPA->" + url;
    }

    protected void printProperties() {
        super.printProperties();
        out.println("openjpa.ConnectionDriverName: " + driver);
        out.println("openjpa.ConnectionURL:        " + url);
        out.println("openjpa.ConnectionRetainMode: " + connectionRetainMode);
        out.println("openjpa.ndb.connectString:    " + ndbConnectString);
        out.println("openjpa.BrokerFactory:        " + brokerFactory);
    }

    protected void init() throws Exception {
        super.init();
        out.println();
        out.print("creating EMFactory ...");
        out.flush();
        // create EMF by standard API, which allows vendors to pool factories
        emf = Persistence.createEntityManagerFactory("crundjpa", props);
        out.println("      [EMF: 1]");
    }

    protected void close() throws Exception {
        out.print("closing EMFactory ...");
        out.flush();
        if (emf != null)
            emf.close();
        emf = null;
        out.println("       [ok]");
        super.close();
    }

    protected void initConnection() {
        out.print("creating EntityManager ...");
        out.flush();
        // See: clearPersistenceContext() for !allowExtendedPC
        // Tx-scope EM supported by JPA only by container injection:
        //   em = emf.createEntityManager(PersistenceContextType.TRANSACTION);
        em = emf.createEntityManager();
        // XXX check query.setHint(...) for standardized optimizations, e.g.:
        //import org.eclipse.persistence.config.QueryHints;
        //import org.eclipse.persistence.config.QueryType;        
        //import org.eclipse.persistence.config.PessimisticLock;        
        //import org.eclipse.persistence.config.HintValues;
        //query.setHint(QueryHints.QUERY_TYPE, QueryType.ReadObject);
        //query.setHint(QueryHints.PESSIMISTIC_LOCK, PessimisticLock.LockNoWait);
        //query.setHint("eclipselink.batch", "e.address");
        //query.setHint("eclipselink.join-fetch", "e.address");
        delAllA = em.createQuery("DELETE FROM A");
        delAllB0 = em.createQuery("DELETE FROM B0");
        out.println("  [EM: 1]");
    }

    protected void closeConnection() {
        out.print("closing EntityManager ...");
        out.flush();
        delAllB0 = null;
        delAllA = null;
        if (em != null)
            em.close();
        em = null;
        out.println("   [ok]");
    }

    protected void setCommonFields(A o, int i) {
        assert o != null;
        o.setCint((int)i);
        o.setClong((long)i);
        o.setCfloat((float)i);
        o.setCdouble((double)i);
    }
    
    protected void setCommonFields(B0 o, int i) {
        assert o != null;
        o.setCint((int)i);
        o.setClong((long)i);
        o.setCfloat((float)i);
        o.setCdouble((double)i);
    }
    
    protected void verifyCommonFields(A o, int i) {
        assert o != null;
        final int id = o.getId();
        verify(id == i);
        final int cint = o.getCint();
        verify(cint == i);
        final long clong = o.getClong();
        verify(clong == i);
        final float cfloat = o.getCfloat();
        verify(cfloat == i);
        final double cdouble = o.getCdouble();
        verify(cdouble == i);
    }

    protected void verifyCommonFields(B0 o, int i) {
        assert o != null;
        final int id = o.getId();
        verify(id == i);
        final int cint = o.getCint();
        verify(cint == i);
        final long clong = o.getClong();
        verify(clong == i);
        final float cfloat = o.getCfloat();
        verify(cfloat == i);
        final double cdouble = o.getCdouble();
        verify(cdouble == i);
    }

    protected void initOperations() {
        out.print("initializing operations ...");
        out.flush();

        ops.add(
            new JpaOp("insA") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countA; i++) {
                        final A o = new A();
                        o.setId(i);
                        em.persist(o);
                    }
                }
            });

        ops.add(
            new JpaOp("insB0") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countB; i++) {
                        final B0 o = new B0();
                        o.setId(i);
                        // XXX NPE with OpenJPA on ClusterJPA
                        o.setCvarbinary_def(new byte[0]);
                        em.persist(o);
                    }
                }
            });

        ops.add(
            new JpaOp("setAByPK_bulk") {
                public void run(int countA, int countB) {
                    // OpenJPA 1.2.1 fails to parse a unary '-' operator
                    final int upd = em.createQuery("UPDATE A o SET o.cint = 0-(o.id), o.clong = 0-(o.id), o.cfloat = 0-(o.id), o.cdouble = 0-(o.id)").executeUpdate();
                    assert upd == countA;
                }
            });

        ops.add(
            new JpaOp("setB0ByPK_bulk") {
                public void run(int countA, int countB) {
                    // OpenJPA 1.2.1 fails to parse a unary '-' operator
                    final int upd = em.createQuery("UPDATE B0 o SET o.cint = 0-(o.id), o.clong = 0-(o.id), o.cfloat = 0-(o.id), o.cdouble = 0-(o.id)").executeUpdate();
                    assert upd == countB;
                }
            });

        ops.add(
            new JpaOp("setAByPK") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countA; i++) {
                        final A o = em.find(A.class, i);
                        setCommonFields(o, i);
                    }
                }
            });

        ops.add(
            new JpaOp("setB0ByPK") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countB; i++) {
                        final B0 o = em.find(B0.class, i);
                        setCommonFields(o, i);
                    }
                }
            });

        ops.add(
            new JpaOp("getAByPK") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countA; i++) {
                        final A o = em.find(A.class, i);
                        verifyCommonFields(o, i);
                    }
                }
            });

        ops.add(
            new JpaOp("getB0ByPK") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countB; i++) {
                        final B0 o = em.find(B0.class, i);
                        verifyCommonFields(o, i);
                    }
                }
            });

        for (int i = 0, l = 1; l <= maxVarbinaryBytes; l *= 10, i++) {
            final byte[] b = bytes[i];
            assert l == b.length;

            ops.add(
                new JpaOp("setVarbinary" + l) {
                    public void run(int countA, int countB) {
                        for (int i = 0; i < countB; i++) {
                            final B0 o = em.find(B0.class, i);
                            assert o != null;
                            //o.cvarbinary_def = b; // not detected by OpenJPA
                            o.setCvarbinary_def(b);
                        }
                    }
                });

            ops.add(
                new JpaOp("getVarbinary" + l) {
                    public void run(int countA, int countB) {
                        for (int i = 0; i < countB; i++) {
                            final B0 o = em.find(B0.class, i);
                            assert o != null;
                            verify(Arrays.equals(b, o.getCvarbinary_def()));
                        }
                    }
                });

            ops.add(
                new JpaOp("clearVarbinary" + l) {
                    public void run(int countA, int countB) {
                        for (int i = 0; i < countB; i++) {
                            final B0 o = em.find(B0.class, i);
                            assert o != null;
                            //o.cvarbinary_def = null; // not detected by OpenJPA
                            o.setCvarbinary_def(null);
                        }
                    }
                });
        }
        
        for (int i = 0, l = 1; l <= maxVarcharChars; l *= 10, i++) {
            final String s = strings[i];
            assert l == s.length();

            ops.add(
                new JpaOp("setVarchar" + l) {
                    public void run(int countA, int countB) {
                        for (int i = 0; i < countB; i++) {
                            final B0 o = em.find(B0.class, i);
                            assert o != null;
                            //o.cvarchar_def = s; // not detected by OpenJPA
                            o.setCvarchar_def(s);
                        }
                    }
                });

            ops.add(
                new JpaOp("getVarchar" + l) {
                    public void run(int countA, int countB) {
                        for (int i = 0; i < countB; i++) {
                            final B0 o = em.find(B0.class, i);
                            assert o != null;
                            verify(s.equals(o.getCvarchar_def()));
                        }
                    }
                });

            ops.add(
                new JpaOp("clearVarchar" + l) {
                    public void run(int countA, int countB) {
                        for (int i = 0; i < countB; i++) {
                            final B0 o = em.find(B0.class, i);
                            assert o != null;
                            //o.cvarchar_def = null; // not detected by OpenJPA
                            o.setCvarchar_def(null);
                        }
                    }
                });
        }
        
        ops.add(
            new JpaOp("setB0->A") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countB; i++) {
                        final B0 b0 = em.find(B0.class, i);
                        assert b0 != null;
                        int aId = i % countA;
                        final A a = em.find(A.class, aId);
                        assert a != null;
                        b0.setA(a);
                    }
                }
            });

        ops.add(
            new JpaOp("navB0->A") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countB; i++) {
                        final B0 b0 = em.find(B0.class, i);
                        assert b0 != null;
                        final A a = b0.getA();
                        verifyCommonFields(a, i % countA);
                    }
                }
            });

        ops.add(
            new JpaOp("navA->B0") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countA; i++) {
                        final A a = em.find(A.class, i);
                        assert a != null;
                        final Collection<B0> b0s = a.getB0s();
                        assert b0s != null;
                        verify(b0s.size() > 0);
                        for (B0 b0 : b0s) {
                            verifyCommonFields(b0, i % countA);
                        }
                    }
                }
            });

        ops.add(
            new JpaOp("nullB0->A") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countB; i++) {
                        final B0 b0 = em.find(B0.class, i);
                        assert b0 != null;
                        b0.setA(null);
                    }
                }
            });

        // JPQL: cannot form a simple_entity_expression from an Id value
        //ops.add(
        //    new JpaOp("setB0->A_bulk") {
        //        public void run(int countA, int countB) {
        //            // these queries are OK but don't do what we need to:
        //            final int upd = em.createQuery("UPDATE B0 o SET o.cint = MOD(o.id, :p)").setParameter("p", countA).executeUpdate();
        //            final int upd = em.createQuery("UPDATE B0 o SET o.a = o WHERE o.id = :id").setParameter("id", 1).executeUpdate();
        //        }
        //    });
        final JpaOp setB0ToA = (JpaOp)ops.get(ops.size() - 4);
        assert setB0ToA.getName().equals("setB0->A");
        ops.add(setB0ToA
            );

        ops.add(
            new JpaOp("nullB0->A_bulk") {
                public void run(int countA, int countB) {
                    // OpenJPA 1.2.1 fails to parse a unary '-' operator
                    final int upd = em.createQuery("UPDATE B0 o SET o.a = NULL").executeUpdate();
                    assert upd == countB;
                }
            });

        ops.add(
            new JpaOp("delB0ByPK") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countB; i++) {
                        final B0 o = em.find(B0.class, i);
                        assert o != null;
                        em.remove(o);
                    }
                }
            });

        ops.add(
            new JpaOp("delAByPK") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countA; i++) {
                        final A o = em.find(A.class, i);
                        assert o != null;
                        em.remove(o);
                    }
                }
            });

        ops.add(
            new JpaOp("insA_attr") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countA; i++) {
                        final A o = new A();
                        o.setId(i);
                        setCommonFields(o, -i);
                        em.persist(o);
                    }
                }
            });

        ops.add(
            new JpaOp("insB0_attr") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countB; i++) {
                        final B0 o = new B0();
                        o.setId(i);
                        setCommonFields(o, -i);
                        em.persist(o);
                    }
                }
            });

        ops.add(
            new JpaOp("delAllB0") {
                public void run(int countA, int countB) {
                    int del = em.createQuery("DELETE FROM B0").executeUpdate();
                    assert del == countB;
                }
            });

        ops.add(
            new JpaOp("delAllA") {
                public void run(int countA, int countB) {
                    int del = em.createQuery("DELETE FROM A").executeUpdate();
                    assert del == countA;
                }
            });

        // prepare queries
        for (Iterator<Driver.Op> i = ops.iterator(); i.hasNext();) {
            ((JpaOp)i.next()).init();
        }
        out.println(" [JpaOp: " + ops.size() + "]");
    }

    protected void closeOperations() {
        out.print("closing operations ...");
        out.flush();

        // close all queries
        for (Iterator<Driver.Op> i = ops.iterator(); i.hasNext();) {
            ((JpaOp)i.next()).close();
        }
        ops.clear();

        out.println("      [ok]");
    }

    protected void beginTransaction() {
        em.getTransaction().begin();
    }

    protected void commitTransaction() {
        em.getTransaction().commit();
    }

    protected void rollbackTransaction() {
        em.getTransaction().rollback();
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
        int delB0 = delAllB0.executeUpdate();
        out.print("    [B0: " + delB0);
        out.flush();
        int delA = delAllA.executeUpdate();
        out.print(", A: " + delA);
        out.flush();
        em.getTransaction().commit();
        em.clear();

        out.println("]");
    }

    // ----------------------------------------------------------------------

    @SuppressWarnings("unchecked")
    static public void main(String[] args) {
        System.out.println("JpaLoad.main()");
        parseArguments(args);
        new JpaLoad().run();
        System.out.println();
        System.out.println("JpaLoad.main(): done.");
    }
}
