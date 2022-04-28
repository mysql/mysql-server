/*
   Copyright (c) 2013, 2022, Oracle and/or its affiliates.

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

import com.mysql.clusterj.ClusterJDatastoreException;
import com.mysql.clusterj.ClusterJException;

import testsuite.clusterj.model.Employee;

public class TransactionErrorSetPartitionKeyTest extends AbstractClusterJModelTest {

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        createEmployeeInstances(4);
        tx = session.currentTransaction();
        tx.begin();
        session.deletePersistentAll(Employee.class);
        tx.commit();
    }

    public void testCommit() {
        // first, create instance
        tx = session.currentTransaction();
        tx.begin();
        session.makePersistent(employees.get(0));
        tx.commit();

        // error: try to insert the same instance again
        tx.begin();
        session.makePersistent(employees.get(0));
        try {
            tx.commit();
            error("flush with error did not throw an exception.");
        } catch (ClusterJDatastoreException cjdex) {
            // good catch; commit failed and the transaction is not active
            errorIfNotEqual("Transaction.isActive()", false, tx.isActive());
        }
        try {
            session.setPartitionKey(Employee.class, 0);
            tx.begin();
            session.find(Employee.class, 0);
            tx.commit();
        } catch (ClusterJException cjex) {
            error("setPartitionKey; begin; find; commit caught: " + cjex.getMessage());
        }
        failOnError();
    }

    public void testFlushCommit() {
        // first, create instance
        tx = session.currentTransaction();
        tx.begin();
        session.makePersistent(employees.get(1));
        tx.commit();

        // error: try to insert the same instance again
        tx.begin();
        session.makePersistent(employees.get(1));
        try {
            session.flush();
            error("flush with error did not throw an exception.");
        } catch (ClusterJDatastoreException cjdex) {
            // good catch; transaction is still active and can be committed
            errorIfNotEqual("Transaction.isActive()", true, tx.isActive());
            tx.commit();
        }
        try {
            session.setPartitionKey(Employee.class, 1);
            tx.begin();
            session.find(Employee.class, 1);
            tx.commit();
        } catch (ClusterJException cjex) {
            error("setPartitionKey; begin; find; commit caught throwable: " + cjex.getMessage());
        }
        failOnError();
    }

    public void testFlushRollback() {
        // first, create instance
        tx = session.currentTransaction();
        tx.begin();
        session.makePersistent(employees.get(2));
        tx.commit();

        // error: try to insert the same instance again
        tx.begin();
        session.makePersistent(employees.get(2));
        try {
            session.flush();
            error("flush with error did not throw an exception.");
        } catch (ClusterJDatastoreException cjdex) {
            // good catch; transaction is still active and can be rolled back
            tx.rollback();
        }
        try {
            session.setPartitionKey(Employee.class, 2);
            tx.begin();
            session.find(Employee.class, 2);
            tx.commit();
        } catch (ClusterJException cjex) {
            error("setPartitionKey; begin; find; commit caught throwable: " + cjex.getMessage());
        }
        failOnError();
    }

    public void testRollback() {
        // first, create instance
        tx = session.currentTransaction();
        tx.begin();
        session.makePersistent(employees.get(3));
        tx.commit();

        // error: try to insert the same instance again
        tx.begin();
        session.makePersistent(employees.get(3));
        try {
            tx.rollback();
        } catch (ClusterJException cjex) {
            // bad catch; rollback failed
            error("rollback threw: " + cjex.getMessage());
        }
        try {
            session.setPartitionKey(Employee.class, 3);
            tx.begin();
            session.find(Employee.class, 3);
            tx.commit();
        } catch (ClusterJException cjex) {
            error("setPartitionKey; begin; find; commit caught: " + cjex.getMessage());
        }
        failOnError();
    }

}
