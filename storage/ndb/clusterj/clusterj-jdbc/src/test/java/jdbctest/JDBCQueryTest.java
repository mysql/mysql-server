package jdbctest;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import com.mysql.clusterj.Session;

import testsuite.clusterj.AbstractClusterJModelTest;
import testsuite.clusterj.model.AllPrimitives;

public abstract class JDBCQueryTest extends AbstractClusterJModelTest {

    @Override
    public void localSetUp() {
        // initialize the jdbc driver
        super.localSetUp();
        // delete instances
        deleteAll();
        // create instances
        createInstances(getNumberOfInstances());
        commit(connection);
    }

    @Override
    public void localTearDown() {
        deleteAll();
    }

    abstract public void createInstances(int numberOfInstances);
    
    public void createAllPrimitiveInstance(int i) {
        String sql = "insert into allprimitives (id, "
            + "int_not_null_hash,"
            + "int_not_null_btree,"
            + "int_not_null_both,"
            + "int_not_null_none,"
            + "int_null_hash,"
            + "int_null_btree,"
            + "int_null_both,"
            + "int_null_none,"
            + "byte_not_null_hash,"
            + "byte_not_null_btree,"
            + "byte_not_null_both,"
            + "byte_not_null_none,"
            + "byte_null_hash,"
            + "byte_null_btree,"
            + "byte_null_both,"
            + "byte_null_none,"
            + "short_not_null_hash,"
            + "short_not_null_btree,"
            + "short_not_null_both,"
            + "short_not_null_none,"
            + "short_null_hash,"
            + "short_null_btree,"
            + "short_null_both,"
            + "short_null_none,"
            + "long_not_null_hash,"
            + "long_not_null_btree,"
            + "long_not_null_both,"
            + "long_not_null_none,"
            + "long_null_hash,"
            + "long_null_btree,"
            + "long_null_both,"
            + "long_null_none"
            + " values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?" +
                        ", ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
        if (getDebug()) System.out.println(sql);
        PreparedStatement statement = prepareStatement(connection, sql);
        int actual = executeUpdate(statement, Integer.valueOf(i),
                Integer.valueOf(i), Integer.valueOf(i), Integer.valueOf(i), Integer.valueOf(i),
                Integer.valueOf(i), Integer.valueOf(i), Integer.valueOf(i), Integer.valueOf(i),
                Byte.valueOf((byte)i), Byte.valueOf((byte)i), Byte.valueOf((byte)i), Byte.valueOf((byte)i), 
                Byte.valueOf((byte)i), Byte.valueOf((byte)i), Byte.valueOf((byte)i), Byte.valueOf((byte)i), 
                Short.valueOf((short)i), Short.valueOf((short)i), Short.valueOf((short)i), Short.valueOf((short)i),
                Short.valueOf((short)i), Short.valueOf((short)i), Short.valueOf((short)i), Short.valueOf((short)i),
                Long.valueOf((long)i), Long.valueOf((long)i), Long.valueOf((long)i), Long.valueOf((long)i),
                Long.valueOf((long)i), Long.valueOf((long)i), Long.valueOf((long)i), Long.valueOf((long)i));
        errorIfNotEqual("createAllPrimitiveInstance: Mismatch on number of instances inserted ", 1, actual);
    }

    /** Most query tests use the same number of instances (10).
     * 
     */
    @Override
    protected int getNumberOfInstances() {
        return 10;
    }

    protected void betweenQuery(String column, String expectedIndex, Object low, Object high, int... expected) {
        String sql = "select id from " + tableName() + " where " + column + " between ? and ?";
        if(getDebug()) System.out.println(sql);
        PreparedStatement statement = prepareStatement(connection, sql);
        int[] actual = executeQuery(statement, low, high);
        commit(connection);
        Arrays.sort(actual);
        Arrays.sort(expected);
        errorIfNotEqual("betweenQuery: Mismatch on betweenQuery", expected, actual);
    }

    protected void equalQuery(String column, String expectedIndex, int parameter, int... expected) {
        String sql = "select id from " + tableName() + " where " + column + " = ?";
        if (getDebug()) System.out.println(sql);
        PreparedStatement statement = prepareStatement(connection, sql);
        int[] actual = executeQuery(statement, parameter);
        commit(connection);
        errorIfNotEqual("equalQuery: Mismatch on equalQuery", expected, actual);
    }

    protected void deleteAll() {
        String sql = "delete from " + tableName();
        if (getDebug()) System.out.println(sql);
        PreparedStatement statement = prepareStatement(connection, sql);
        int actual = executeUpdate(statement);
        if (getDebug()) System.out.println("deleteAll deleted " + actual + " instances.");
        commit(connection);
    }

    protected void deleteEqualQuery(String column, String expectedIndex, int i, int expected) {
        String sql = "delete from " + tableName() + " where " + column + " = ?";
        if (getDebug()) System.out.println(sql);
        PreparedStatement statement = prepareStatement(connection, sql);
        int actual = executeUpdate(statement, i);
        commit(connection);
        errorIfNotEqual("deleteEqualQuery: Mismatch on number of instances deleted ", expected, actual);
    }

    protected void deleteGreaterThanAndLessThanQuery(String column, String expectedIndex, int i, int j, int expected) {
        String sql = "delete from " + tableName() + " where " + column + " > ? and " + column + " < ?";
        if (getDebug()) System.out.println(sql);
        PreparedStatement statement = prepareStatement(connection, sql);
        int actual = executeUpdate(statement, i, j);
        commit(connection);
        errorIfNotEqual("deleteGreaterThanAndLessThanQuery: Mismatch on number of instances deleted ",
                expected, actual);
    }

    private PreparedStatement prepareStatement(Connection connection, String sql) {
        try {
            return connection.prepareStatement(sql);
        } catch (SQLException e) {
            error("Caught exception " + e.getMessage());
            return null;
        }
    }

    private void commit(Connection connection) {
        try {
            connection.commit();
        } catch (SQLException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
    }

    private int[] executeQuery(PreparedStatement statement, Object... parameters) {
        int[] result = null;
        try {
            int index = 1;
            for (Object parameter: parameters) {
                setObject(statement, index++, parameter);
            }
            List<Integer> results = new ArrayList<Integer>();
            ResultSet resultSet = statement.executeQuery();
            while (resultSet.next()) {
                results.add(resultSet.getInt(1));
            }
            result = new int[results.size()];
            for (int i = 0; i < results.size(); ++i) {
                result[i] = results.get(i);
            }
        } catch (SQLException e) {
            e.printStackTrace();
        }
        return result;
    }

    private void setObject(PreparedStatement statement, int parameterIndex, Object x) throws SQLException {
        if (x instanceof Integer) {
            statement.setInt(parameterIndex, (Integer)x);
            return;
        } else if (x instanceof Byte) {
            statement.setByte(parameterIndex, (Byte)x);
            return;
        } else if (x instanceof Short) {
            statement.setShort(parameterIndex, (Short)x);
            return;
        } else if (x instanceof Long) {
            statement.setLong(parameterIndex, (Long)x);
            return;
        }
        throw new RuntimeException("Object is of unsupported type " + x.getClass().getName());
    }

    private int executeUpdate(PreparedStatement statement, Object... parameters) {
        int result = 0;
        try {
            int index = 1;
            for (Object parameter: parameters) {
                setObject(statement, index++, parameter);
            }
            result = statement.executeUpdate();
        } catch (SQLException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
        return result;
    }

    public String tableName() {
        return null;
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

}
