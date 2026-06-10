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
import com.mysql.clusterj.SessionFactory;

import java.sql.PreparedStatement;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;
import java.util.Properties;

public class UnloadSchemaTest extends AbstractClusterJModelTest {

  private static final String TABLE = "fgtest";
  private static String DROP_TABLE_CMD = "drop table if exists " + TABLE;
  private static String CREATE_TABLE_CMD = "CREATE TABLE " + TABLE +
          "(id int NOT NULL, " +
          " col_1 int DEFAULT NULL, " +
          " col_2 varchar(1000) COLLATE utf8_unicode_ci DEFAULT NULL, " +
          " PRIMARY KEY (id)" +
          ") ENGINE=ndbcluster";
  private static String ADD_COL_3_COPY =
          "alter table " + TABLE + " add column col_3 bigint NOT NULL DEFAULT '0', ALGORITHM=COPY";
  private static String ADD_COL_3_INPLACE =
          "alter table " + TABLE + " add column (col_3 bigint DEFAULT NULL), ALGORITHM=INPLACE";
  private static String ADD_COL_4_COPY =
          "alter table " + TABLE + " add column col_4 varchar(100) COLLATE utf8_unicode_ci NOT " +
                  "NULL DEFAULT 'abc_default', ALGORITHM=COPY";
  private static String ADD_COL_4_INPLACE =
          "alter table " + TABLE + " add column col_4 varchar(100) COLLATE utf8_unicode_ci, algorithm=INPLACE";
  private static String TRUNCATE_TABLE =
          "truncate table " + TABLE;

  private static String defaultDB = "test";
  private static final int NUM_THREADS = 10;
  private int SLEEP_TIME = 2000;

  // The test will not pass with USE_COPY_ALGO set to true.
  // Cluster/J cannot keep using a table when a copying ALTER TABLE is in progress.
  // This is discussed below in more detail.
  private boolean USE_COPY_ALGO = false;

  private SessionFactory factoryWithCacheOff = null;
  private SessionFactory factoryWithCacheOn = null;

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

  void returnSession(Session s) {
      s.close();
  }

  void closeDTO(Session s, DynamicObject dto, Class dtoClass) {
      s.release(dto);
  }

  public static class FGTest extends DynamicObject {
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
      System.err.println("Failed to run SQL command. "+e);
      test.error("Failed to drop table. Error: " + e.getMessage());
      throw new RuntimeException("Failed to command: ", e);
    }
  }


  class DataInsertWorker extends Thread {
    private final AbstractClusterJTest test;
    private final SessionFactory factory;
    private final int startIndex;
    private final int maxRowsToWrite;

    private volatile boolean run = true;
    private volatile int insertsCounter = 0;
    private volatile int failCounter = 0;

    DataInsertWorker(AbstractClusterJTest test, SessionFactory factory,
                     int startIndex, int maxRowsToWrite) {
      this.test = test;
      this.factory = factory;
      this.startIndex = startIndex;
      this.maxRowsToWrite = maxRowsToWrite;
    }

    @Override
    public void run() {

      int currentIndex = startIndex;
      while (run) {
        test.sleep(1);
        Session session = factory.getSession(defaultDB);
        DynamicObject e = null;
        boolean rowInserted = false;
        try {
          e = (DynamicObject) session.newInstance(FGTest.class);
          setFields(e, currentIndex++);
          session.savePersistent(e);
          closeDTO(session, e, FGTest.class);
          insertsCounter++;
          rowInserted = true;
          if (currentIndex > (startIndex + maxRowsToWrite)) {
            currentIndex = startIndex;
          }
        } catch (ClusterJDatastoreException ex) {
          failCounter++;
          if(ex.isStaleMetadata()) {
            session.unloadSchema(FGTest.class);
           } else if(ex.isSchemaChangePending()) {
              ex.awaitSchemaChange();
           } else if(! ex.tableNotFound()) {
              throw ex;
           }
        } catch (Throwable t) {
          failCounter++;
          error("Caught exception " + t.getMessage());
          throw t;
        }
        finally {
          returnSession(session);
        }
      }
    }

    public void stopDataInsertion() {
      run = false;
    }

    public int getInsertsCounter() {
      return insertsCounter;
    }

    public int getFailCounter() {
      return failCounter;
    }

    public void setFields(DynamicObject e, int num) {
      for (int i = 0; i < e.columnMetadata().length; i++) {
        String fieldName = e.columnMetadata()[i].name();
        if (fieldName.equals("id")) {
          e.set(i, num);
        } else if (fieldName.equals("col_1")) {
          e.set(i, num);
        } else if (fieldName.equals("col_2")) {
          e.set(i, Long.toString(num));
        } else if (fieldName.equals("col_3")) {
          long num_long = num;
          e.set(i, num_long);
        } else if (fieldName.equals("col_4")) {
          e.set(i, Long.toString(num));
        } else {
          throw new IllegalArgumentException("Unexpected Column");
        }
      }
    }
  }

  public void testInplaceWithCache() {
    this.USE_COPY_ALGO = false;
    runUloadSchemaTest(factoryWithCacheOn);
  }

  public void testInplaceNoCache() {
    this.USE_COPY_ALGO = false;
    runUloadSchemaTest(factoryWithCacheOff);
  }

  /* If this function were named testCopyingNoCache() it would be an actual test
     and would fail. In particular, the JDBC statement running "ALTER TABLE"
     would fail with "Detected change to data in source table during copying
     ALTER TABLE. Alter aborted to avoid inconsistency." A copying ALTER TABLE
     operation depends on a voluntary protocol between MySQL servers, but
     Cluster/J does not participate.
  */
  public void __toastCopyingNoCache() {
    this.USE_COPY_ALGO = true;
    runUloadSchemaTest(factoryWithCacheOff);
  }

  private void runUloadSchemaTest(SessionFactory factory) {
    try {  
      runSQLCMD(this, DROP_TABLE_CMD);
      runSQLCMD(this, CREATE_TABLE_CMD);

      // Unload the schema from some previous test run
      Session session = factory.getSession();
      session.unloadSchema(FGTest.class);
      closeSession();

      List<DataInsertWorker> threads = new ArrayList<>(NUM_THREADS);
      for (int i = 0; i < NUM_THREADS; i++) {
        DataInsertWorker t = new DataInsertWorker(this, factory, i * 1000000, 1000);
        threads.add(t);
        t.start();
      }

      Thread.sleep(SLEEP_TIME);

      if (USE_COPY_ALGO) {
        runSQLCMD(this, ADD_COL_3_COPY);
      } else {
        runSQLCMD(this, ADD_COL_3_INPLACE);
      }

      Thread.sleep(SLEEP_TIME);

      if (USE_COPY_ALGO) {
        runSQLCMD(this, ADD_COL_4_COPY);
      } else {
        runSQLCMD(this, ADD_COL_4_INPLACE);
      }

      Thread.sleep(SLEEP_TIME);

      for (int i = 0; i < NUM_THREADS; i++) {
        threads.get(i).stopDataInsertion();
      }

      int totalInsertions = 0;
      int totalFailures = 0;
      for (int i = 0; i < NUM_THREADS; i++) {
        threads.get(i).join();
        totalInsertions += threads.get(i).getInsertsCounter();
        totalFailures += threads.get(i).getFailCounter();
      }
      if(logger.isInfoEnabled())
          logger.info("Total Inserts: " + totalInsertions +
                      " Failed Inserts: " + totalFailures);
      if(totalInsertions < NUM_THREADS)
          error("Success count too low: " + totalInsertions);
    } catch (Exception e) {
      error("Caught exception " + e.getMessage());
    }
    failOnError();
  }
}
