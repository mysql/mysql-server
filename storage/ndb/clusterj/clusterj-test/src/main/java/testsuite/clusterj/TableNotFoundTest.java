/*
   Copyright (c) 2025, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


package testsuite.clusterj;

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJFatalUserException;
import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.ClusterJTableException;
import com.mysql.clusterj.ClusterJTableException;
import com.mysql.clusterj.Constants;
import com.mysql.clusterj.DynamicObject;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.SessionFactory;

import java.util.Properties;


public class TableNotFoundTest extends AbstractClusterJTest {

    /* Class T1 is mapped to a table that does not exist */
    public static class T1 extends DynamicObject {
        @Override
        public String table() { return "Freddie"; }
    }

    /* Class UpperTBasic is mapped to (uppercase) table name "T_BASIC" */
    public static class UpperTBasic extends DynamicObject {
        @Override
        public String table() { return "T_BASIC"; }
    }


    @Override
    public void localSetUp() {
        createSessionFactory();
    }

    public void test1() {
        Properties p = new Properties();
        p.putAll(props);
        p.put(Constants.PROPERTY_TABLE_WAIT_MSEC, 10);
        SessionFactory sf = ClusterJHelper.getSessionFactory(p);
        Session session = sf.getSession();
        try {
            T1 t1 = session.newInstance(T1.class);
            error("expected exception");
        } catch ( ClusterJTableException ex) {
            errorIfNotEqual("Table name", "Freddie", ex.getTableName());
            errorIfNotEqual("tableNotFound", true, ex.tableNotFound());
            errorIfNotEqual("hasError", false, ex.hasError());
        }
        failOnError();
        session.close();
        sf.close();
    }

    public void test2__non_numeric() {
        Properties p = new Properties();
        p.putAll(props);
        p.put(Constants.PROPERTY_TABLE_WAIT_MSEC, "hello");
        SessionFactory sf = null;
        try {
            sf = ClusterJHelper.getSessionFactory(p);
            error("expected exception");
        } catch(ClusterJFatalUserException ex) {
        } finally {
            if(sf != null) sf.close();
        }
        failOnError();
    }

    public void test3__too_low() {
        Properties p = new Properties();
        p.putAll(props);
        p.put(Constants.PROPERTY_TABLE_WAIT_MSEC, -1);
        SessionFactory sf = null;
        try {
            sf = ClusterJHelper.getSessionFactory(p);
            error("expected exception");
        } catch(ClusterJFatalUserException ex) {
        } finally {
            if(sf != null) sf.close();
        }
        failOnError();
    }

    public void test4__too_high() {
        Properties p = new Properties();
        p.putAll(props);
        p.put(Constants.PROPERTY_TABLE_WAIT_MSEC, 1001);
        SessionFactory sf = null;
        try {
            sf = ClusterJHelper.getSessionFactory(p);
            error("expected exception");
        } catch(ClusterJFatalUserException ex) {
        } finally {
            if(sf != null) sf.close();
        }
        failOnError();
    }

    public void test5() {
        Properties p = new Properties();
        p.putAll(props);
        p.put(Constants.PROPERTY_CLUSTER_MAX_CACHED_SESSIONS, 3);
        p.put(Constants.PROPERTY_TABLE_WAIT_MSEC, 500);
        SessionFactory sf = ClusterJHelper.getSessionFactory(p);
        Session s = sf.getSession();
        try {
            T1 t1 = s.newInstance(T1.class);
            error("expected exception");
        } catch(ClusterJTableException ex) {
            if(ex.elapsedMillis() < 500)
                error("expected longer wait than " +  ex.elapsedMillis() + " msec.");
        }
        s.close();
        sf.close();
        failOnError();
    }

    public void test6_uppercase() {
        Properties p = new Properties();
        p.putAll(props);
        p.put(Constants.PROPERTY_TABLE_WAIT_MSEC, 0);
        SessionFactory sf = ClusterJHelper.getSessionFactory(p);
        Session session = sf.getSession();
        UpperTBasic t1 = session.newInstance(UpperTBasic.class);
        session.close();
        sf.close();
    }

}
