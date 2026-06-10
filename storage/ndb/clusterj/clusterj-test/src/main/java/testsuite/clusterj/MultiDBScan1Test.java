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
import com.mysql.clusterj.Query;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.query.Predicate;
import com.mysql.clusterj.query.QueryBuilder;
import com.mysql.clusterj.query.QueryDefinition;
import com.mysql.clusterj.query.QueryDomainType;
import testsuite.clusterj.model.Employee;

import java.util.*;


/*
Using DynamicObjects. Created separate class for each table
 */
public class MultiDBScan1Test extends AbstractClusterJModelTest {

  private static final int NUMBER_TO_INSERT = 128;
  private static String defaultDB;

  boolean useCache = false;

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
  protected Properties modifyProperties() {
    Properties modifiedProps = new Properties();
    modifiedProps.putAll(props);
    modifiedProps.put(Constants.PROPERTY_CLUSTER_MAX_CACHED_SESSIONS, 10);
    modifiedProps.put(Constants.PROPERTY_CLUSTER_MULTI_DB, "true");
    return modifiedProps;
  }

  @Override
  public void localSetUp() {
    createSessionFactory();
    defaultDB = props.getProperty(Constants.PROPERTY_CLUSTER_DATABASE);
  }

  public void cleanUp() {
    cleanUpInt(defaultDB, EmpBasic1.class);
    cleanUpInt("test2", EmpBasic2.class);
    cleanUpInt("test3", EmpBasic3.class);
  }

  public void cleanUpInt(String db, Class c) {
    Session s = getSession(db);
    s.deletePersistentAll(c);
    returnSession(s);
  }

  public void testSimple() {
    useCache = false;
    cleanUp();
    runTest(defaultDB, EmpBasic1.class);
    runTest("test2", EmpBasic2.class);
    runTest("test3", EmpBasic3.class);
  }

  public void testSimpleWithCache() {
    useCache = true;
    cleanUp();
    runTest(defaultDB, EmpBasic1.class);
    runTest("test2", EmpBasic2.class);
    runTest("test3", EmpBasic3.class);
  }

  public void runTest(String db, Class cls) {
    //System.out.println("Adding rows to DB: " + db + " table: " + cls);
    for (int i = 0; i < NUMBER_TO_INSERT; i++) {
      Session s = getSession(db);
      DynamicObject e = (DynamicObject) s.newInstance(cls);
      MultiDBHelper.setEmployeeFields(this, e, i);
      s.savePersistent(e);
      closeDTO(s, e, cls);
      returnSession(s);
    }

    // now verify data
    Session s = getSession(db);
    List<DynamicObject> list = new ArrayList<>(NUMBER_TO_INSERT);
    s.currentTransaction().begin();
    QueryBuilder qb = s.getQueryBuilder();
    QueryDomainType qd = qb.createQueryDefinition(cls);
    Predicate pred = qd.get("id").greaterThan(qd.param("idParam"));
    qd.where(pred);
    Query query = s.createQuery(qd);
    query.setParameter("idParam", -1);

    try {
      list = (List<DynamicObject>) query.getResultList();
      if (list.size() != NUMBER_TO_INSERT) {
        error("Wrong number of Rows Read using scan op. " +
          " Expecting: " + NUMBER_TO_INSERT + " Got: " + list.size());
      }
    } catch (Exception e) {
      error(e.getMessage());
    }

    // sort by ID
     Collections.sort(list, new Comparator<DynamicObject>() {
      public int compare(DynamicObject s1, DynamicObject s2) {
        int id1 = MultiDBHelper.getEmployeeID(s1);
        int id2 = MultiDBHelper.getEmployeeID(s2);
        return Integer.compare(id1, id2);
      }
    });

    for (int i = 0; i < NUMBER_TO_INSERT; i++) {
      MultiDBHelper.verifyEmployeeFields(this, list.get(i), i);
      closeDTO(s, list.get(i), cls);
    }
    list.clear();
    s.currentTransaction().commit();
    returnSession(s);

    // now delete data
    for (int i = 0; i < NUMBER_TO_INSERT; i++) {
      s = getSession(db);
      DynamicObject e = (DynamicObject) s.find(cls, i);
      s.deletePersistent(e);
      closeDTO(s, e, cls);
      returnSession(s);
    }

    failOnError();
  }

  Session getSession(String db) {
    if (db == null) {
      return sessionFactory.getSession();
    } else {
      return sessionFactory.getSession(db);
    }
  }

  void returnSession(Session s) {
      s.close();
  }

  void closeDTO(Session s, Object dto, Class dtoClass) {
      s.release(dto);
  }
}
