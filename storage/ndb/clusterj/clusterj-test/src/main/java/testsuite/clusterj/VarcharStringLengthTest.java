/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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
