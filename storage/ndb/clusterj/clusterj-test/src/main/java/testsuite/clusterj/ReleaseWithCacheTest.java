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

import com.mysql.clusterj.Constants;
import com.mysql.clusterj.DynamicObject;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.SessionFactory;

import java.util.ArrayList;
import java.util.List;
import java.util.Properties;

/*
Using DynamicObjects. Created separate class for each table
 */
public class ReleaseWithCacheTest extends AbstractClusterJModelTest {

  protected static final int NUMBER_TO_INSERT = 2048;
  protected static String defaultDB;
  private SessionFactory factoryWithCacheOff = null;
  private SessionFactory factoryWithCacheOn = null;

  public static class EmpBasic1 extends DynamicObject {
    @Override
    public String table() {
      return "t_basic";
    }
  }

  public static class EmpBasic2 extends DynamicObject {
    @Override
    public String table() {
      return "t_basic2";
    }
  }

  public static class EmpBasic3 extends DynamicObject {
    @Override
    public String table() {
      return "t_basic3";
    }
  }

  @Override
  public void localSetUp() {
    createSessionFactory();
    defaultDB = props.getProperty(Constants.PROPERTY_CLUSTER_DATABASE);

    if(factoryWithCacheOn == null) {
      Properties customProps = new Properties();
      customProps.putAll(props);

      customProps.put(Constants.PROPERTY_CLUSTER_MULTI_DB, "true");
      factoryWithCacheOn = getSessionFactory(customProps);

      customProps.put(Constants.PROPERTY_CLUSTER_MAX_CACHED_SESSIONS, 0);
      factoryWithCacheOff = getSessionFactory(customProps);
    }
  }

  public void cleanUp() {
    cleanUpInt(defaultDB, EmpBasic1.class);
    cleanUpInt("test2", EmpBasic2.class);
    cleanUpInt("test3", EmpBasic3.class);
  }

  public void cleanUpInt(String db, Class c) {
    Session s = factoryWithCacheOn.getSession(db);
    s.deletePersistentAll(c);
    returnSession(s);
  }

  public void testWithCacheOn() {
    runSimple(factoryWithCacheOn, "on");
  }

  public void testWithCacheOff() {
    runSimple(factoryWithCacheOff, "off");
  }

  public void runSimple(SessionFactory factory, String id) {
    cleanUp();

    List<Thread> threads = new ArrayList(3);
    threads.add(new Thread(new Worker(this, factory, id, defaultDB, EmpBasic1.class)));
    threads.add(new Thread(new Worker(this, factory, id, "test2", EmpBasic2.class)));
    threads.add(new Thread(new Worker(this, factory, id, "test3", EmpBasic3.class)));

    for (Thread t : threads) {
      t.start();
    }

    for (Thread t : threads) {
      try {
        t.join();
      } catch (InterruptedException e) {
        error(e.getMessage());
      }
    }
  }

  void returnSession(Session s) {
    s.close();
  }

  void closeDTO(Session s, DynamicObject dto, Class dtoClass) {
    s.release(dto);
  }

  class Worker implements Runnable {
    AbstractClusterJModelTest test;
    SessionFactory factory;
    String id;
    String db;
    Class cls;

    public Worker(AbstractClusterJModelTest test, SessionFactory factory,
                  String id, String db, Class cls) {
      this.test = test;
      this.factory = factory;
      this.id = id;
      this.db = db;
      this.cls = cls;
    }

    @Override
    public void run() {
      //recreating session to each operation is inefficient but
      //here we just want to test how the
      //cache works after creating many sessions
      //and dynamic objects

      long startTime = System.nanoTime();
      for (int i = 0; i < NUMBER_TO_INSERT; i++) {
        Session s = factory.getSession(db);
        DynamicObject e = (DynamicObject) s.newInstance(cls);
        MultiDBHelper.setEmployeeFields(test, e, i);
        s.savePersistent(e);
        closeDTO(s, e, cls);
        returnSession(s);
      }

      // now verify data
      for (int i = 0; i < NUMBER_TO_INSERT; i++) {
        Session s = factory.getSession(db);
        DynamicObject e = (DynamicObject) s.find(cls, i);
        MultiDBHelper.verifyEmployeeFields(test, e, i);
        closeDTO(s, e, cls);
        returnSession(s);
      }

      // now delete data
      for (int i = 0; i < NUMBER_TO_INSERT; i++) {
        Session s = factory.getSession(db);
        DynamicObject e = (DynamicObject) s.find(cls, i);
        s.deletePersistent(e);
        closeDTO(s, e, cls);
        returnSession(s);
      }
      long endTime = System.nanoTime();

      if("test3".equals(db))
        logger.info(() -> "Thread " + db + ", session cache " + id +
            ", elapsed time: " + (endTime-startTime)/1000000 + " msec. ");
    }
  }
}
