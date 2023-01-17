/* -*- mode: java; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (c) 2010, 2023, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is also distributed with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have included with MySQL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License, version 2.0, for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package com.mysql.cluster.benchmark.tws;

import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.SessionFactory;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.LockMode;
import com.mysql.clusterj.Constants;
import com.mysql.clusterj.annotation.Index;
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.PrimaryKey;

import java.util.Map;
import java.util.Iterator;


class ClusterjLoad extends TwsLoad {

    // ClusterJ resources
    protected SessionFactory sessionFactory;
    protected Session session;


    public ClusterjLoad(TwsDriver driver, MetaData md) {
        super(driver, md);
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
            out.println();
            out.print(msg.toString());
        }

        // have mgmdConnect initialized first
        descr = "clusterj(" + mgmdConnect + ")";
    }

    protected void printProperties() {
        for (Iterator<Map.Entry<Object,Object>> i
                 = driver.props.entrySet().iterator(); i.hasNext();) {
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
        session.setLockMode(driver.lockMode);
        out.println("   [ok: " + driver.lockMode + "]");
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

    // ----------------------------------------------------------------------

    public void runOperations() {
        out.println();
        out.println("running ClusterJ operations ..."
                    + " [nRows=" + driver.nRows + "]");

        if (driver.doSingle) {
            if (driver.doInsert) runClusterjInsert(TwsDriver.XMode.SINGLE);
            if (driver.doLookup) runClusterjLookup(TwsDriver.XMode.SINGLE);
            if (driver.doUpdate) runClusterjUpdate(TwsDriver.XMode.SINGLE);
            if (driver.doDelete) runClusterjDelete(TwsDriver.XMode.SINGLE);
        }
        if (driver.doBulk) {
            if (driver.doInsert) runClusterjInsert(TwsDriver.XMode.BULK);
            if (driver.doLookup) runClusterjLookup(TwsDriver.XMode.BULK);
            if (driver.doUpdate) runClusterjUpdate(TwsDriver.XMode.BULK);
            if (driver.doDelete) runClusterjDelete(TwsDriver.XMode.BULK);
        }
        if (driver.doBatch) {
            if (driver.doInsert) runClusterjInsert(TwsDriver.XMode.BATCH);
            //if (driver.doLookup) runClusterjLookup(TwsDriver.XMode.BATCH);
            if (driver.doUpdate) runClusterjUpdate(TwsDriver.XMode.BATCH);
            if (driver.doDelete) runClusterjDelete(TwsDriver.XMode.BATCH);
        }
    }

    // ----------------------------------------------------------------------

    protected void runClusterjInsert(TwsDriver.XMode mode) {
        final String name = "insert_" + mode.toString().toLowerCase();
        driver.begin(name);

        if (mode != TwsDriver.XMode.SINGLE)
            session.currentTransaction().begin();
        for(int i = 0; i < driver.nRows; i++) {
            clusterjInsert(i);
            if (mode == TwsDriver.XMode.BULK)
                session.flush();
        }
        if (mode != TwsDriver.XMode.SINGLE)
            session.currentTransaction().commit();

        driver.finish(name);
    }

    protected void clusterjInsert(int c0) {

        assert(metaData != null);

        final CJSubscriber o = session.newInstance(CJSubscriber.class);
        final int i = c0;
        final String str = Integer.toString(i);
        //final String oneChar = Integer.toString(1);
        o.setC0(str);
        int width = metaData.getColumnWidth(1);
        o.setC1(fixedStr.substring(0, width));
        o.setC2(i);
        o.setC3(i);
        //o.setC4(i);
        width = metaData.getColumnWidth(5);
        o.setC5(fixedStr.substring(0, width));
        width = metaData.getColumnWidth(6);
        o.setC6(fixedStr.substring(0, width));
        width = metaData.getColumnWidth(7);
        o.setC7(fixedStr.substring(0, width));
        width = metaData.getColumnWidth(8);
        o.setC8(fixedStr.substring(0, width));
        width = metaData.getColumnWidth(9);
        o.setC9(fixedStr.substring(0, width));
        width = metaData.getColumnWidth(10);
        o.setC10(fixedStr.substring(0, width));
        width = metaData.getColumnWidth(11);
        o.setC11(fixedStr.substring(0, width));
        width = metaData.getColumnWidth(12);
        o.setC12(fixedStr.substring(0, width));
        width = metaData.getColumnWidth(13);
        o.setC13(fixedStr.substring(0, width));
        width = metaData.getColumnWidth(14);
        o.setC14(fixedStr.substring(0, width));
        session.persist(o);
    }

    // ----------------------------------------------------------------------

    protected void runClusterjLookup(TwsDriver.XMode mode) {
        assert(mode != TwsDriver.XMode.BATCH);

        final String name = "lookup_" + mode.toString().toLowerCase();
        driver.begin(name);

        if (mode != TwsDriver.XMode.SINGLE)
            session.currentTransaction().begin();
        for(int i = 0; i < driver.nRows; i++) {
            clusterjLookup(i);
        }
        if (mode != TwsDriver.XMode.SINGLE)
            session.currentTransaction().commit();

        driver.finish(name);
    }

    protected void clusterjLookup(int c0) {
        final CJSubscriber o
            = session.find(CJSubscriber.class, Integer.toString(c0));
        if (o != null) {
            // not verifying at this time
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
    }

    // ----------------------------------------------------------------------

    protected void runClusterjUpdate(TwsDriver.XMode mode) {
        final String name = "update_" + mode.toString().toLowerCase();
        driver.begin(name);

        if (mode != TwsDriver.XMode.SINGLE)
            session.currentTransaction().begin();
        for(int i = 0; i < driver.nRows; i++) {
            clusterjUpdate(i);
            if (mode == TwsDriver.XMode.BULK)
                session.flush();
        }
        if (mode != TwsDriver.XMode.SINGLE)
            session.currentTransaction().commit();

        driver.finish(name);
    }

    protected void clusterjUpdate(int c0) {
        final String str0 = Integer.toString(c0);
        final int r = -c0;
        final String str1 = Integer.toString(r);

        // blind update
        final CJSubscriber o = session.newInstance(CJSubscriber.class);
        o.setC0(str0);
        //final CJSubscriber o = session.find(CJSubscriber.class, str0);
        //String oneChar = Integer.toString(2);
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

    protected void runClusterjDelete(TwsDriver.XMode mode) {
        final String name = "delete_" + mode.toString().toLowerCase();
        driver.begin(name);

        if (mode != TwsDriver.XMode.SINGLE)
            session.currentTransaction().begin();
        for(int i = 0; i < driver.nRows; i++) {
            clusterjDelete(i);
            if (mode == TwsDriver.XMode.BULK)
                session.flush();
        }
        if (mode != TwsDriver.XMode.SINGLE)
            session.currentTransaction().commit();

        driver.finish(name);
    }

    protected void clusterjDelete(int c0) {
        // XXX use new API for blind delete
        final CJSubscriber o = session.newInstance(CJSubscriber.class);
        o.setC0(Integer.toString(c0));
        assert o != null;
        session.remove(o);
    }

    // ----------------------------------------------------------------------

    @PersistenceCapable(table="mytable")
    //@Index(name="c0_UNIQUE")
    static public interface CJSubscriber {
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

}
