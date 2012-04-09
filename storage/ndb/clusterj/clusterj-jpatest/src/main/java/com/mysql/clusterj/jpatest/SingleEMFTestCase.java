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

import java.util.ArrayList;
import java.util.List;

import javax.persistence.EntityManagerFactory;

/**
 * Base class for test cases that use a single EMF.  
 *   
 */
public abstract class SingleEMFTestCase
    extends PersistenceTestCase {

    protected EntityManagerFactory emf;

    /**
     * Check system properties so the tests can be run with overrides.
     */
    public void setUp() {
        List<String> extraProps = new ArrayList<String>();
        String connectionDriverName = System.getProperty("openjpa.ConnectionDriverName");
        String connectionURL = System.getProperty("openjpa.ConnectionURL");
        String connectString = System.getProperty("openjpa.ndb.connectString");
        if (connectionDriverName != null) {
            extraProps.add("openjpa.ConnectionDriverName");
            extraProps.add(connectionDriverName);
        }
        if (connectionURL != null) {
            extraProps.add("openjpa.ConnectionURL");
            extraProps.add(connectionURL);
        }
        if (connectString != null) {
            extraProps.add("openjpa.ndb.connectString");
            extraProps.add(connectString);
        }
        setUp(extraProps.toArray());
    }

    /**
     * Initialize entity manager factory.
     * @param props configuration values in the form key, value, key, value...
     */
    protected void setUp(Object... props) {
        emf = createEMF(props);
    }

    /**
     * Closes the entity manager factory.
     */
    public void tearDown() throws Exception {
        super.tearDown();
        if (emf == null)
            return;
        try {
            closeEMF(emf);
        } catch (Exception e) {
            // if the test failed, swallow any exceptions that happen
            // during tear-down, as these just mask the original problem.
            // if the test succeeded, this is a real problem.
            if (testResult.wasSuccessful())
                throw e;
        }
    }
    
}
