/*
 *  Copyright (c) 2025, 2026, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is designed to work with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have either included with
 *  the program or referenced in the documentation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License, version 2.0, for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj.tie;

import java.nio.ByteBuffer;

import com.mysql.clusterj.ClusterJUserException;

import com.mysql.ndbjtie.ndbapi.Ndb;
import com.mysql.ndbjtie.ndbapi.NdbDictionary.Dictionary;
import com.mysql.ndbjtie.ndbapi.NdbErrorConst;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

class DbImplCore {
    /** Message translator for DbImplCore, DbImpl, DbImplForNdbRecord */
    static final I18NHelper local = I18NHelper.getInstance(DbImpl.class);

    /** Logger for DbImplCore, DbImpl, DbImplForNdbRecord */
    static final Logger logger = LoggerFactoryService.getFactory()
            .getInstance(DbImpl.class);

    /** The Ndb instance that this instance is wrapping */
    Ndb ndb;

    /** The configured transaction max */
    final int maxTransactions;

    /** The ndb error detail buffer */
    protected ByteBuffer errorBuffer = null;

    /** The NdbDictionary for this Ndb */
    protected Dictionary ndbDictionary = null;

    /** Reutrn code from ndb.init() */
    protected final int initReturnCode;

    /** This db is closing */
    private boolean closing = false;

    /** Common constructor */
    DbImplCore(Ndb ndb, int maxTransactions) {
        this.ndb = ndb;
        this.maxTransactions = maxTransactions;
        this.initReturnCode = ndb.init(maxTransactions);
        if(initReturnCode == 0) {
            ndbDictionary = ndb.getDictionary();
        }
    }

    /* Copy core fields between a DbImplCore cache item and a full DbImpl */
    DbImplCore(DbImplCore item) {
        ndb = item.ndb;
        maxTransactions = item.maxTransactions;
        ndbDictionary = item.ndbDictionary;
        initReturnCode = 0;
    }

    /* Delete the Ndb object and clear the cache item */
    void delete() {
        Ndb.delete(ndb);
        ndbDictionary = null;
    }

    public Dictionary getNdbDictionary() {
        return ndbDictionary;
    }

    protected void closing() {
        closing = true;
    }

    public void assertNotClosed(String where) {
        if (closing || ndb == null) {
            throw new ClusterJUserException(local.message("ERR_Db_Is_Closing", where));
        }
    }

    protected void handleInitError() {
        if (initReturnCode == 0) {
            return;
        } else {
            NdbErrorConst ndbError = ndb.getNdbError();
            String detail = getNdbErrorDetail(ndbError);
            Utility.throwError(initReturnCode, ndbError, detail);
        }
    }

    protected void handleError(Object object, Ndb ndb) {
        if (object != null) {
            return;
        } else {
            NdbErrorConst ndbError = ndb.getNdbError();
            String detail = getNdbErrorDetail(ndbError);
            Utility.throwError(null, ndbError, detail);
        }
    }

    public String getNdbErrorDetail(NdbErrorConst ndbError) {
        return ndb.getNdbErrorDetail(ndbError, errorBuffer, errorBuffer.capacity());
    }
}
