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


/*
There is a bug in clusterj such that when a not null string column is set to null then
the value is set to empty string
 */
public class NotNullColumnTest extends AbstractClusterJModelTest {
  private static final String TABLE = "notnulltable";

  private static String defaultDB = "test";

  @Override
  public void localSetUp() {
    createSessionFactory();
    defaultDB = props.getProperty(Constants.PROPERTY_CLUSTER_DATABASE);
  }

  public static class TestTable extends DynamicObject {
    @Override
    public String table() {
      return TABLE;
    }
  }


  public void testSetValueNull() {
    boolean dataInserted;
    try {
      Session session1 = sessionFactory.getSession(defaultDB);
      session1.currentTransaction().begin();
      TestTable dto2 = (TestTable) session1.newInstance(TestTable.class);
      setFields(this, dto2);
      session1.savePersistent(dto2);
      session1.currentTransaction().commit();
      dataInserted = true; // we should not have gotten here as "value" column is set to null.
    } catch (Exception e) {
      dataInserted = false;
    }
    if (dataInserted) {
      this.error("FAILED. Data insertion should have failed");
    }
    this.failOnError();
  }

  private void setFields(AbstractClusterJModelTest test, DynamicObject e) {
    for (int i = 0; i < e.columnMetadata().length; i++) {
      String fieldName = e.columnMetadata()[i].name();
      if (fieldName.equals("id")) {
        e.set(i, 1);
      } else if (fieldName.equals("value")) {
        e.set(i, null);
      } else {
        test.error("Unexpected Column");
      }
    }
  }
}
