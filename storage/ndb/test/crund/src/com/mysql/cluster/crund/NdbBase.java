/* -*- mode: java; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (C) 2008 MySQL
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

package com.mysql.cluster.crund;

abstract public class NdbBase extends Driver {

    // the NDB database connection
    protected String mgmdConnect;
    protected String catalog;
    protected String schema;

    // so far, there's no NDB support for caching data beyond Tx scope
    protected void clearPersistenceContext() throws Exception {}

    // NDB operations must demarcate transactions themselves
    final protected void beginTransaction() throws Exception {}
    final protected void commitTransaction() throws Exception {}
    final protected void rollbackTransaction() throws Exception {}

    protected void initProperties() {
        super.initProperties();

        // the hostname and port number of NDB mgmd
        mgmdConnect = props.getProperty("ndb.mgmdConnect", "localhost");
        assert mgmdConnect != null;

        // the database
        catalog = props.getProperty("ndb.catalog", "");
        assert catalog != null;

        // the schema
        schema = props.getProperty("ndb.schema", "");
        assert schema != null;
    }

    protected void printProperties() {
        super.printProperties();
        out.println("ndb.mgmdConnect             " + mgmdConnect);
        out.println("ndb.catalog                 " + catalog);
        out.println("ndb.schema                  " + schema);
    }
}
