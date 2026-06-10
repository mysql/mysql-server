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
import com.mysql.clusterj.Constants;
import com.mysql.clusterj.DynamicObject;
import com.mysql.clusterj.Session;

import java.sql.PreparedStatement;
import java.sql.SQLException;
import java.util.Properties;
import java.util.Random;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

/*
Fixes for recreating a table with the same name while using session cache.
 */
public class UnloadSchemaAfterDeleteWithCacheTest extends AbstractClusterJModelTest {

  String DEFAULT_DB = "test";
  private static final String TABLE = "fgtest";
  private static String DROP_TABLE_CMD = "drop table if exists " + TABLE;

  private static String CREATE_TABLE_CMD1 =
    "CREATE TABLE " + TABLE +
    " ( id int NOT NULL," +
    "   number1  int DEFAULT NULL, " +
    "   number2  int DEFAULT NULL, " +
    "   PRIMARY KEY (id)) ENGINE=ndbcluster";

  // table with same name as above but different columns
  private static String CREATE_TABLE_CMD2 =
    "CREATE TABLE " + TABLE +
    " ( id int NOT NULL," +
    "   number1  int DEFAULT NULL," +
    "   number2  int DEFAULT NULL," +
    "   number3  int DEFAULT NULL," +
    "   PRIMARY KEY (id)) ENGINE=ndbcluster";

  // Allow plenty of time for DROP TABLE followed by CREATE TABLE
  @Override
  protected Properties modifyProperties() {
    Properties custom = new Properties();
    custom.putAll(props);
    custom.put(Constants.PROPERTY_TABLE_WAIT_MSEC, 700);
    return custom;
  }

  @Override
  public void localSetUp() {
    createSessionFactory();
    DEFAULT_DB = props.getProperty(Constants.PROPERTY_CLUSTER_DATABASE);
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

  void closeDTO(Session s, DynamicObject dto, Class dtoClass) {
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

  public void test() throws Exception {
    closeSession();

    runSQLCMD(this, DROP_TABLE_CMD);
    Session s = getSession(DEFAULT_DB);
    s.unloadSchema(FGTest1.class);
    s.close();
    runSQLCMD(this, CREATE_TABLE_CMD2);

    // write something
    int tries = 1;
    Session session;
    DynamicObject dto;
    for (int i = 0; i < tries; i++) {
      session = getSession(DEFAULT_DB);
      dto = (DynamicObject) session.newInstance(FGTest1.class);
      setFields(this, dto, i);
      session.savePersistent(dto);
      closeDTO(session, dto, FGTest1.class);
      returnSession(session);
    }

    // drop the table and create a new table with the same name
    runSQLCMD(this, DROP_TABLE_CMD);
    runSQLCMD(this, CREATE_TABLE_CMD1);

    Session session1 = getSession(DEFAULT_DB);

    // unload schema
    session = getSession(DEFAULT_DB);
    session.unloadSchema(FGTest2.class);
    returnSession(session);

    // write something to the new table
    session = session1;
    for (int i = 0; i < tries; i++) {
      dto = (DynamicObject) session.newInstance(FGTest2.class);
      setFields(this, dto, i);
      session.savePersistent(dto);
      closeDTO(session, dto, FGTest2.class);
      returnSession(session);
    }
  }

  public void testMT() {
    closeSession();

    runSQLCMD(this, DROP_TABLE_CMD);
    runSQLCMD(this, CREATE_TABLE_CMD2);

    int numWorker = 6 + new Random().nextInt(8);

    final Writer[] writers = new Writer[numWorker];
    final Future[] futures = new Future[numWorker];
    ExecutorService es = Executors.newFixedThreadPool(numWorker);
    for (int i = 0; i < numWorker; i++) {
      writers[i] = new Writer(this, i);
      futures[i] = es.submit(writers[i]);
    }

    sleep(2500);

    runSQLCMD(this, DROP_TABLE_CMD);
    runSQLCMD(this, CREATE_TABLE_CMD2);

    sleep(3500);

    for (int i = 0; i < numWorker; i++) {
      writers[i].stopWriting();
    }

    for (int i = 0; i < numWorker; i++) {
      while (!writers[i].isWriterStopped()) {
        sleep(10);
      }
    }

    int totalFailedOps = 0;
    int totalSuccessfulOps = 0;
    for (int i = 0; i < numWorker; i++) {
      totalFailedOps += writers[i].failedOps;
      totalSuccessfulOps += writers[i].successfulOps;
    }

    if(logger.isInfoEnabled())
      logger.info("Successful Ops: " + totalSuccessfulOps +
                  " Failed Ops: " + totalFailedOps);
    if(totalSuccessfulOps <= totalFailedOps)
      error("Not enough successful operations (" + totalSuccessfulOps +")");

    try {
      for (Future f : futures) {
        f.get();
      }
    } catch (Exception e) {
      e.printStackTrace();
      error(e.getMessage());
    }
    failOnError();
  }

  class Writer implements Callable {
    AbstractClusterJModelTest test;
    int id = 0;
    volatile boolean stopWriting = false;
    volatile boolean isWriterStopped = false;
    int failedOps = 0;
    int successfulOps = 0;

    Writer(AbstractClusterJModelTest test, int id) {
      this.id = id;
      this.test = test;
    }

    public void stopWriting() {
      this.stopWriting = true;
    }

    private void fatalError(String message) {
      stopWriting();
      test.error(message);
    }

    private void fatalError(Exception e) throws Exception {
      stopWriting();
      throw e;
    }

    @Override
    public Object call() throws Exception {
      /* Expect one "invalid schema object version", one "table is being
         dropped", and one (or just a few) "table not found".
      */
      final int MaxErrors = 5;

      try {
        Random rand = new Random();
        while (!stopWriting) {
          Session session = null;
          try {
            session = getSession(DEFAULT_DB);
            DynamicObject dto = session.newInstance(FGTest1.class);
            setFields(test, dto, rand.nextInt());
            session.savePersistent(dto);
            closeDTO(session, dto, FGTest1.class);
            returnSession(session);
            sleep(10);
            successfulOps++;
          } catch (ClusterJDatastoreException dse) {
            failedOps++;
            if(dse.isStaleMetadata()) {
              /* Operation tried to use stale metadata; call unloadSchema() */
              if(session.unloadSchema(FGTest1.class) == null)
                  fatalError("unloadSchema() returned null");
            } else if(dse.isSchemaChangePending()) {
              /* Schema change in progress. Wait for it to complete. */
              dse.awaitSchemaChange();
            } else if (dse.canRetry()) {
                sleep(10);
            } else if(! dse.tableNotFound()) {
              /* Expect TableNotFound due to the interval of time between DROP
                 and CREATE. Any other ClusterJDatastoreException is an error */
              fatalError(dse);
            }
          } catch (Exception e) {
            /* Any other Exception is an error */
            fatalError(e);
          }
          session.close();
          if(failedOps > MaxErrors) {
            fatalError("Thread " + id + " exceeded limit of " + MaxErrors + " errors");
          }
        }
        return null;
      } finally {
        isWriterStopped = true;
      }
    }

    boolean isWriterStopped() {
      return isWriterStopped;
    }
  }

  public void setFields(AbstractClusterJModelTest test, DynamicObject e, int num) {
    for (int i = 0; i < e.columnMetadata().length; i++) {
      String fieldName = e.columnMetadata()[i].name();
      if (fieldName.equals("id")) {
        e.set(i, num);
      } else if (fieldName.startsWith("name")) {
        e.set(i, Integer.toString(num));
      } else if (fieldName.startsWith("number")) {
        e.set(i, num);
      } else {
        test.error("Unexpected Column. "+fieldName);
      }
    }
  }
}
