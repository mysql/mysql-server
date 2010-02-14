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

import javax.jdo.*;

/**
 * A benchmark implementation against a JDO-mapped datastore.
 */
public class JdoLoad extends Driver {

    // non-static fields
    protected PersistenceManagerFactory pmf;
    protected PersistenceManager pm;
    protected Query getAllA;
    protected Query getAllB0;
    protected Query getAByB0;
    protected Query getB0ByA;

    protected void init() {
        // init PersistenceManagerFactory
        final String driver = props.getProperty("jdo.driver");
        final String url = props.getProperty("jdo.url");
        final String user = props.getProperty("jdo.user");
        final String password = props.getProperty("jdo.password");
        try {
            pmf = JDOHelper.getPersistenceManagerFactory();
            pmf.setConnectionDriverName(driver);
            pmf.setConnectionURL(url);
            pmf.setConnectionUserName(user);
            pmf.setConnectionPassword(password);
            //pmf.setConnectionMinPool(1);
            //pmf.setConnectionMaxPool(1);
            //pmf.setOptimistic(false);
            //pmf.setRetainValues(false);
        } catch (Exception e) {
            out.println("caught " + e);
            e.printStackTrace();
            throw new RuntimeException("Cannot get PersistenceManagerFactory");
        }
    }

    protected void close() {
        if (pmf == null)
            pmf.close();
        pmf = null;
    }

    protected void initConnection() {
        if (pm != null)
            pm.close();
        pm = pmf.getPersistenceManager();
    }

    protected void closeConnection() {
        if (pm != null)
            pm.close();
        pm = null;
    }

    protected void initOperations() {
/*
        Extent<A> extentA = pm.getExtent(A.class, false);
        Extent<B0> extentB0 = pm.getExtent(B0.class, false);

        getAllA = pm.newQuery(extentA);

        getAllB0 = pm.newQuery(extentB0);

        getAByB0 = pm.newQuery(extentA, "b0s.contains(b0) & b0.id==id");
        getAByB0.declareParameters("int id");
        getAByB0.declareVariables("scalability.B0 b0");

        getB0ByA = pm.newQuery(extentB0, "a.id == id");
        getB0ByA.declareParameters("int id");
*/
    }

    protected void closeOperations() {
    }

    protected void beginTransaction() {
        pm.currentTransaction().begin();
    }

    protected void commitTransaction() {
        pm.currentTransaction().commit();
    }

    protected void rollbackTransaction() {
        pm.currentTransaction().rollback();
    }

    protected void clearPersistenceContext() {
        // XXX adapt to JDO (retain values...)
        // as long as we haven't created the EM with a Tx PC scope
        // (i.e. emf.createEntityManager(PersistenceContextType.TRANSACTION))
        // we effectively prevent caching beyond Tx scope by clearing
        // the EM's PC
        //pm.clear();
    }

    protected void clearData() {
        out.println();
        out.println("delete all objects");

        beginTransaction();
        pm.deletePersistentAll((Collection)getAllA.execute());
        commitTransaction();

        beginTransaction();
        pm.deletePersistentAll((Collection)getAllB0.execute());
        commitTransaction();
    }

/*
    protected void create0(int countA, int countB0) {
        Object o[] = new Object[countA];
        for (int i = 0; i < countA; i++) {
            o[i] = createA(i);
        }
    }

    protected void create1(int countA, int countB0) {
        Object o[] = new Object[countB0];
        for (int i = 0; i < countB0; i++) {
            o[i] = createB0(countA + i);
        }
    }

    protected void delete0(int countA, int countB0) {
        //TODO: iterate extent <-> JDBC
        //Collection result = (Collection)getAllA.execute();
        //pm.deletePersistentAll(result);
    }

    protected void delete1(int countA, int countB0) {
        //TODO: iterate extent <-> JDBC
        //Collection result = (Collection)getAllB0.execute();
        //pm.deletePersistentAll(result);
    }

    protected void query0(int countA, int countB0) {
        Collection result = (Collection)getAllA.execute();
        for (Iterator i = result.iterator(); i.hasNext();) {
            A o = (A) i.next();
        }
    }

    protected void query1(int countA, int countB0) {
        Collection result = (Collection)getAllB0.execute();
        for (Iterator i = result.iterator(); i.hasNext();) {
            B0 o = (B0) i.next();
        }
    }

    protected void setRel0(int countA, int countB0) {
        Collection aResult = (Collection)getAllA.execute();
        Collection b0Result = (Collection)getAllB0.execute();
        final int rAB0 = countA / countB0;
        final int rB0A = countB0 / countA;
        if (rAB0 <= 1) {
            Iterator j = b0Result.iterator();
            Iterator i = aResult.iterator();
            while (i.hasNext()) {
                A a = (A) i.next();
                for (int r = 0; r < rB0A; r++) {
                    B0 b0 = (B0) j.next();
                    a.getB0s().add(b0);
                    b0 = null;
                }
                a = null;
            }
            i = null;
            j = null;
        } else {
            Iterator j = b0Result.iterator();
            Iterator i = aResult.iterator();
            while (i.hasNext()) {
                A a = null;
                for (int r = 0; r < rAB0; r++)
                    a = (A) i.next();
                B0 b0 = (B0) j.next();
                a.getB0s().add(b0);
                b0 = null;
                a = null;
            }
            i = null;
            j = null;
        }
        aResult = null;
        b0Result = null;
    }

    protected void setRel1(int countA, int countB0) {
        Collection aResult = (Collection)getAllA.execute();
        Collection b0Result = (Collection)getAllB0.execute();
        final int rAB0 = countA / countB0;
        final int rB0A = countB0 / countA;
        if (rAB0 <= 1) {
            Iterator j = b0Result.iterator();
            for (Iterator i = aResult.iterator(); i.hasNext();) {
                A a = (A) i.next();
                for (int r = 0; r < rB0A; r++) {
                    B0 b0 = (B0) j.next();
                    b0.setA(a);
                }
            }
        } else {
            Iterator j = b0Result.iterator();
            for (Iterator i = aResult.iterator(); i.hasNext();) {
                A a = null;
                for (int r = 0; r < rAB0; r++)
                    a = (A) i.next();
                B0 b0 = (B0) j.next();
                b0.setA(a);
            }
        }
    }

    protected void delRel0(int countA, int countB0) {
        Collection aResult = (Collection)getAllA.execute();
        for (Iterator i = aResult.iterator(); i.hasNext();) {
            A a = (A) i.next();
            a.getB0s().clear();
        }
    }

    protected void delRel1(int countA, int countB0) {
        Collection b0Result = (Collection)getAllB0.execute();
        for (Iterator i = b0Result.iterator(); i.hasNext();) {
            B0 b0 = (B0) i.next();
            b0.setA(null);
        }
    }

    protected void delRel2(int countA, int countB0) {
        Collection aResult = (Collection)getAllA.execute();
        for (Iterator i = aResult.iterator(); i.hasNext();) {
            A a = (A) i.next();
            for (Iterator j = a.getB0s().iterator(); j.hasNext();) {
                B0 b0 = (B0) j.next();
                a.getB0s().remove(b0);
            }
        }
    }

    protected void queryRel0(int countA, int countB0) {
        Collection aResult = (Collection)getAllA.execute();
        for (Iterator i = aResult.iterator(); i.hasNext();) {
            A a = (A) i.next();
            Collection b0Result
                = (Collection)getB0ByA.execute(new java.lang.Integer(a.getId()));
            for (Iterator j = b0Result.iterator(); j.hasNext();) {
                B0 b0 = (B0) j.next();
            }
        }
    }

    protected void queryRel1(int countA, int countB0) {
        Collection b0Result = (Collection)getAllB0.execute();
        for (Iterator i = b0Result.iterator(); i.hasNext();) {
            B0 b0 = (B0) i.next();
            Collection aResult
                = (Collection)getAByB0.execute(new java.lang.Integer(b0.getId()));
            for (Iterator j = aResult.iterator(); j.hasNext();) {
                A a = (A) j.next();
            }
        }
    }

    protected void fetchInt0(int countA, int countB0) {
        Collection aResult = (Collection)getAllA.execute();
        for (Iterator i = aResult.iterator(); i.hasNext();) {
            A a = (A) i.next();
            a.getCint();
        }
    }

    protected void fetchInt1(int countA, int countB0) {
        Collection b0Result = (Collection)getAllB0.execute();
        for (Iterator i = b0Result.iterator(); i.hasNext();) {
            B0 b0 = (B0) i.next();
            b0.getCint();
        }
    }

    protected void fetchLong0(int countA, int countB0) {
        Collection aResult = (Collection)getAllA.execute();
        for (Iterator i = aResult.iterator(); i.hasNext();) {
            A a = (A) i.next();
            a.getClong();
        }
    }

    protected void fetchLong1(int countA, int countB0) {
        Collection b0Result = (Collection)getAllB0.execute();
        for (Iterator i = b0Result.iterator(); i.hasNext();) {
            B0 b0 = (B0) i.next();
            b0.getClong();
        }
    }

    protected void fetchFloat0(int countA, int countB0) {
        Collection aResult = (Collection)getAllA.execute();
        for (Iterator i = aResult.iterator(); i.hasNext();) {
            A a = (A) i.next();
            a.getCfloat();
        }
    }

    protected void fetchFloat1(int countA, int countB0) {
        Collection b0Result = (Collection)getAllB0.execute();
        for (Iterator i = b0Result.iterator(); i.hasNext();) {
            B0 b0 = (B0) i.next();
            b0.getCfloat();
        }
    }

    protected void fetchDouble0(int countA, int countB0) {
        Collection aResult = (Collection)getAllA.execute();
        for (Iterator i = aResult.iterator(); i.hasNext();) {
            A a = (A) i.next();
            a.getCdouble();
        }
    }

    protected void fetchDouble1(int countA, int countB0) {
        Collection b0Result = (Collection)getAllB0.execute();
        for (Iterator i = b0Result.iterator(); i.hasNext();) {
            B0 b0 = (B0) i.next();
            b0.getCdouble();
        }
    }

    protected void fetchString0(int countA, int countB0) {
        Collection aResult = (Collection)getAllA.execute();
        for (Iterator i = aResult.iterator(); i.hasNext();) {
            A a = (A) i.next();
            a.getCstring();
        }
    }

    protected void fetchString1(int countA, int countB0) {
        Collection b0Result = (Collection)getAllB0.execute();
        for (Iterator i = b0Result.iterator(); i.hasNext();) {
            B0 b0 = (B0) i.next();
            b0.getCstring();
        }
    }

    protected void updateInt0(int countA, int countB0) {
        Collection aResult = (Collection)getAllA.execute();
        for (Iterator i = aResult.iterator(); i.hasNext();) {
            A a = (A) i.next();
            a.setCint(0);
        }
    }

    protected void updateInt1(int countA, int countB0) {
        Collection b0Result = (Collection)getAllB0.execute();
        for (Iterator i = b0Result.iterator(); i.hasNext();) {
            B0 b0 = (B0) i.next();
            b0.setCint(0);
        }
    }

    protected void updateLong0(int countA, int countB0) {
        Collection aResult = (Collection)getAllA.execute();
        for (Iterator i = aResult.iterator(); i.hasNext();) {
            A a = (A) i.next();
            a.setClong(0);
        }
    }

    protected void updateLong1(int countA, int countB0) {
        Collection b0Result = (Collection)getAllB0.execute();
        for (Iterator i = b0Result.iterator(); i.hasNext();) {
            B0 b0 = (B0) i.next();
            b0.setClong(0);
        }
    }

    protected void updateFloat0(int countA, int countB0) {
        Collection aResult = (Collection)getAllA.execute();
        for (Iterator i = aResult.iterator(); i.hasNext();) {
            A a = (A) i.next();
            a.setCfloat(0);
        }
    }

    protected void updateFloat1(int countA, int countB0) {
        Collection b0Result = (Collection)getAllB0.execute();
        for (Iterator i = b0Result.iterator(); i.hasNext();) {
            B0 b0 = (B0) i.next();
            b0.setCfloat(0);
        }
    }

    protected void updateDouble0(int countA, int countB0) {
        Collection aResult = (Collection)getAllA.execute();
        for (Iterator i = aResult.iterator(); i.hasNext();) {
            A a = (A) i.next();
            a.setCdouble(0);
        }
    }

    protected void updateDouble1(int countA, int countB0) {
        Collection b0Result = (Collection)getAllB0.execute();
        for (Iterator i = b0Result.iterator(); i.hasNext();) {
            B0 b0 = (B0) i.next();
            b0.setCdouble(0);
        }
    }

    protected void updateString0(int countA, int countB0) {
        Collection aResult = (Collection)getAllA.execute();
        for (Iterator i = aResult.iterator(); i.hasNext();) {
            A a = (A) i.next();
            a.setCstring("newAString");
        }
    }

    protected void updateString1(int countA, int countB0) {
        Collection b0Result = (Collection)getAllB0.execute();
        for (Iterator i = b0Result.iterator(); i.hasNext();) {
            B0 b0 = (B0) i.next();
            b0.setCstring("newB0String");
        }
    }

    protected A createA(int id) {
        A o = new A();
        o.setId(id);
        o.setCint(id);
        o.setClong((long)id);
        o.setCfloat((float)id);
        o.setCdouble((double)id);
        o.setCstring("aString");
        pm.makePersistent(o);
        return o;
    }

    protected B0 createB0(int id) {
        B0 o = new B0();
        o.setId(id);
        o.setCint(id);
        o.setClong((long)id);
        o.setCfloat((float)id);
        o.setCdouble((double)id);
        o.setCstring("b0String");
        pm.makePersistent(o);
        return o;
    }
*/

    // ----------------------------------------------------------------------

    static public void main(String[] args) {
        System.out.println("JdoLoad.main()");
        parseArguments(args);
        new JdoLoad().run();
        System.out.println();
        System.out.println("JdoLoad.main(): done.");
    }
}
