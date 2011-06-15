/*
   Copyright 2010 Sun Microsystems, Inc.
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
