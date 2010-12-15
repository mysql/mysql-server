/*
   Copyright (C) 2009 Sun Microsystems Inc.
   All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

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
    public void test() {
        createSessionFactory();
        session = sessionFactory.getSession();
        session.deletePersistentAll(Employee.class);
        for (int i = 0; i < 500; ++i) {
            Employee e = session.newInstance(Employee.class, i);
            e.setName(name.substring(0, i));
            e.setAge(i);
            e.setMagic(i);
            try {
                session.makePersistent(e);
            } catch (ClusterJException ex) {
                if (i < 33) {
                    // unexpected error for lengths not greater than varchar size 
                    error("Bad insert for: " + i + " " + ex.getMessage());
                }
            }
        }
    }
}
