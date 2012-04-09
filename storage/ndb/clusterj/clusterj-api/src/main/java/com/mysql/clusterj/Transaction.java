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

package com.mysql.clusterj;

/** Transaction represents a user transaction active in the cluster.
 *
 */
public interface Transaction {

    /** Begin a transaction.
     * 
     */
    void begin();

    /** Commit a transaction.
     * 
     */
    void commit();

    /** Roll back a transaction.
     * 
     */
    void rollback();

    /** Is there a transaction currently active?
     * @return true if a transaction is active
     */
    boolean isActive();

    /** Mark this transaction as rollback only. After this method is called,
     * commit() will roll back the transaction and throw an exception;
     * rollback() will roll back the transaction and not throw an exception.
     */
    void setRollbackOnly();

    /** Has this transaction been marked for rollback only?
     * @return true if the transaction has been marked for rollback only
     */
    boolean getRollbackOnly();

}
