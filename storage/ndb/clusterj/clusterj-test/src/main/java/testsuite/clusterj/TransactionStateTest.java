/*
   Copyright 2010 Sun Microsystems, Inc.
   Use is subject to license terms.

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

public class TransactionStateTest extends AbstractClusterJModelTest {

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        tx = session.currentTransaction();
    }

    public void test() {
        beginAfterBegin();
        rollbackWithoutBegin();
        commitWithoutBegin();
        commitWithSetRollbackOnly();
        failOnError();
    }

    private void beginAfterBegin() {
        try {
            tx.begin();
            tx.begin(); // should throw exception
            error("Begin after begin failed to throw exception.");
        } catch (ClusterJException ex) {
            // good catch
            errorIfNotEqual("Begin after begin should leave transaction active",
                    true, tx.isActive());
        } finally {
            tx.commit();
        }
    }

    private void commitWithSetRollbackOnly() {
        try {
            tx.begin();
            tx.setRollbackOnly();
            tx.commit(); // should throw exception
            error("Commit after setRollbackOnly failed to throw exception.");
        } catch (ClusterJException ex) {
            // good catch
            errorIfNotEqual("Commit after setRollbackOnly should leave transaction not active",
                    false, tx.isActive());
        }
    }

    private void commitWithoutBegin() {
        try {
            tx.commit(); // should throw exception
            error("Commit without begin failed to throw exception.");
        } catch (ClusterJException ex) {
            // good catch
            errorIfNotEqual("Commit without begin should leave transaction not active",
                    false, tx.isActive());
        }
    }

    private void rollbackWithoutBegin() {
        try {
            tx.rollback(); // should throw exception
            error("Rollback without begin failed to throw exception.");
        } catch (ClusterJException ex) {
            // good catch
            errorIfNotEqual("Rollback without begin should leave transaction not active",
                    false, tx.isActive());
        }
    }

}
