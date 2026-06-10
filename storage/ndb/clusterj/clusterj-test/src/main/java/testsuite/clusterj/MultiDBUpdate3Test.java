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

import java.util.Properties;

/*
same table in three different databases
 */
public class MultiDBUpdate3Test extends AbstractClusterJModelTest {

  private static final int NUMBER_TO_INSERT = 100;
  private static String defaultDB;

  boolean useCache = false;

  public static class SameTable extends DynamicObject {
    @Override
    public String table() {
      return "same_table";
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
    if(defaultDB == null) defaultDB = "test";
  }

  public void cleanUp() {
    cleanUpInt(defaultDB, SameTable.class);
    cleanUpInt("test2", SameTable.class);
    cleanUpInt("test3", SameTable.class);
  }

  public void cleanUpInt(String db, Class c) {
    Session s = getSession(db);
    s.deletePersistentAll(c);
    returnSession(s);
  }

  public void testSimple() {
    useCache = false;
    cleanUp();
    runTest(defaultDB, SameTable.class);
    runTest("test2", SameTable.class);
    runTest("test3", SameTable.class);
  }

  public void testSimpleWithCache() {
    useCache = true;
    cleanUp();
    runTest(defaultDB, SameTable.class);
    runTest("test2", SameTable.class);
    runTest("test3", SameTable.class);
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
    for (int i = 0; i < NUMBER_TO_INSERT; i++) {
      Session s = getSession(db);
      DynamicObject e = (DynamicObject) s.find(cls, i);
      MultiDBHelper.verifyEmployeeFields(this, e, i);
      closeDTO(s, e, cls);
      returnSession(s);
    }

    int count = getCount(db, "same_table");
    if (count != NUMBER_TO_INSERT) {
      error("Wrong number of rows in the table " + db + ".same_table. Expecting: "
        + NUMBER_TO_INSERT + " Got: " + count);
    }

    // now delete data
    for (int i = 0; i < NUMBER_TO_INSERT; i++) {
      Session s = getSession(db);
      DynamicObject e = (DynamicObject) s.find(cls, i);
      s.deletePersistent(e);
      closeDTO(s, e, cls);
      returnSession(s);
    }

    count = getCount(db, "same_table");
    if (count != 0) {
      error("Wrong number of rows in the table " + db + ".same_table. Expecting: "
        + 0 + " Got: " + count);
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
