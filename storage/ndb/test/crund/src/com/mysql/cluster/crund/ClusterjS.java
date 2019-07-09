/*
  Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

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

import java.util.Map;
import java.util.List;
import java.util.ArrayList;

import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.SessionFactory;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.LockMode;
import com.mysql.clusterj.Constants;
import com.mysql.clusterj.annotation.Index;
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.PrimaryKey;

import com.mysql.cluster.crund.CrundDriver.XMode;

class ClusterjS extends CrundSLoad {
    // ClusterJ resources
    protected SessionFactory sessionFactory;
    protected Session session;

    public ClusterjS(CrundDriver driver) {
        super(driver);
    }

    // ----------------------------------------------------------------------
    // ClusterJ intializers/finalizers
    // ----------------------------------------------------------------------

    protected void initProperties() {
        out.println();
        out.print("setting clusterj properties ...");

        final StringBuilder msg = new StringBuilder();
        final String eol = System.getProperty("line.separator");

        // check required properties
        String mgmdConnect
            = driver.props.getProperty(Constants.PROPERTY_CLUSTER_CONNECTSTRING);

        if (msg.length() == 0) {
            out.println(" [ok]");
        } else {
            driver.hasIgnoredSettings = true;
            out.println();
            out.print(msg.toString());
        }

        name = "clusterj"; // shortcut will do, "(" + mgmdConnect + ")";
    }

    protected void printProperties() {
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

    public void init() throws Exception {
        super.init();
        assert (sessionFactory == null);

        // load native library (better diagnostics doing it explicitely)
        //loadSystemLibrary("ndbclient");

        out.print("creating SessionFactory ...");
        out.flush();
        sessionFactory = ClusterJHelper.getSessionFactory(driver.props);
        out.println("     [ok]");
    }

    public void close() throws Exception {
        assert (sessionFactory != null);

        out.println();
        out.print("closing SessionFactory ...");
        out.flush();
        sessionFactory.close();
        sessionFactory = null;
        out.println("      [ok]");

        super.close();
    }

    // ----------------------------------------------------------------------
    // ClusterJ datastore operations
    // ----------------------------------------------------------------------

    public void initConnection() {
        assert (sessionFactory != null);
        assert (session == null);

        out.println();
        out.println("initializing clusterj resources ...");

        out.print("starting clusterj session ...");
        out.flush();
        session = sessionFactory.getSession();
        out.println("   [ok]");

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
    }

    public void closeConnection() {
        assert (session != null);

        out.println();
        out.println("releasing clusterj resources ...");

        out.print("closing clusterj session ...");
        out.flush();
        session.close();
        session = null;
        out.println("    [ok]");
    }

    public void clearData() {
        out.print("deleting all objects ...");
        out.flush();
        session.currentTransaction().begin();
        final int d = session.deletePersistentAll(IS.class);
        session.currentTransaction().commit();
        out.println("        [S: " + d + "]");
    }

    // ----------------------------------------------------------------------

    protected void runInsert(XMode mode, int[] id) throws Exception {
        final String name = "S_insAttr," + mode;
        final int n = id.length;
        driver.beginOp(name);
        if (mode != XMode.indy) // indy uses auto-tx mode
            session.currentTransaction().begin();
        for(int i = 0; i < n; i++) {
            insert(id[i]);
            if (mode == XMode.each)
                session.flush();
        }
        if (mode != XMode.indy)
            session.currentTransaction().commit();
        driver.finishOp(name, n);
    }

    protected void insert(int id) {
        final IS o = session.newInstance(IS.class);
        final int i = id;
        final String str = Integer.toString(i);
        //final String oneChar = Integer.toString(1);
        o.setC0(str);
        o.setC1(str);
        o.setC2(i);
        o.setC3(i);
        //o.setC4(i);
        o.setC5(str);
        o.setC6(str);
        o.setC7(str);
        o.setC8(str);
        //o.setC9(oneChar);
        //o.setC10(oneChar);
        //o.setC11(str);
        //o.setC12(str);
        //o.setC13(oneChar);
        //o.setC14(str);
        session.persist(o);
    }

    // ----------------------------------------------------------------------

    protected void runLookup(XMode mode, int[] id) throws Exception {
        final String name = "S_getAttr," + mode;
        final int n = id.length;
        driver.beginOp(name);
        if (mode != XMode.indy) // indy uses auto-tx mode
            session.currentTransaction().begin();
        if (mode != XMode.bulk) {
            for(int i = 0; i < n; i++)
                lookup(id[i]);
        } else {
            lookup(id);
        }
        if (mode != XMode.indy)
            session.currentTransaction().commit();
        driver.finishOp(name, n);
    }

    protected void lookup(int[] id) {
        final int n = id.length;
        final List<IS> os = new ArrayList<IS>(n);
        for (int i = 0; i < n; i++)
            os.add(session.newInstance(IS.class, Integer.toString(id[i])));
        session.load(os);
        session.flush(); // needed
        for (int i = 0; i < n; i++) {
            final IS o = os.get(i);
            assert o != null;
            check(id[i], o);
        }
    }

    protected void lookup(int id) {
        final IS o = session.find(IS.class, Integer.toString(id));
        assert o != null;
        check(id, o);
    }

    protected void check(int id, IS o) {
        // XXX not verifying at this time
        String ac0 = o.getC0();
        String c1 = o.getC1();
        int c2 = o.getC2();
        int c3 = o.getC3();
        int c4 = o.getC4();
        String c5 = o.getC5();
        String c6 = o.getC6();
        String c7 = o.getC7();
        String c8 = o.getC8();
        String c9 = o.getC9();
        String c10 = o.getC10();
        String c11 = o.getC11();
        String c12 = o.getC12();
        String c13 = o.getC13();
        String c14 = o.getC14();
    }

    // ----------------------------------------------------------------------

    protected void runUpdate(XMode mode, int[] id) throws Exception {
        final String name = "S_setAttr," + mode;
        final int n = id.length;
        driver.beginOp(name);
        if (mode != XMode.indy) // indy uses auto-tx mode
            session.currentTransaction().begin();
        for(int i = 0; i < n; i++) {
            update(id[i]);
            if (mode == XMode.each)
                session.flush();
        }
        if (mode != XMode.indy)
            session.currentTransaction().commit();
        driver.finishOp(name, n);
    }

    protected void update(int id) {
        final String str0 = Integer.toString(id);
        final int r = -id;
        final String str1 = Integer.toString(r);
        //String oneChar = Integer.toString(2);
        final IS o = session.newInstance(IS.class); // blind update
        o.setC0(str0);
        o.setC1(str1);
        o.setC2(r);
        o.setC3(r);
        //o.setC4(r);
        o.setC5(str1);
        o.setC6(str1);
        o.setC7(str1);
        o.setC8(str1);
        //o.setC9(oneChar);
        //o.setC10(oneChar);
        //o.setC11(str);
        //o.setC12(str);
        //o.setC13(oneChar);
        //o.setC14(str);
        session.updatePersistent(o);
    }

    // ----------------------------------------------------------------------

    protected void runDelete(XMode mode, int[] id) throws Exception {
        final String name = "S_del," + mode;
        final int n = id.length;
        driver.beginOp(name);
        if (mode != XMode.indy) // indy uses auto-tx mode
            session.currentTransaction().begin();
        for(int i = 0; i < n; i++) {
            delete(id[i]);
            if (mode == XMode.each)
                session.flush();
        }
        if (mode != XMode.indy)
            session.currentTransaction().commit();
        driver.finishOp(name, n);
    }

    protected void delete(int id) {
        final IS o = session.newInstance(IS.class); // blind delete
        o.setC0(Integer.toString(id));
        assert o != null;
        session.remove(o);
    }

    // ----------------------------------------------------------------------

    @PersistenceCapable(table="S")
    //@Index(name="c0_UNIQUE")
    static public interface IS {
        @PrimaryKey
        String getC0();
        void setC0(String c0);

        @Index(name="c1_UNIQUE")
        String getC1();
        void setC1(String c1);

        @Index(name="c2_UNIQUE")
        int getC2();
        void setC2(int c2);

        int getC3();
        void setC3(int c3);

        int getC4();
        void setC4(int c4);

        String getC5();
        void setC5(String c5);

        String getC6();
        void setC6(String c6);

        @Index(name="c7_UNIQUE")
        String getC7();
        void setC7(String c7);

        @Index(name="c8_UNIQUE")
        String getC8();
        void setC8(String c8);

        String getC9();
        void setC9(String c9);

        String getC10();
        void setC10(String c10);

        String getC11();
        void setC11(String c11);

        String getC12();
        void setC12(String c12);

        String getC13();
        void setC13(String c13);

        String getC14();
        void setC14(String c14);
    }

    // ----------------------------------------------------------------------

    protected void clearPersistenceContext() {
        // nothing to do as we're not caching beyond Tx scope
    }
}
