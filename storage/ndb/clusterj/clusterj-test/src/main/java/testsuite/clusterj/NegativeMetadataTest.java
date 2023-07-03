/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

import com.mysql.clusterj.ClusterJUserException;
import testsuite.clusterj.model.BadEmployeeWrongPrimaryKeyAnnotationOnClass;
import testsuite.clusterj.model.BadEmployeeNoPrimaryKeyAnnotationOnClass;
import testsuite.clusterj.model.BadEmployeePrimaryKeyAnnotationColumnAndColumns;
import testsuite.clusterj.model.BadEmployeePrimaryKeyAnnotationNoColumnOrColumns;
import testsuite.clusterj.model.BadEmployeePrimaryKeyAnnotationOnClassMisspelledField;
import testsuite.clusterj.model.BadIndexDuplicateColumn;
import testsuite.clusterj.model.BadIndexDuplicateIndexName;
import testsuite.clusterj.model.BadIndexMissingColumn;

public class NegativeMetadataTest extends AbstractClusterJModelTest {

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
    }

    public void test() {
        // doTestFor(BadEmployeeNoPrimaryKeyAnnotationOnClass.class);
        // doTestFor(BadEmployeePrimaryKeyAnnotationOnClassMisspelledField.class);
        // doTestFor(BadEmployeeWrongPrimaryKeyAnnotationOnClass.class);
        // doTestFor(BadEmployeePrimaryKeyAnnotationColumnAndColumns.class);
        // doTestFor(BadEmployeePrimaryKeyAnnotationNoColumnOrColumns.class);
        // doTestFor(BadIndexDuplicateIndexName.class);
        // doTestFor(BadIndexMissingColumn.class);
        // doTestFor(BadIndexDuplicateColumn.class);
        failOnError();
    }

    public void doTestFor(Class<?> cls) {
        try {
            session.newInstance(cls);
            error("failed to throw exception for " + cls.getName());
        } catch (ClusterJUserException ex) {
            // good catch
            if (debug) {
                System.out.println(ex.getMessage());
            }
        }
    }

}
