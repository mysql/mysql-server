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

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

import javax.persistence.EntityManagerFactory;
import javax.persistence.Persistence;

import junit.framework.TestCase;
import junit.framework.TestResult;

/**
 * Base class for all ClusterJPA test cases.
 */
public abstract class PersistenceTestCase
    extends TestCase {

    /**
     * The {@link TestResult} instance for the current test run.
     */
    protected TestResult testResult;

    /**
     *
     * Error messages collected during a test.
     */
    private StringBuffer errorMessages;

    private String NL = "\n";

    /** The properties used to create the entity manager factory */
    protected Map map;

    /**
     * Create an entity manager factory from properties.
     * @param props configuration values in the form key, value, key, value...
     */
    protected EntityManagerFactory createEMF(Object... props) {
        String puName = getPersistenceUnitName();
        EntityManagerFactory result = createNamedEMF(puName, props);
//        if (result == null) {
//            System.out.println("Unable to create EMF with properties PUName " + puName + " props " + Arrays.toString(props));
//        } else {
//            System.out.println("Created EMF with properties PUName " + puName + " props " + Arrays.toString(props));
//        }
        return result;
    }

    /**
     * The name of the persistence unit that this test class should use
     * by default. This defaults to "ndb".
     */
    protected String getPersistenceUnitName() {
        String puName = System.getProperty(
                "com.mysql.clusterj.jpa.PersistenceUnit", "ndb");
        if (puName.length() == 0 || puName.equals("${com.mysql.clusterj.jpa.PersistenceUnit}")) {
            return "ndb";
        } else {
            return puName;
        }
    }

    /**
     * Create an entity manager factory for persistence unit <code>pu</code>.
     *
     * @param props configuration values in the form key, value, key, value...
     */
    protected EntityManagerFactory createNamedEMF(String pu,
        Object... props) {
        map = new HashMap();
        boolean prop = false;
        for (int i = 0; props != null && i < props.length; i++) {
            if (prop) {
                map.put(props[i - 1], props[i]);
                prop = false;
            } else if (props[i] != null)
                prop = true;
        }
        return Persistence.createEntityManagerFactory(pu, map);
    }

    @Override
    public void run(TestResult testResult) {
        this.testResult = testResult;
        super.run(testResult);
    }

    @Override
    public void tearDown() throws Exception {
        try {
            super.tearDown();
        } catch (Exception e) {
            // if the test failed, swallow any exceptions that happen
            // during tear-down, as these just mask the original problem.
            // if the test succeeded, this is a real problem.
            if (testResult.wasSuccessful())
                throw e;
        }
    }

    /**
     * Close the EMF.
     */
    protected boolean closeEMF(EntityManagerFactory emf) {
        if (emf == null || !emf.isOpen())
            return false;
        emf.close();
        return !emf.isOpen();
    }

    protected void initializeErrorMessages() {
        if (errorMessages == null) {
            errorMessages = new StringBuffer();
            errorMessages.append(NL);
        }
    }

    protected void error(String message) {
        initializeErrorMessages();
        errorMessages.append(message + NL);
    }

    protected void errorIfNotEqual(String message, Object expected, Object actual) {
        if (expected == null && actual == null) {
            return;
        }
        if (expected != null && expected.equals(actual)) {
            return;
        } else {
            initializeErrorMessages();
            errorMessages.append(message + NL);
            errorMessages.append(
                    "Expected: " + ((expected==null)?"null":expected.toString())
                    + " actual: " + ((actual==null)?"null":actual.toString()) + NL);
        }
    }

    protected void errorIfNull(String message, Object actual) {
        if (actual != null) {
            return;
        } else {
            initializeErrorMessages();
            errorMessages.append(message + NL);
        }
    }

    protected void failOnError() {
        if (errorMessages != null) {
            fail(errorMessages.toString());
        }
    }

}
