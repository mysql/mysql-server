/*
   Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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

import testsuite.clusterj.model.IdBase;

import com.mysql.clusterj.ClusterJUserException;

import com.mysql.clusterj.annotation.PersistenceCapable;

public class TableWithoutPKTest extends AbstractClusterJModelTest {

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
    }

    public void testFind() {
        try {
            session.find(TableWithoutPK.class, 0);
            error("Find on table without primary key mapped should fail.");
        } catch (ClusterJUserException e) {
            if (!e.getMessage().contains("TableWithoutPK")) {
                error("Find on table without primary key mapped should fail with ClusterJUserException.");
            }
        }
        failOnError();
    }

    public void testInsert() {
        try {
            session.newInstance(TableWithoutPK.class);
            error("newInstance on table without primary key mapped should fail.");
        } catch (ClusterJUserException e) {
            if (!e.getMessage().contains("TableWithoutPK")) {
                error("newInstance on table without primary key mapped should fail with ClusterJUserException.");
            }
        }
        failOnError();
    }

    public void testQuery() {
        try {
            session.getQueryBuilder().createQueryDefinition(TableWithoutPK.class);
            error("createQueryDefinition on table without primary key mapped should fail.");
        } catch (ClusterJUserException e) {
            if (!e.getMessage().contains("TableWithoutPK")) {
                error("newInstance on table without primary key mapped should fail with ClusterJUserException.");
            }
        }
        failOnError();
    }

    @PersistenceCapable(table="twopk")
    static public interface TableWithoutPK extends IdBase {
        String getName();
        void setName(String value);
    }

}
