/*
   Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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

package testsuite.clusterj;

import com.mysql.clusterj.ClusterJException;
import com.mysql.clusterj.ClusterJHelper;

import com.mysql.clusterj.Constants;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.SessionFactory;
import com.mysql.clusterj.Transaction;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;

import java.io.InputStreamReader;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map.Entry;
import java.util.Properties;

import junit.framework.TestCase;

public abstract class AbstractClusterJCoreTest extends TestCase {

    /** 
     * Indicates an exception thrown in method <code>tearDown</code>.
     * At the end of method <code>tearDown</code> this field is nullified. 
     */
    private Throwable tearDownThrowable;

    /** 
     * A list of registered oid instances. 
     * Corresponding pc instances are deleted in <code>localTearDown</code>.
     */
    private Collection tearDownInstances = new LinkedList();

    /** 
     * A list of registered pc classes. 
     * The extents of these classes are deleted in <code>tearDown</code>.
     */
    private Collection<Class> tearDownClasses = new LinkedList<Class>();

    /** My class loader */
    private static ClassLoader ABSTRACT_CLUSTERJ_CORE_TEST_CLASS_LOADER =
            AbstractClusterJCoreTest.class.getClassLoader();

    /** 
     * Error messages collected during a test.
     */
    private StringBuffer errorMessages;

    private String NL = "\n";

    /** A1 values. */
    String[] a1values = new String[]{"dc=abc", "dc=prs", "dc=xyz"};

    /** This method is called by JUnit 
     * before each test case in each test class.
     */
    @Override
    protected final void setUp() throws Exception {
        setupDn2idPK();
        localSetUp();
    }

    /**
     * Subclasses may override this method to allocate any data and resources
     * that they need in order to successfully execute this testcase.
     * Adding teardown classes and instances is done in the overridden method.
     */
    protected void localSetUp() {}

    /** This method is called by JUnit 
     * after each test case in each test class.
     */
    @Override
    protected final void tearDown() throws Exception {
        localTearDown();
        // if session is null or closed, test class has already cleaned up
        if (session != null && !(session.isClosed())) {
            // if tx is null, get it again
            if (tx == null) {
                tx = session.currentTransaction();
            }
            // if transaction is active (leftover), roll it back
            if (tx.isActive()) {
                tx.rollback();
            }
            // if any work to do, start a transaction and clean up
            if (!tearDownClasses.isEmpty() | !tearDownInstances.isEmpty()) {
                tx.begin();
                for (Class<?> cls: tearDownClasses) {
                    session.deletePersistentAll(cls);
                }
                for (Object o: tearDownInstances) {
                    session.deletePersistent(o);
                }
                tx.commit();
            session.close();
            session = null;
            }
        }
    }

    /**
     * Subclasses may override this method to deallocate any data and resources
     * that they needed in order to successfully execute this testcase.
     */
    protected void localTearDown() {}

    protected void addTearDownClasses(Class... classes) {
        for (Class cls: classes) {
            tearDownClasses.add(cls);
        }
    }

    protected Object[] dn2idPK = new Object[16];

    public AbstractClusterJCoreTest() {
    }

    /** The name of the properties file; default "clusterj.properties". */
    String PROPS_FILE_NAME = System.getProperty(
            "clusterj.properties", "clusterj.properties");

    /** The properties for this test run */
    protected static Properties props;
    
    /** The JDBC Driver name */
    protected static String jdbcDriverName;
    
    /** The JDBC URL */
    protected static String jdbcURL;

    /** The JDBC Username */
    protected static String jdbcUsername;

    /** The JDBC Password */
    protected static String jdbcPassword;

    /** The JDBC Connection */
    protected static Connection connection;

    /** The Schema initialization statements */
    protected static List<String> schemaDefinition = new ArrayList<String>();

    /** Has the schema been initialized */
    protected static boolean schemaInitialized = false;

    /** The connection sessionFactory. */
    protected SessionFactory sessionFactory;

    /** The connection. Local setUp should initialize this if needed. */
    protected Session session;

    /** The transaction. Local setUp should initialize this if needed. */
    protected Transaction tx;

    /** 
     * Get a connection sessionFactory.
     */
    protected void createSessionFactory() {
        loadProperties();
        if (sessionFactory == null) {
            sessionFactory = ClusterJHelper.getSessionFactory(props);
        }
        loadSchema();
    }

    /** Create a file from a property name. */
    Properties getProperties(String fileName) {
        Properties result = null;
        try {
            InputStream stream = new FileInputStream(new File(fileName));
            result = new Properties();
            result.load(stream);
            return result;
        } catch (FileNotFoundException ex) {
        } catch (IOException ex) {
        }
        if (result == null) {
            try {
                // try to load the resource from the class loader
                InputStream stream = ABSTRACT_CLUSTERJ_CORE_TEST_CLASS_LOADER.getResourceAsStream(fileName);
                result = new Properties();
                result.load(stream);
                return result;
            } catch (IOException ex) {
                fail("Could not create ConnectionFactory " + ex);
            }
        }
        return null; // not reached; fail will throw an exception
    }

    protected String getA1for(int number, int index) {
        int a1factor = 1 + number/a1values.length;
        return a1values[index/a1factor];
    }

    protected String getA3for(long i) {
        return "employeenumber=100000" + i;
    }

    protected void setupDn2idPK() {
        dn2idPK[0] = "dc=com";
        // pk[1] changes and is set inside loop
        dn2idPK[1] = "dc=example";
        dn2idPK[2] = "ou=people";
        // pk[3] changes and is set inside loop
        dn2idPK[4] = "";
        dn2idPK[5] = "";
        dn2idPK[6] = "";
        dn2idPK[7] = "";
        dn2idPK[8] = "";
        dn2idPK[9] = "";
        dn2idPK[10] = "";
        dn2idPK[11] = "";
        dn2idPK[12] = "";
        dn2idPK[13] = "";
        dn2idPK[14] = "";
        dn2idPK[15] = "";
    }

    /** Load the schema for tests */
    protected void loadSchema() {
        initializeJDBC();
        if (!schemaInitialized) {
            loadSchemaDefinition();
            if (!testSchema()) {
                initializeSchema();
            }
        }
    }
    
    /** Initialize the JDBC driver */
    protected void initializeJDBC() {
        loadProperties();
        getConnection();
    }

    /** Load properties from clusterj.properties */
    protected void loadProperties() {
        if (props == null) {
            props = getProperties(PROPS_FILE_NAME);
        }
    }

    /** Load JDBC driver */
    protected void getConnection() {
        if (connection == null) {
            jdbcDriverName = props.getProperty(Constants.PROPERTY_JDBC_DRIVER_NAME);
            jdbcURL = props.getProperty(Constants.PROPERTY_JDBC_URL);
            jdbcUsername = props.getProperty(Constants.PROPERTY_JDBC_USERNAME);
            jdbcPassword = props.getProperty(Constants.PROPERTY_JDBC_PASSWORD);
            if (jdbcPassword == null) jdbcPassword = "";
            try {
                Class.forName(jdbcDriverName, true, ABSTRACT_CLUSTERJ_CORE_TEST_CLASS_LOADER);
                connection = DriverManager.getConnection(jdbcURL, jdbcUsername, jdbcPassword);
            } catch (SQLException ex) {
                throw new ClusterJException(
                        "Exception getting connection to " + jdbcURL +
                        "; username " + jdbcUsername, ex);
            } catch (ClassNotFoundException ex) {
                throw new ClusterJException(
                        "Exception loading JDBC driver." + jdbcDriverName, ex);
            }
        }
    }

    protected void loadSchemaDefinition() {
        InputStream inputStream = null;
        StringBuffer buffer = new StringBuffer();
        String line;
        try {
            inputStream = ABSTRACT_CLUSTERJ_CORE_TEST_CLASS_LOADER
                    .getResourceAsStream("schema.sql");
            BufferedReader reader = new BufferedReader(
                    new InputStreamReader(inputStream));
            while (reader.ready()) {
                line = reader.readLine();
                if (line.contains("#")) {
                    // comment line; ignore
                    continue;
                }
                int semi = line.indexOf(";");
                if (semi != -1) {
                    // end of sql statement; finish this statement
                    buffer.append(line.substring(0, semi));
                    schemaDefinition.add(buffer.toString());
                    buffer = new StringBuffer();
                } else {
                    buffer.append(line);
                }
            }
        } catch (IOException ex) {
                throw new ClusterJException(
                        "Exception reading schema.sql.", ex);
        } finally {
            try {
                if (inputStream != null) {
                    inputStream.close();
                }
            } catch (IOException ex) {
                // ignore this
            }
        }
    }

    protected boolean testSchema() {
        try {
            PreparedStatement ps = connection.prepareStatement(schemaDefinition.get(0));
            ps.execute();
            ps.close();
            return true;
        } catch (SQLException ex) {
            System.out.println(
                    "Test schema failed (normal)" + schemaDefinition.get(0));
            return false;
        }
    }

    /** Initialize schema definitions. */
    protected void initializeSchema() {
        Iterator it = schemaDefinition.iterator();
        it.next(); // skip past condition
        String statement = null;
        try {
            while(it.hasNext()) {
                statement = (String) it.next();
                System.out.println("Executing statement " + statement + ".");
                PreparedStatement s = connection.prepareStatement(statement);
                s.execute();
                s.close();
            }
            schemaInitialized = true;
            connection.close();
        } catch (SQLException ex) {
            throw new ClusterJException(
                    "initializeSchema threw exception on " + statement, ex);
        }
    }

    protected void dumpSystemProperties() {
        Properties sysprops =System.getProperties();
        List<Entry<Object, Object>> entries = new ArrayList<Entry<Object, Object>>(sysprops.entrySet());
        Collections.sort(entries, new Comparator<Entry<Object, Object>>() {
            public int compare(Entry<Object, Object> o1, Entry<Object, Object> o2) {
                return ((String)o1.getKey()).compareToIgnoreCase((String)o2.getKey());
            }
        });
        for (Iterator<Entry<Object, Object>> iterator = entries.iterator();iterator.hasNext();) {
            Entry<Object,Object> entry = iterator.next();
            System.out.println("key: " + entry.getKey() + "; value: "+ entry.getValue());
        }
    }

    protected void initializeErrorMessages() {
        if (errorMessages == null) {
            errorMessages = new StringBuffer();
            errorMessages.append(NL);
        }
    }

    protected void errorIfNotEqual(String message, Object expected, Object actual) {
        if (!expected.equals(actual)) {
            initializeErrorMessages();
            errorMessages.append(message + NL);
            errorMessages.append("Expected: " + expected.toString()
                    + " actual: " + actual.toString() + NL);
        }
    }

    protected void error(String message) {
        initializeErrorMessages();
        errorMessages.append(message + NL);
    }

    protected void failOnError() {
        if (errorMessages != null) {
            fail(errorMessages.toString());
        }
    }

}
