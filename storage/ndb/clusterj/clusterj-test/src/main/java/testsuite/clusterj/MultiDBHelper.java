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

import com.mysql.clusterj.DynamicObject;

public class MultiDBHelper {

  public static boolean verifyEmployeeFields(AbstractClusterJModelTest test,
                                             DynamicObject e,
                                             int num) {
    for (int i = 0; i < e.columnMetadata().length; i++) {
      String fieldName = e.columnMetadata()[i].name();
      if (fieldName.equals("id")) {
        Integer actual = (Integer) e.get(i);
        if (actual != num) {
          test.error("Failed update: for employee " + num
                     + " expected id " + num
                     + " actual id " + actual);
          return false;
        }
      } else if (fieldName.equals("age")) {
        Integer actual = (Integer) e.get(i);
        if (actual != num) {
          test.error("Failed update: for employee " + num
                     + " expected age " + num
                     + " actual age " + actual);
          return false;
        }
      } else if (fieldName.equals("name")) {
        String actual = (String) e.get(i);
        if (actual.compareTo(Integer.toString(num)) != 0) {
          test.error("Failed update: for employee " + num
                     + " expected name " + num
                     + " actual name " + actual);
          return false;
        }
      } else if (fieldName.equals("magic")) {
        Integer actual = (Integer) e.get(i);
        if (actual != num) {
          test.error("Failed update: for employee " + num
                     + " expected magic " + num
                     + " actual magic " + actual);
          return false;
        }
      } else {
        test.error("Unexpected Column");
        return false;
      }
    }
    return true;
  }

  public static boolean setEmployeeFields(AbstractClusterJModelTest test, DynamicObject e,
                                          int num) {
    for (int i = 0; i < e.columnMetadata().length; i++) {
      String fieldName = e.columnMetadata()[i].name();
      if (fieldName.equals("id")) {
        e.set(i, num);
      } else if (fieldName.equals("age")) {
        e.set(i, num);
      } else if (fieldName.equals("name")) {
        e.set(i, Integer.toString(num));
      } else if (fieldName.equals("magic")) {
        e.set(i, num);
      } else {
        test.error("Unexpected Column");
        return false;
      }
    }
    return true;
  }

  public static int getEmployeeID(DynamicObject e) {
    for (int i = 0; i < e.columnMetadata().length; i++) {
      String fieldName = e.columnMetadata()[i].name();
      if (fieldName.equals("id")) {
        return (Integer) e.get(i);
      } else if (fieldName.equals("age")) {
      } else if (fieldName.equals("name")) {
      } else if (fieldName.equals("magic")) {
      } else {
        return -1;
      }
    }
    return -1;
  }
}
