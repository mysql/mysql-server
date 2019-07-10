/* -*- mode: java; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is also distributed with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have included with MySQL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License, version 2.0, for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

package com.mysql.cluster.crund;

abstract public class NdbBase extends CrundDriver {

    // NDB settings
    protected String mgmdConnect;
    protected String catalog;
    protected String schema;

    // so far, there's no NDB support for caching data beyond Tx scope
    protected void clearPersistenceContext() throws Exception {}

    // ----------------------------------------------------------------------
    // NDB Base intializers/finalizers
    // ----------------------------------------------------------------------

   protected void initProperties() {
        super.initProperties();

        out.print("setting ndb properties ...");

        final StringBuilder msg = new StringBuilder();
        final String eol = System.getProperty("line.separator");

        // the hostname and port number of NDB mgmd
        mgmdConnect = props.getProperty("ndb.mgmdConnect", "localhost");
        assert mgmdConnect != null;

        // the database
        catalog = props.getProperty("ndb.catalog", "crunddb");
        assert catalog != null;

        // the schema
        schema = props.getProperty("ndb.schema", "def");
        assert schema != null;

        if (msg.length() == 0) {
            out.println("      [ok]");
        } else {
            out.println();
            out.print(msg.toString());
        }
    }

    protected void printProperties() {
        super.printProperties();

        out.println();
        out.println("ndb settings ...");
        out.println("ndb.mgmdConnect:                \"" + mgmdConnect + "\"");
        out.println("ndb.catalog:                    \"" + catalog + "\"");
        out.println("ndb.schema:                     \"" + schema + "\"");
    }
}
