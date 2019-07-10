/*
   Copyright 2010 Sun Microsystems, Inc.
   All rights reserved. Use is subject to license terms.

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

package com.mysql.clusterj.openjpa;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactory;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import java.util.Map;

import org.apache.openjpa.conf.OpenJPAProductDerivation;

import org.apache.openjpa.lib.conf.AbstractProductDerivation;

/**
 * Sets JDBC as default store.
 */
public class NdbOpenJPAProductDerivation extends AbstractProductDerivation
    implements OpenJPAProductDerivation {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(NdbOpenJPAProductDerivation.class);

    /** Register logger for Ndb OpenJPA stuff. */
    static final LoggerFactory loggerFactory = LoggerFactoryService.getFactory();
    static {
        loggerFactory.registerLogger("com.mysql.clusterj.openjpa");
    }

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(NdbOpenJPAProductDerivation.class);

    @SuppressWarnings("unchecked")
    public void putBrokerFactoryAliases(Map m) {
        m.put("ndb", NdbOpenJPABrokerFactory.class.getName());
    }

    public int getType() {
        return TYPE_STORE;
    }

}
