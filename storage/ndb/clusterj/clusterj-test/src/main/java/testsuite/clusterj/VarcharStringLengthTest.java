/*
   Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package testsuite.clusterj;

import com.mysql.clusterj.ClusterJException;

import testsuite.clusterj.model.Employee;

public class VarcharStringLengthTest extends AbstractClusterJTest {

    private static String name = 
        "11111111111111111111111111111111111111111111111111" +
        "11111111111111111111111111111111111111111111111111" +
        "11111111111111111111111111111111111111111111111111" +
        "11111111111111111111111111111111111111111111111111" +
        "11111111111111111111111111111111111111111111111111" +
        "11111111111111111111111111111111111111111111111111" +
        "11111111111111111111111111111111111111111111111111" +
        "11111111111111111111111111111111111111111111111111" +
        "11111111111111111111111111111111111111111111111111" +
        "11111111111111111111111111111111111111111111111111";

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        session.deletePersistentAll(Employee.class);
        addTearDownClasses(Employee.class);
    }

    public void test() {
        for (int i = 0; i < 500; ++i) {
            Employee e = session.newInstance(Employee.class, i);
            e.setName(name.substring(0, i));
            e.setAge(i);
            e.setMagic(i);
            try {
                session.makePersistent(e);
                if (i > 32) {
                    // unexpected success for lengths greater than varchar size
                    error("Expected exception not thrown for: " + i);
                }
            } catch (ClusterJException ex) {
                if (i < 33) {
                    // unexpected error for lengths not greater than varchar size 
                    error("Unexpected exception for: " + i + " " + ex.getMessage());
                }
            }
        }
        failOnError();
    }
}
