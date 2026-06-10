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

import java.sql.PreparedStatement;
import java.sql.SQLException;

/*
When a table is deleted and recreated with different schema then we need to unload
the schema otherwise we will get schema version mismatch errors. However, the
SessionFactoryImpl.typeToHandlerMap uses Class as keys. The Dynamic class that will represent the
new table will not match with any key in SessionFactoryImpl.typeToHandlerMap and unloadSchema
will not do anything.

We have changed unloadSchema such that if class is not found in SessionFactoryImpl.typeToHandlerMap
then we iterate over the keys in the SessionFactoryImpl.typeToHandlersMap and check if the table
name matches with the user supplied class. If a match is found then we unload that table and
return.
 */
public class UnloadSchemaAfterRecreateTest extends AbstractClusterJModelTest {
  private static final String TABLE = "fgtest";
  private String DROP_TABLE_CMD = "drop table if exists " + TABLE;

  private String CREATE_TABLE_CMD1 = "CREATE TABLE " + TABLE + " ( id int NOT NULL," +
          " number int DEFAULT NULL, PRIMARY KEY (id)) ENGINE=ndbcluster";

  // table with same name a above but different columns
  private String CREATE_TABLE_CMD2 = "CREATE TABLE " + TABLE + " ( id int NOT NULL," +
          " name varchar(1000) COLLATE utf8_unicode_ci DEFAULT NULL," +
          " PRIMARY KEY (id)) ENGINE=ndbcluster";

  private static String defaultDB = "test";

  @Override
  public void localSetUp() {
    createSessionFactory();
    defaultDB = props.getProperty(Constants.PROPERTY_CLUSTER_DATABASE);
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

  void closeDTO(Session s, DynamicObject dto) {
    s.release(dto);
  }

  public static class FGTest1 extends DynamicObject {
    @Override
    public String table() {
      return TABLE;
    }
  }

  public static class FGTest2 extends DynamicObject {
    @Override
    public String table() {
      return TABLE;
    }
  }

  public void runSQLCMD(AbstractClusterJModelTest test, String cmd) {
    PreparedStatement preparedStatement = null;

    try {
      preparedStatement = connection.prepareStatement(cmd);
      preparedStatement.executeUpdate();
    } catch (SQLException e) {
      test.error("Failed to drop table. Error: " + e.getMessage());
      throw new RuntimeException("Failed to command: ", e);
    }
  }

  public void testUnloadSchema() throws Exception {
    closeSession();
    closeAllExistingSessionFactories();
    sessionFactory = null;
    createSessionFactory();

    runSQLCMD(this, DROP_TABLE_CMD);
    runSQLCMD(this, CREATE_TABLE_CMD1);

    // write something
    Session session = getSession(defaultDB);
    DynamicObject e = session.newInstance(FGTest1.class);
    setFields(this, e, 0);
    session.savePersistent(e);
    closeDTO(session, e);
    returnSession(session);

    // delete the table and create a new table with the same name
    runSQLCMD(this, DROP_TABLE_CMD);
    runSQLCMD(this, CREATE_TABLE_CMD2);

    // unload the schema using new dynamic class
    session = getSession(defaultDB);
    String table = session.unloadSchema(FGTest2.class);
    errorIfNotEqual("unloadSchema result", TABLE, table);
    returnSession(session);

    // write something to the new table
    session = getSession(defaultDB);
    e = session.newInstance(FGTest2.class);
    setFields(this, e, 0);
    session.savePersistent(e);
    closeDTO(session, e);
    returnSession(session);
    failOnError();
  }

  public void setFields(AbstractClusterJModelTest test, DynamicObject e, int num) {
    for (int i = 0; i < e.columnMetadata().length; i++) {
      String fieldName = e.columnMetadata()[i].name();
      if (fieldName.equals("id")) {
        e.set(i, num);
      } else if (fieldName.equals("name")) {
        e.set(i, Integer.toString(num));
      } else if (fieldName.equals("number")) {
        e.set(i, num);
      } else {
        test.error("Unexpected Column");
      }
    }
  }
}
