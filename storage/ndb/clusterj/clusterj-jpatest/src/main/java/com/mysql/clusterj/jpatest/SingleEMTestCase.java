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

package com.mysql.clusterj.jpatest;

import javax.persistence.EntityManager;
import javax.persistence.EntityTransaction;

/**
 * A base test case for a single EntityManager at any given time.
 */
public abstract class SingleEMTestCase extends SingleEMFTestCase {

    protected EntityManager em;

    protected EntityTransaction tx;

    @Override
    public void setUp() {
        super.setUp();
        em = emf.createEntityManager(); 
    }

    @Override
    public void setUp(Object... props) {
        super.setUp(props);
        em = emf.createEntityManager(); 
    }

    @Override
    public void tearDown() throws Exception {
        rollback();
        close();
        super.tearDown();
    }

    /** 
     * Start a new transaction if there isn't currently one active. 
     * @return true if a transaction was started, false if one already existed
     */
    protected boolean begin() {
        EntityTransaction tx = em.getTransaction();
        if (tx.isActive())
            return false;
        tx.begin();
        return true;
    }

    /** 
     * Commit the current transaction, if it is active. 
     * @return true if the transaction was committed
     */
    protected boolean commit() {
        EntityTransaction tx = em.getTransaction();
        if (!tx.isActive())
            return false;

        tx.commit();
        return true;
    }

    /** 
     * Roll back the current transaction, if it is active. 
     * @return true if the transaction was rolled back
     */
    protected boolean rollback() {
        if (em != null && em.isOpen()) {
            EntityTransaction tx = em.getTransaction();
            if (!tx.isActive()) {
                return false;
            } else {
                tx.rollback();
                return true;
            }
        } else {
            return false;
        }
    }

    /** 
     * Closes the current EntityManager if it is open. 
     * @return false if the EntityManager was already closed
     */
    protected boolean close() {
        if (em == null)
            return false;

        rollback();

        if (!em.isOpen())
            return false;

        em.close();
        return !em.isOpen();
    }

}
