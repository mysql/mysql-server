/*
   Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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

import testsuite.clusterj.model.CharsetLatin1;

public class VarcharStringLengthTest extends AbstractClusterJTest {

    private static String characters = 
        "11111111111111111111111111111111111111111111111111" +
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
        session.deletePersistentAll(CharsetLatin1.class);
        addTearDownClasses(CharsetLatin1.class);
    }

    public void testSmall() {
        session.currentTransaction().begin();
        for (int i = 195; i < 205; ++i) {
            try {
                CharsetLatin1 e = session.newInstance(CharsetLatin1.class, i);
                e.setSmallColumn(characters.substring(0, i));
                session.makePersistent(e);
                if (i > 200) {
                    // unexpected success for lengths greater than varchar size
                    error("Expected exception not thrown for: " + i);
                }
            } catch (ClusterJException ex) {
                if (i < 201) {
                    // unexpected error for lengths not greater than varchar size 
                    error("Unexpected exception for: " + i + " " + ex.getMessage());
                }
            }
        }
        session.currentTransaction().rollback();
        failOnError();
    }

    public void testMedium() {
        session.currentTransaction().begin();
        for (int i = 495; i < 505; ++i) {
            try {
                CharsetLatin1 e = session.newInstance(CharsetLatin1.class, i);
                e.setMediumColumn(characters.substring(0, i));
                session.makePersistent(e);
                if (i > 500) {
                    // unexpected success for lengths greater than varchar size
                    error("Expected exception not thrown for: " + i);
                }
            } catch (ClusterJException ex) {
                if (i < 501) {
                    // unexpected error for lengths not greater than varchar size 
                    error("Unexpected exception for: " + i + " " + ex.getMessage());
                }
            }
        }
        session.currentTransaction().rollback();
        failOnError();
    }

}
