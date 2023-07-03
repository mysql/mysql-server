/*
   Copyright (c) 2012, 2022, Oracle and/or its affiliates.

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
