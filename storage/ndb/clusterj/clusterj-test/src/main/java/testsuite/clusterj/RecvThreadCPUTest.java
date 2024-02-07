/*
   Copyright (c) 2017, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package testsuite.clusterj;

import java.util.Arrays;
import java.util.Properties;

import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.Constants;
import com.mysql.clusterj.SessionFactory.State;

public class RecvThreadCPUTest extends AbstractClusterJTest {

    Properties testProperties = new Properties();
    enum BindCpuSupport {UNDEFINED, SUPPORTED, NOT_SUPPORTED};
    static BindCpuSupport bindCPUsupport = BindCpuSupport.UNDEFINED;

    @Override
    protected void localSetUp() {
        // close any existing session factory
        closeAllExistingSessionFactories();
        // checking if CPU set is supported in the system
        if (bindCPUsupport != BindCpuSupport.UNDEFINED) {
            // Already checked
            return;
        }
        try {
            createSessionFactory();
            sessionFactory.setRecvThreadCPUids(new short[] {0});
            bindCPUsupport = BindCpuSupport.SUPPORTED;
        } catch (Exception ex) {
            if (ex.getMessage().matches("Binding the receiver thread to CPU is not supported in this environment.")) {
                bindCPUsupport = BindCpuSupport.NOT_SUPPORTED;
            } else {
                throw ex;
            }
        }
        destroySessionFactory();
    }

    /**
     * A test to check the default cpu bind settings and to test the api calls
     * to change those values.
     */
    public void testRecvThreadCpuLockingApis() {
        if (bindCPUsupport == BindCpuSupport.NOT_SUPPORTED) {
            // no need to run test if CPU bind is not supported
            return;
        }
        // use a custom dummy database to force creation of new session factory
        testProperties.put(Constants.PROPERTY_CLUSTER_DATABASE, "testDb1");
        // create a session factory with default cpu locking properties
        createSessionFactoryAndVerify();
        // test locking the receive threads to cpu
        errorIfNotEqual("Failure in getting the default cpu id", new short[]{-1},
                sessionFactory.getRecvThreadCPUids());
        setRecvThreadCPUidsAndVerify(new short[] {0});
        setRecvThreadCPUidsAndVerify(new short[] {-1});
        setRecvThreadCPUidsAndFail(new short[] {-10}, "The cpuid .* is not valid.*");
        setRecvThreadCPUidsAndFail(new short[] {0, 1}, ".*The number of cpu ids must match the connection pool size.");

        // test the activation threshold settings
        errorIfNotEqual("Failure in getting default recv thread activation threshold", 8,
                sessionFactory.getRecvThreadActivationThreshold());
        setRecvThreadActivationThresholdAndVerify(0);
        setRecvThreadActivationThresholdAndVerify(3);
        setRecvThreadActivationThresholdAndVerify(8);
        setRecvThreadActivationThresholdAndVerify(15);
        setRecvThreadActivationThresholdAndVerify(16);
        setRecvThreadActivationThresholdAndFail(-1);
        destroySessionFactory();
        failOnError();
    }

    /**
     * A test to check the receive thread cpu properties.
     */
    public void testRecvThreadCpuProperties() {
        if (bindCPUsupport == BindCpuSupport.NOT_SUPPORTED) {
            // no need to run test if CPU bind is not supported
            return;
        }
        // use a custom dummy database to force creation of new session factory
        testProperties.put(Constants.PROPERTY_CLUSTER_DATABASE, "testDb2");
        // create session factory with modified cpu locking properties
        testProperties.put(Constants.PROPERTY_CONNECTION_POOL_RECV_THREAD_CPUIDS, "0");
        testProperties.put(Constants.PROPERTY_CONNECTION_POOL_RECV_THREAD_ACTIVATION_THRESHOLD, 4);
        createSessionFactoryAndVerify();
        if (sessionFactory != null) {
            // check if the session factory was created with proper settings
            errorIfNotEqual("Sessionfactory created with wrong properties.", new short[]{0},
                    sessionFactory.getRecvThreadCPUids());
            errorIfNotEqual("Sessionfactory created with wrong properties.", 4,
                    sessionFactory.getRecvThreadActivationThreshold());
            setRecvThreadActivationThresholdAndVerify(3);
            setRecvThreadCPUidsAndVerify(new short[] {-1});
        }
        destroySessionFactory();

        // use a custom dummy database to force creation of new session factory
        testProperties.put(Constants.PROPERTY_CLUSTER_DATABASE, "testDb3");
        // create a session factory with a connection pool and test
        testProperties.put(Constants.PROPERTY_CONNECTION_POOL_SIZE, 2);
        createSessionFactoryAndVerify();
        if (sessionFactory != null) {
            setRecvThreadCPUidsAndVerify(new short[]{0, -1});
            setRecvThreadCPUidsAndFail(new short[]{-1, 999},
                    "The cpuid .* is not valid.*");
            errorIfNotEqual("setRecvThreadCPUids failed to restore CPUids setting on error.", new short[]{0, -1},
                    sessionFactory.getRecvThreadCPUids());
        }
        destroySessionFactory();

        // disable connection pooling and test the cpu locking properties
        testProperties.put(Constants.PROPERTY_CONNECTION_POOL_SIZE, 0);
        // create session factory with modified cpu locking properties
        testProperties.put(Constants.PROPERTY_CONNECTION_POOL_RECV_THREAD_CPUIDS, "0");
        testProperties.put(Constants.PROPERTY_CONNECTION_POOL_RECV_THREAD_ACTIVATION_THRESHOLD, 4);
        createSessionFactoryAndVerify();
        if (sessionFactory != null) {
            // check if the session factory was created with proper settings
            errorIfNotEqual("Sessionfactory created with wrong properties.", new short[]{0},
                sessionFactory.getRecvThreadCPUids());
            errorIfNotEqual("Sessionfactory created with wrong properties.", 4,
                sessionFactory.getRecvThreadActivationThreshold());
            setRecvThreadActivationThresholdAndVerify(3);
            setRecvThreadCPUidsAndVerify(new short[] {-1});
        }
        destroySessionFactory();

        // use a custom dummy database to force creation of new session factory
        testProperties.put(Constants.PROPERTY_CLUSTER_DATABASE, "testDb4");
        // negative tests with invalid property settings
        testProperties.put(Constants.PROPERTY_CONNECTION_POOL_RECV_THREAD_CPUIDS, "999");
        createSessionFactoryAndFail(".*The cpuid .* is not valid.*");
        testProperties.put(Constants.PROPERTY_CONNECTION_POOL_RECV_THREAD_CPUIDS, "0,0");
        createSessionFactoryAndFail(".*The number of cpu ids must match the connection pool size.");
        testProperties.put(Constants.PROPERTY_CONNECTION_POOL_RECV_THREAD_CPUIDS, "cpu1");
        createSessionFactoryAndFail("The cpuids property .* is invalid.*");
        destroySessionFactory();

        // use a custom dummy database to force creation of new session factory
        testProperties.put(Constants.PROPERTY_CLUSTER_DATABASE, "testDb5");
        testProperties.put(Constants.PROPERTY_CONNECTION_POOL_RECV_THREAD_ACTIVATION_THRESHOLD, -1);
        createSessionFactoryAndFail(".*The activation threshold .* is not valid.*");
        testProperties.put(Constants.PROPERTY_CONNECTION_POOL_RECV_THREAD_ACTIVATION_THRESHOLD, "8,8");
        createSessionFactoryAndFail("Property .* must be numeric.");
        destroySessionFactory();

        // disable connection pooling and verify properties are validated
        testProperties.put(Constants.PROPERTY_CONNECTION_POOL_SIZE, 0);
        testProperties.put(Constants.PROPERTY_CONNECTION_POOL_RECV_THREAD_CPUIDS, "cpu1");
        createSessionFactoryAndFail("The cpuids property .* is invalid.*");
        testProperties.put(Constants.PROPERTY_CONNECTION_POOL_RECV_THREAD_CPUIDS, "0,0");
        createSessionFactoryAndFail("The cpuids property specifies multiple cpu ids .*");
        destroySessionFactory();

        failOnError();
    }

    private void createSessionFactoryAndVerify() {
        String errorMessage = "Error creating session factory with props : " + testProperties.toString();
        try {
            createSessionFactory();
        } catch (Exception ex) {
            error(errorMessage);
            error("Caused by:\n"+ ex.getMessage());
        }
        errorIfEqual(errorMessage, sessionFactory, null);
        if (sessionFactory != null) {
            errorIfNotEqual(errorMessage, sessionFactory.currentState(), State.Open);
        }
    }

    @Override
    protected void createSessionFactory() {
        // Verify that the session factory from previous run was cleaned up
        if (sessionFactory != null) {
            throw new RuntimeException("Sessionfactory from previous run not cleaned up");
        }
        // Use all the properties and create a session factory
        Properties modifiedProperties = new Properties();
        loadProperties();
        modifiedProperties.putAll(props);
        modifiedProperties.putAll(testProperties);
        if (debug) System.out.println("createSessionFactory props: " + modifiedProperties);
        sessionFactory = ClusterJHelper.getSessionFactory(modifiedProperties);
    }

    private void destroySessionFactory() {
        if (sessionFactory != null) {
            sessionFactory.close();
            sessionFactory = null;
        }
        testProperties.clear();
    }

    private void createSessionFactoryAndFail(String exceptionPattern) {
        Exception caughtException = null;
        try {
            createSessionFactory();
        } catch (Exception ex) {
            caughtException = ex;
        }
        verifyException("Creating session factory with invalid properties - " + testProperties.toString(),
                caughtException, exceptionPattern);
    }

    private void setRecvThreadCPUidsAndVerify(short[] cpuids) {
        try {
            sessionFactory.setRecvThreadCPUids(cpuids);
            errorIfNotEqual("Failure in binding the recv thread to the cpu ids.", cpuids,
                    sessionFactory.getRecvThreadCPUids());
        } catch (Exception ex) {
            error("Failure in binding the recv thread to the cpu ids : " + Arrays.toString(cpuids));
            error("Caused by:\n" + ex.getMessage());
        }
    }

    private void setRecvThreadCPUidsAndFail(short[] cpuids, String exceptionPattern) {
        Exception caughtException = null;
        try {
            sessionFactory.setRecvThreadCPUids(cpuids);
        } catch (Exception ex) {
            caughtException = ex;
        }
        verifyException("Setting invalid cpuids - '"+ Arrays.toString(cpuids) + "'",
                caughtException, exceptionPattern);
    }

    private void setRecvThreadActivationThresholdAndVerify(int threshold) {
        int expectedThreshold = (threshold < 16)? threshold : 256;
        try {
            sessionFactory.setRecvThreadActivationThreshold(threshold);
            errorIfNotEqual("Error in setting/getting recv thread activation threshold",
                    expectedThreshold, sessionFactory.getRecvThreadActivationThreshold());
        } catch (Exception ex) {
            error(ex.getMessage());
        }
    }

    private void setRecvThreadActivationThresholdAndFail(int threshold) {
        Exception caughtException = null;
        try {
            sessionFactory.setRecvThreadActivationThreshold(threshold);
        } catch (Exception ex) {
            caughtException = ex;
        }
        verifyException("Setting invalid activation threshold '" + threshold + "'",
                caughtException, "The activation threshold .* is not valid.*");
    }

    private void errorIfNotEqual(String message, short[] expected, short[] actual) {
        if (expected.length == 0 && actual.length == 0) {
            return;
        }
        if (expected.length != 0 && Arrays.equals(expected, actual)) {
            return;
        } else {
            error(message);
            error("Expected: " + Arrays.toString(expected)
                    + " actual: " + Arrays.toString(actual));
        }
    }
}
