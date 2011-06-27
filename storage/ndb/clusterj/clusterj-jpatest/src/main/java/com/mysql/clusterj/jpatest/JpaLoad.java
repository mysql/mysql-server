/*
   Copyright 2010 Sun Microsystems, Inc.
   All rights reserved. Use is subject to license terms.

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

package com.mysql.clusterj.jpatest;

import com.mysql.clusterj.jpatest.model.A;
import com.mysql.clusterj.jpatest.model.B0;
import java.util.Collection;
import java.util.Iterator;
import java.util.ArrayList;

import javax.persistence.Persistence;
import javax.persistence.EntityManagerFactory;
import javax.persistence.EntityManager;
import javax.persistence.Query;

/**
 * A benchmark implementation against a JPA-mapped database.
 */
public class JpaLoad extends Driver {

    // JPA database connection
    protected String driver;
    protected String url;
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

    @Override
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

        descr = "->JPA->" + url;
    }

    @Override
    protected void printProperties() {
        super.printProperties();
        out.println("openjpa.ConnectionDriverName: " + driver);
        out.println("openjpa.ConnectionURL:        " + url);
    }

    @Override
    protected void init() throws Exception {
        super.init();
        out.println();
        out.print("creating EMFactory ...");
        out.flush();
        emf = Persistence.createEntityManagerFactory("crundjpa", props);
        out.println("      [EMF: 1]");
    }

    @Override
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
        em = emf.createEntityManager();
        // not supported by, at least, OpenJPA (JPA spec? Hibernate?):
        //em = emf.createEntityManager(PersistenceContextType.TRANSACTION);
        // It seems that the only way to obtain an EntityManager with
        // transaction persistence context is by container injection.
        //
        // However, if/where/when tx-PC is supported, we need to check here
        // against property allowExtendedPC; no need then for em.clear()
        // in clearPersistenceContext()
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

    protected int checkFields(A o) {
        final int cint = o.getCint();
        final long clong = o.getClong();
        verify(clong == cint);
        final float cfloat = o.getCfloat();
        verify(cfloat == cint);
        final double cdouble = o.getCdouble();
        verify(cdouble == cint);
        return cint;
    }

    protected int checkFields(B0 o) {
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
                        em.persist(o);
                    }
                }
            });

        ops.add(
            new JpaOp("setAByPK") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countA; i++) {
                        final A o = em.find(A.class, i);
                        assert o != null;
                        o.setCint((int)i);
                        o.setClong((long)i);
                        o.setCfloat((float)i);
                        o.setCdouble((double)i);
                    }
                }
            });

        ops.add(
            new JpaOp("setB0ByPK") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countB; i++) {
                        final B0 o = em.find(B0.class, i);
                        assert o != null;
                        o.setCint((int)i);
                        o.setClong((long)i);
                        o.setCfloat((float)i);
                        o.setCdouble((double)i);
                    }
                }
            });

        ops.add(
            new JpaOp("getAByPK") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countA; i++) {
                        final A o = em.find(A.class, i);
                        assert o != null;
                        final int id = o.getId();
                        verify(id == i);
                        final int j = checkFields(o);
                        verify(j == id);
                    }
                }
            });

        ops.add(
            new JpaOp("getB0ByPK") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countB; i++) {
                        final B0 o = em.find(B0.class, i);
                        assert o != null;
                        final int id = o.getId();
                        verify(id == i);
                        final int j = checkFields(o);
                        verify(j == id);
                    }
                }
            });

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
                        assert a != null;
                        final int id = a.getId();
                        verify(id == i % countA);
                        final int j = checkFields(a);
                        verify(j == id);
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

        ops.add(
           new JpaOp("setA->B0") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countA; i++) {
                        final A a = em.find(A.class, i);
                        assert a != null;
                        final Collection<B0> b0s = new ArrayList<B0>();
                        for (int j = i; j < countB; j += countA) {
                            //assert i == j % countA;
                            final B0 b0 = em.find(B0.class, j);
                            assert b0 != null;
                            b0s.add(b0);
                        }
                        a.setB0s(b0s);
                    }
                }
            });

        // this operation fails on an empty PersistenceContext
        if (allowExtendedPC) {
            ops.add(
                new JpaOp("navA->B0") {
                    public void run(int countA, int countB) {
                        for (int i = 0; i < countA; i++) {
                            final A a = em.find(A.class, i);
                            assert a != null;
                            final Collection<B0> b0s = a.getB0s();
                            assert b0s != null;
                            // fails on an empty PC (no managed relationships)
                            verify(b0s.size() > 0);
                            for (B0 b0 : b0s) {
                                assert b0 != null;
                                final int id = b0.getId();
                                verify(id % countA == i);
                                final int j = checkFields(b0);
                                verify(j == id);
                            }
                        }
                    }
                });
        }

        ops.add(
            new JpaOp("nullA->B0") {
                public void run(int countA, int countB) {
                    for (int i = 0; i < countA; i++) {
                        final A a = em.find(A.class, i);
                        assert a != null;
                        a.setB0s(null);
                    }
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
                        o.setCint((int)-i);
                        o.setClong((long)-i);
                        o.setCfloat((float)-i);
                        o.setCdouble((double)-i);
                        o.setCstring(String.valueOf(i));
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
                        o.setCint((int)-i);
                        o.setClong((long)-i);
                        o.setCfloat((float)-i);
                        o.setCdouble((double)-i);
                        o.setCstring(String.valueOf(i));
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
        // as long as we haven't created the EM with a Tx PC scope
        // (i.e. emf.createEntityManager(PersistenceContextType.TRANSACTION))
        // we effectively prevent caching beyond Tx scope by clearing
        // the EM's PC
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

    static public void main(String[] args) {
        clearPropFileNames();
        System.out.println("JpaLoad.main()");
        parseArguments(args);
        new JpaLoad().run();
        System.out.println();
        System.out.println("JpaLoad.main(): done.");
    }
}
