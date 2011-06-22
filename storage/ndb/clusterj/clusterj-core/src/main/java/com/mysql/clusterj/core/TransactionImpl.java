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

package com.mysql.clusterj.core;


import com.mysql.clusterj.Transaction;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

public class TransactionImpl implements Transaction {

    /** My logger */
    static final I18NHelper local = I18NHelper.getInstance(TransactionImpl.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(TransactionImpl.class);

    /** My session. */
    protected SessionImpl session;

    TransactionImpl(SessionImpl session) {
        this.session = session;
    }

    public void begin() {
        session.begin();
    }

    public void commit() {
        session.commit();
    }

    public void rollback() {
        session.rollback();
    }

    public boolean isActive() {
        return session.isActive();
    }

    public void setRollbackOnly() {
        session.setRollbackOnly();
    }

    public boolean getRollbackOnly() {
        return session.getRollbackOnly();
    }

}
