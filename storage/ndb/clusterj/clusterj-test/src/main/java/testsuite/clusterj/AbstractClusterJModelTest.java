/*
   Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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

package testsuite.clusterj;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Calendar;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TimeZone;
import java.util.TreeSet;

import com.mysql.clusterj.Session;

import testsuite.clusterj.model.AllPrimitives;
import testsuite.clusterj.model.Dn2id;
import testsuite.clusterj.model.Employee;
import testsuite.clusterj.model.IdBase;

public abstract class AbstractClusterJModelTest extends AbstractClusterJTest {

    /** The local system default time zone, which is reset by resetLocalSystemDefaultTimeZone */
    protected static TimeZone localSystemTimeZone = TimeZone.getDefault();

    /** ONE_SECOND is the number of milliseconds in one second. */
    protected static final long ONE_SECOND = 1000L;

    /** ONE_MINUTE is the number of milliseconds in one minute. */
    protected static final long ONE_MINUTE = 1000L * 60L;

    /** ONE_HOUR is the number of milliseconds in one hour. */
    protected static final long ONE_HOUR = 1000L * 60L * 60L;

    /** TEN_HOURS is the number of milliseconds in ten hours. */
    protected static final long TEN_HOURS = 1000L * 60L * 60L * 10L;

    /** ONE_DAY is the number of milliseconds in one day. */
    protected static final long ONE_DAY = 1000L * 60L * 60L * 24L;

    /** Convert year, month, day, hour, minute, second into milliseconds after the Epoch, UCT.
     * @param year the year
     * @param month the month (0 for January)
     * @param day the day of the month
     * @param hour the hour of the day
     * @param minute the minute
     * @param second the second
     * @return
     */
    protected static long getMillisFor(int year, int month, int day, int hour, int minute, int second) {
        Calendar calendar = Calendar.getInstance();
        calendar.clear();
        calendar.set(Calendar.YEAR, year);
        calendar.set(Calendar.MONTH, month);
        calendar.set(Calendar.DATE, day);
        calendar.set(Calendar.HOUR, hour);
        calendar.set(Calendar.MINUTE, minute);
        calendar.set(Calendar.SECOND, second);
        calendar.set(Calendar.MILLISECOND, 0);
        long result = calendar.getTimeInMillis();
        return result;
    }

    /** Convert year, month, day into milliseconds after the Epoch, UCT.
     * Set hours, minutes, seconds, and milliseconds to zero.
     * @param year the year
     * @param month the month (0 for January)
     * @param day the day of the month
     * @return
     */
    protected static long getMillisFor(int year, int month, int day) {
        Calendar calendar = Calendar.getInstance();
        calendar.clear();
        calendar.set(Calendar.YEAR, year);
        calendar.set(Calendar.MONTH, month);
        calendar.set(Calendar.DATE, day);
        calendar.set(Calendar.HOUR, 0);
        calendar.set(Calendar.MINUTE, 0);
        calendar.set(Calendar.SECOND, 0);
        calendar.set(Calendar.MILLISECOND, 0);
        long result = calendar.getTimeInMillis();
        return result;
    }

    /** Convert days, hours, minutes, and seconds into milliseconds after the Epoch, UCT.
     * Date is index origin 1 so add one to the number of days. Default year and month,
     * as these are assumed by Calendar to be the Epoch.
     * @param day the number of days
     * @param hour the hour (or number of hours)
     * @param minute the minute (or number of minutes)
     * @param second the second (or number of seconds)
     * @return millis past the Epoch UCT
     */
    protected static long getMillisFor(int days, int hour, int minute, int second) {
        Calendar calendar = Calendar.getInstance();
        calendar.clear();
        calendar.set(Calendar.DATE, days + 1);
        calendar.set(Calendar.HOUR, hour);
        calendar.set(Calendar.MINUTE, minute);
        calendar.set(Calendar.SECOND, second);
        calendar.set(Calendar.MILLISECOND, 0);
        long result = calendar.getTimeInMillis();
        return result;
    }

    /** A1 values. */
    String[] a1values = new String[]{"dc=abc", "dc=prs", "dc=xyz"};

    protected List<Employee> employees;

    protected List<Dn2id> dn2ids;

    protected static Object[] dn2idPK = setupDn2idPK();

    /** The instances used in the tests, generated by generateInstances */
    protected List<IdBase> instances = new ArrayList<IdBase>();

    /** List of expected results, generated by generateInstances */
    private List<Object[]> expected = null;

    /** The column descriptors as provided by subclasses */
    ColumnDescriptor[] columnDescriptors = null;

    /** The class loader for the domain object type */
    protected ClassLoader loader;

    public AbstractClusterJModelTest() {
        columnDescriptors = getColumnDescriptors();
    }

    protected boolean getCleanupAfterTest() {
        return true;
    }

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        setAutoCommit(connection, false);
        if (getModelClass() != null && getCleanupAfterTest()) {
            addTearDownClasses(getModelClass());
        }
    }

    /** Reset the local system default time zone to the time zone used
     * by the MySQL server. This guarantees that there is no time zone
     * offset between the time zone in the client and the time zone
     * in the server.
     * @param connection 
     */
    protected static void resetLocalSystemDefaultTimeZone(Connection connection) {
        try {
            PreparedStatement statement = connection.prepareStatement("select @@global.time_zone, @@global.system_time_zone, @@session.time_zone");
            ResultSet rs = statement.executeQuery();
            // there are two columns in the result
            rs.next();
            String globalTimeZone = rs.getString(1);
            String globalSystemTimeZone = rs.getString(2);
            String sessionTimeZone = rs.getString(3);
//            if (debug) System.out.println("Global time zone: " + globalTimeZone + 
//                    " Global system time zone: " + globalSystemTimeZone +" Session time zone: " + sessionTimeZone);
            connection.commit();
            if ("SYSTEM".equalsIgnoreCase(globalTimeZone)) {
                globalTimeZone = globalSystemTimeZone;
            } else {
                globalTimeZone = "GMT" + globalTimeZone;
            }
            localSystemTimeZone = TimeZone.getTimeZone(globalTimeZone);
//            if (debug) System.out.println("Local system time zone set to: " + globalTimeZone + "(" + localSystemTimeZone + ")");
//            TimeZone.setDefault(localSystemTimeZone);
            // get a new connection after setting local default time zone
            // because a connection contains a session calendar used to create Timestamp instances
            connection.close();
        } catch (SQLException e) {
            throw new RuntimeException("setServerTimeZone failed", e);
        }
    }

    protected void setAutoCommit(Connection connection, boolean b) {
        try {
            connection.setAutoCommit(false);
        } catch (SQLException e) {
            throw new RuntimeException("setAutoCommit failed", e);
        }
    }

    protected void createEmployeeInstances(int count) {
        employees = new ArrayList<Employee>(count);
        for (int i = 0; i < count; ++i) {
            Employee emp = session.newInstance(Employee.class);
            emp.setId(i);
            emp.setName("Employee number " + i);
            emp.setAge(i);
            emp.setMagic(i);
            employees.add(emp);
        }
    }

    protected void consistencyCheck(Employee emp) {
        int id = emp.getId();
        String expectedName = "Employee number " + id;
        String actualName = emp.getName();
        if (!expectedName.equals(actualName)) {
//            System.out.println("expected " + dump(expectedName));
//            System.out.println("actual " + dump(actualName));
            error("Employee " + id
                    + " name mismatch; expected length: " + expectedName.length() + "'" + expectedName
                    + "'; actual length: " + actualName.length() + "'" + actualName + "'");
        }
        int actualAge = emp.getAge();
        if (!(actualAge == id)) {
            error("Employee " + id
                    + " age mismatch; expected " + id
                    + "'; actual '" + actualAge);
        }
        int actualMagic = emp.getMagic();
        if (!(actualMagic == id)) {
            error("Employee " + id
                    + " magic mismatch; expected " + id
                    + "'; actual '" + actualMagic);
        }
    }

    protected <T> void consistencyCheck(Iterable<T> instances) {
        for (T instance: instances) {
            if (instance instanceof Employee) {
                consistencyCheck((Employee)instance);
            } else if (instance instanceof Dn2id) {
                consistencyCheck((Dn2id)instance);
            }
        }
    }

    protected void createDn2idInstances(int number) {
        dn2ids = new ArrayList<Dn2id>();
        for (int i = 0; i < number; ++i) {
            Dn2id d = session.newInstance(Dn2id.class);
            d.setEid(i);
            d.setObjectClasses("testObject");
            // XObjectClasses has a NullValue=DEFAULT so don't need to set it
            d.setA0("dc=com");
            // a1 should pick all of the a1values equally
            d.setA1(getA1for(number, i));
            d.setA2("ou=people");
            d.setA3(getA3for(i));
            d.setA4("");
            d.setA5("");
            d.setA6("");
            d.setA7("");
            d.setA8("");
            d.setA9("");
            d.setA10("");
            d.setA11("");
            d.setA12("");
            d.setA13("");
            d.setA14("");
            d.setA15("");
            dn2ids.add(d);
        }
    }

    protected void consistencyCheck(Dn2id dn2id) {
        long eid = dn2id.getEid();
        String expected = getA3for(eid);
        String actual = dn2id.getA3();
        if (!expected.equals(actual)) {
            error("Dn2id " + eid
                    + " a3 mismatch; expected '" + expected
                    + "'; actual '" + actual + "'");
        }
    }

    /** Subclasses usually should not override this method to provide the list of expected results */
    protected List<Object[]> getExpected() {
        return expected;
    }

    /** Subclasses must override this method to provide the name of the table for the test */
    protected String getTableName() {
        return null;
    }

    /** Subclasses must override this method to provide the number of instances to create */
    protected int getNumberOfInstances() {
        return 0;
    }

    /** Subclasses must override this method to provide the column descriptors for the test */
    protected ColumnDescriptor[] getColumnDescriptors() {
        return null;
    }

    /** Subclasses must override this method to provide the model class for the test */
    Class<? extends IdBase> getModelClass() {
        return null;
    }

    /** Subclasses must override this method to provide values for rows (i) and columns (j) */
    protected Object getColumnValue(int i, int j) {
        return null;
    }

    /** Write data via JDBC and read back the data via NDB */
    protected void writeJDBCreadNDB() {
        generateInstances(getColumnDescriptors());
        removeAll(getModelClass());
        List<Object[]> result = null;
        writeToJDBC(columnDescriptors, instances);
        result = readFromNDB(columnDescriptors);
        verify("writeJDBCreadNDB", getExpected(), result);
    }

    /** Write data via JDBC and read back the data via JDBC */
    protected void writeJDBCreadJDBC() {
        generateInstances(getColumnDescriptors());
        removeAll(getModelClass());
        List<Object[]> result = null;
        writeToJDBC(columnDescriptors, instances);
        result = readFromJDBC(columnDescriptors);
        verify("writeJDBCreadJDBC", getExpected(), result);
    }

    /** Write data via NDB and read back the data via NDB */
    protected void writeNDBreadNDB() {
        generateInstances(getColumnDescriptors());
        removeAll(getModelClass());
        List<Object[]> result = null;
        writeToNDB(columnDescriptors, instances);
        result = readFromNDB(columnDescriptors);
        verify("writeNDBreadNDB", getExpected(), result);
    }

    /** Write data via NDB and read back the data via JDBC */
    protected void writeNDBreadJDBC() {
        generateInstances(getColumnDescriptors());
        removeAll(getModelClass());
        List<Object[]> result = null;
        writeToNDB(columnDescriptors, instances);
        result = readFromJDBC(columnDescriptors);
        verify("writeNDBreadJDBC", getExpected(), result);
    }

    /** Dump the contents of the expected or actual results of the operation */
    private String dumpListOfObjectArray(List<Object[]> results) {
        StringBuffer result = new StringBuffer(results.size() + " rows\n");
        for (Object[] row: results) {
            result.append("Id: ");
            for (Object column: row) {
                result.append(column);
                result.append(' ');
            }
            result.append('\n');
        }
        return result.toString();
    }

    protected void queryAndVerifyResults(String where, ColumnDescriptor[] columnDescriptors,
            String conditions, Object[] parameters, int... objectIds) {
        List<Object[]> results = queryJDBC(columnDescriptors, conditions, parameters);
        verifyQueryResults(where, results, objectIds);
    }

    /** Read data via JDBC */
    protected List<Object[]> queryJDBC(ColumnDescriptor[] columnDescriptors,
            String conditions, Object[] parameters) {
        getConnection();
        String tableName = getTableName();
        List<Object[]> result = new ArrayList<Object[]>();
        StringBuffer buffer = new StringBuffer("SELECT id");
        for (ColumnDescriptor columnDescriptor: columnDescriptors) {
            buffer.append(", ");
            buffer.append(columnDescriptor.getColumnName());
        }
        buffer.append(" FROM ");
        buffer.append(tableName);
        buffer.append(" WHERE ");
        buffer.append(conditions);
        String statement = buffer.toString();
        if (debug) System.out.println(statement);
        PreparedStatement preparedStatement = null;
        try {
            int p = 1;
            preparedStatement = connection.prepareStatement(statement);
            for (Object parameter: parameters) {
                preparedStatement.setObject(p++, parameter);
            }
            ResultSet rs = preparedStatement.executeQuery();
            while (rs.next()) {
                Object[] row = new Object[columnDescriptors.length + 1];
                int j = 1;
                row[0] = rs.getInt(1);
                for (ColumnDescriptor columnDescriptor: columnDescriptors) {
                    row[j] = columnDescriptor.getResultSetValue(rs, j + 1);
                    ++j;
                }
                result.add(row);
            }
            connection.commit();
        } catch (SQLException e) {
            throw new RuntimeException("Failed to read " + tableName, e);
        }
        if (debug) System.out.println("readFromJDBC: " + dumpObjectArray(result));
        return result;
    }

    /** Dump the contents of the expected or actual results of the operation */
    private String dumpObjectArray(List<Object[]> results) {
        StringBuffer result = new StringBuffer(results.size() + " rows\n");
        for (Object[] row: results) {
            result.append("Id: ");
            for (Object column: row) {
                result.append(column);
                result.append(' ');
            }
            result.append('\n');
        }
        return result.toString();
    }

    protected void verifyQueryResults(String where, List<Object[]> results, int... objectIds) {
        errorIfNotEqual(where + " mismatch in number of results.", objectIds.length, results.size());
        for (Object[] result: results) {
            int id = (Integer)result[0];
            if (Arrays.binarySearch(objectIds, id) < 0) {
                // couldn't find it
                error(where + " result " + id + " not expected.");
            }
        }
    }

    /** Verify that the actual results match the expected results. If not, use the multiple error
     * reporting method errorIfNotEqual defined in the superclass.
     * @param where the location of the verification of results, normally the name of the test method
     * @param expecteds the expected results
     * @param actuals the actual results
     */
    protected void verify(String where, List<Object[]> expecteds, List<Object[]> actuals) {
        if (expecteds.size() != actuals.size()) {
            error(where + " failure on size of results: expected: " + expecteds.size() + " actual: " + actuals.size());
            return;
        }
        for (int i = 0; i < expecteds.size(); ++i) {
            Object[] expected = expecteds.get(i);
            Object[] actual = actuals.get(i);
            errorIfNotEqual(where + " got failure on id for row " + i, i, actual[0]);
            for (int j = 1; j < expected.length; ++j) {
                errorIfNotEqual(where + " got failure to match column data for row "
                        + i + " column " + j,
                        expected[j], actual[j]);
            }
        }
    }

    /** Generated instances to persist. When using JDBC, the data is obtained from the instance
     * via the column descriptors. As a side effect (!) create the list of expected results from read.
     * @param columnDescriptors the column descriptors
     * @return the generated instances
     */
    protected void generateInstances(ColumnDescriptor[] columnDescriptors) {
        Class<? extends IdBase> modelClass = getModelClass();
        expected = new ArrayList<Object[]>();
        instances = new ArrayList<IdBase>();
        IdBase instance = null;
        int numberOfInstances = getNumberOfInstances();
        for (int i = 0; i < numberOfInstances; ++i) {
            // create the instance
            instance = getNewInstance(modelClass);
            instance.setId(i);
            // create the expected result row
            int j = 0;
            for (ColumnDescriptor columnDescriptor: columnDescriptors) {
                Object value = getColumnValue(i, j);
                // set the column value in the instance
                columnDescriptor.setFieldValue(instance, value);
                // set the column value in the expected result
                if (debug) System.out.println("generateInstances set field " + columnDescriptor.getColumnName() + " to value "  + value);
                ++j;
            }
            instances.add(instance);
            Object[] expectedRow = createRow(columnDescriptors, instance);
            expected.add(expectedRow);
        }
        if (debug) System.out.println("Created " + instances.size() + " instances of " + modelClass.getName());
    }

    /** Create a new instance of the parameter interface
     * @param modelClass the interface to instantiate
     * @return an instance of the class
     */
    protected IdBase getNewInstance(Class<? extends IdBase> modelClass) {
        IdBase instance;
        instance = session.newInstance(modelClass);
        return instance;
    }

    /** Write data to JDBC. */
    protected void writeToJDBC(ColumnDescriptor[] columnDescriptors, List<IdBase> instances) {
        String tableName = getTableName();
        StringBuffer buffer = new StringBuffer("INSERT INTO ");
        buffer.append(tableName);
        buffer.append(" (id");
        for (ColumnDescriptor columnDescriptor: columnDescriptors) {
            buffer.append(", ");
            buffer.append(columnDescriptor.getColumnName());
        }
        buffer.append(") VALUES (?");
        for (ColumnDescriptor columnDescriptor: columnDescriptors) {
            buffer.append(", ?");
        }
        buffer.append(")");
        String statement = buffer.toString();
        if (debug) System.out.println(statement);
    
        PreparedStatement preparedStatement = null;
        int i = 0;
        try {
            preparedStatement = connection.prepareStatement(statement);
            if (debug) System.out.println(preparedStatement.toString());
            for (i = 0; i < instances.size(); ++i) {
                IdBase instance = instances.get(i);
                preparedStatement.setInt(1, instance.getId());
                int j = 2;
                for (ColumnDescriptor columnDescriptor: columnDescriptors) {
                    Object value = columnDescriptor.getFieldValue(instance);
                    columnDescriptor.setPreparedStatementValue(preparedStatement, j++, value);
                    if (debug) System.out.println("writeToJDBC set column: " + columnDescriptor.getColumnName() + " to value: " + value);
                }
                preparedStatement.execute();
            }
            connection.commit();
        } catch (SQLException e) {
            throw new RuntimeException("Failed to insert " + tableName + " at instance " + i, e);
        }
    }

    /** Write data via NDB */
    protected void writeToNDB(ColumnDescriptor[] columnDescriptors, List<IdBase> instances) {
        session.currentTransaction().begin();
        session.makePersistentAll(instances);
        session.currentTransaction().commit();
    }

    /** Read data via NDB */
    protected List<Object[]> readFromNDB(ColumnDescriptor[] columnDescriptors) {
        Class<? extends IdBase> modelClass = getModelClass();
        List<Object[]> result = new ArrayList<Object[]>();
        session.currentTransaction().begin();
        for (int i = 0; i < getNumberOfInstances() ; ++i) {
            IdBase instance = session.find(modelClass, i);
            if (instance != null) {
                Object[] row = createRow(columnDescriptors, instance);
                result.add(row);
            }
        }
        session.currentTransaction().commit();
        if (debug) System.out.println("readFromNDB: " + dumpListOfObjectArray(result));
        return result;
    }

    /** Create row data from an instance.
     * @param columnDescriptors the column descriptors describing the data
     * @param instance the instance to extract data from
     * @return the row data representing the instance
     */
    private Object[] createRow(ColumnDescriptor[] columnDescriptors,
            IdBase instance) {
        Object[] row = new Object[columnDescriptors.length + 1];
        row[0] = instance.getId();
        int j = 1;
        for (ColumnDescriptor columnDescriptor: columnDescriptors) {
            row[j++] = columnDescriptor.getFieldValue(instance);
        }
        return row;
    }

    /** Read data via JDBC ordered by id */
    protected List<Object[]> readFromJDBC(ColumnDescriptor[] columnDescriptors) {
        String tableName = getTableName();
        List<Object[]> result = new ArrayList<Object[]>();
        Set<Object[]> rows = new TreeSet<Object[]>(new Comparator<Object[]>(){
            public int compare(Object[] me, Object[] other) {
                return ((Integer)me[0]) - ((Integer)other[0]);
            }
        });
        StringBuffer buffer = new StringBuffer("SELECT id");
        for (ColumnDescriptor columnDescriptor: columnDescriptors) {
            buffer.append(", ");
            buffer.append(columnDescriptor.getColumnName());
        }
        buffer.append(" FROM ");
        buffer.append(tableName);
        String statement = buffer.toString();
        if (debug) System.out.println(statement);
        PreparedStatement preparedStatement = null;
        int i = 0;
        try {
            preparedStatement = connection.prepareStatement(statement);
            ResultSet rs = preparedStatement.executeQuery();
            while (rs.next()) {
                Object[] row = new Object[columnDescriptors.length + 1];
                int j = 1;
                row[0] = rs.getInt(1);
                for (ColumnDescriptor columnDescriptor: columnDescriptors) {
                    row[j] = columnDescriptor.getResultSetValue(rs, j + 1);
                    ++j;
                }
                ++i;
                rows.add(row);
            }
            connection.commit();
        } catch (SQLException e) {
            throw new RuntimeException("Failed to read " + tableName + " at instance " + i, e);
        }
        result = new ArrayList<Object[]>(rows);
        if (debug) System.out.println("readFromJDBC: " + dumpListOfObjectArray(result));
        return result;
    }

    @SuppressWarnings("unchecked") // cast proxy to T
    protected <T> T proxyFor (final Class<T> cls) {
        InvocationHandler handler = new InvocationHandler() {
            private Map<String, Object> values = new HashMap<String, Object>();
            public Object invoke(Object instance, Method method, Object[] args)
                    throws Throwable {
                String methodName = method.getName();
                String propertyName = methodName.substring(3);
                String methodPrefix = methodName.substring(0, 3);
                if ("get".equals(methodPrefix)) {
                    return values.get(propertyName);
                } else if ("set".equals(methodPrefix)) {
                    values.put(propertyName, args[0]);
                    return null;
                }
                // error
                throw new RuntimeException("Not a get/set method: " + methodName);
            }
            
        };
        Object proxy = Proxy.newProxyInstance(loader, new Class[] {cls}, handler);
        return (T)proxy;
    }

    /** This class describes columns and fields for a table and model class. 
     * A subclass will instantiate instances of this class and provide handlers to
     * read and write fields and columns via methods defined in the instance handler.
     */
    protected static class ColumnDescriptor {

        private String columnName;
    
        protected InstanceHandler instanceHandler;
    
        public String getColumnName() {
            return columnName;
        }
    
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return instanceHandler.getResultSetValue(rs, j);
        }
    
        public Object getFieldValue(IdBase instance) {
            return instanceHandler.getFieldValue(instance);
        }
    
        public void setFieldValue(IdBase instance, Object value) {
            this.instanceHandler.setFieldValue(instance, value);
        }
    
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            instanceHandler.setPreparedStatementValue(preparedStatement, j, value);
        }
    
        public ColumnDescriptor(String name, InstanceHandler instanceHandler) {
            this.columnName = name;
            this.instanceHandler = instanceHandler;
        }
    }

    protected interface InstanceHandler {
        void setFieldValue(IdBase instance, Object value);
        Object getResultSetValue(ResultSet rs, int j)
                throws SQLException;
        Object getFieldValue(IdBase instance);
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException;
    }

    protected String getA1for(int number, int index) {
        int a1factor = 1 + number/a1values.length;
        return a1values[index/a1factor];
    }

    protected String getA3for(long i) {
        return "employeenumber=100000" + i;
    }

    protected void createAllPrimitivesInstances(int number) {
        createAllPrimitivesInstances(session, number);
    }

    protected void createAllPrimitivesInstances(Session session, int number) {
        for (int i = 0; i < number; ++i) {
            AllPrimitives instance = createAllPrimitiveInstance(session, i);
            instances.add(instance);
        }
    }

    protected AllPrimitives createAllPrimitiveInstance(Session session, int i) {
        AllPrimitives instance = session.newInstance(AllPrimitives.class, i);
        initialize(instance, i);
        return instance;
    }

    protected void initialize(AllPrimitives instance, int i) {
        instance.setInt_not_null_hash(i);
        instance.setInt_not_null_btree(i);
        instance.setInt_not_null_both(i);
        instance.setInt_not_null_none(i);
        instance.setInt_null_hash(i);
        instance.setInt_null_btree(i);
        instance.setInt_null_both(i);
        instance.setInt_null_none(i);

        instance.setLong_not_null_hash((long)i);
        instance.setLong_not_null_btree((long)i);
        instance.setLong_not_null_both((long)i);
        instance.setLong_not_null_none((long)i);
        instance.setLong_null_hash((long)i);
        instance.setLong_null_btree((long)i);
        instance.setLong_null_both((long)i);
        instance.setLong_null_none((long)i);

        instance.setByte_not_null_hash((byte)i);
        instance.setByte_not_null_btree((byte)i);
        instance.setByte_not_null_both((byte)i);
        instance.setByte_not_null_none((byte)i);
        instance.setByte_null_hash((byte)i);
        instance.setByte_null_btree((byte)i);
        instance.setByte_null_both((byte)i);
        instance.setByte_null_none((byte)i);

        instance.setShort_not_null_hash((short)i);
        instance.setShort_not_null_btree((short)i);
        instance.setShort_not_null_both((short)i);
        instance.setShort_not_null_none((short)i);
        instance.setShort_null_hash((short)i);
        instance.setShort_null_btree((short)i);
        instance.setShort_null_both((short)i);
        instance.setShort_null_none((short)i);
    }

    protected static Object[] setupDn2idPK() {
        Object[] result = new Object[16];
        result[0] = "dc=com";
        // pk[1] changes and is set inside loop
        result[1] = "dc=example";
        result[2] = "ou=people";
        // pk[3] changes and is set inside loop
        result[4] = "";
        result[5] = "";
        result[6] = "";
        result[7] = "";
        result[8] = "";
        result[9] = "";
        result[10] = "";
        result[11] = "";
        result[12] = "";
        result[13] = "";
        result[14] = "";
        result[15] = "";
        return result;
    }

}
