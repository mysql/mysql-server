/*
   Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.

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

import java.lang.reflect.Method;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Timestamp;

import org.junit.Ignore;

import testsuite.clusterj.model.IdBase;
import testsuite.clusterj.model.Timestamp2AsSqlTimestampTypes;

/** Test that Timestamps with fractional seconds can be read and written in memory.
 * Schema
 *
drop table if exists timestamp2types;
create table timestamp2types (
id int not null primary key auto_increment,

timestampx timestamp    null,
timestamp1 timestamp(1) null,
timestamp2 timestamp(2) null,
timestamp3 timestamp(3) null,
timestamp4 timestamp(4) null,
timestamp5 timestamp(5) null,
timestamp6 timestamp(6) null

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

 */
public class Timestamp2AsSqlTimestampTypesTest extends AbstractClusterJModelTest {

    @Override
    public void localSetUp() {
        super.localSetUp();
    }

    static int NUMBER_OF_INSTANCES = 10;

    @Override
    protected boolean getDebug() {
        return false;
    }

    @Override
    protected int getNumberOfInstances() {
        return NUMBER_OF_INSTANCES;
    }

    @Override
    protected String getTableName() {
        return "timestamp2types";
    }

    /** Subclasses override this method to provide the model class for the test */
    @Override
    Class<? extends IdBase> getModelClass() {
        return Timestamp2AsSqlTimestampTypes.class;
    }

    @Override
    protected boolean getCleanupAfterTest() {
        return true;
    }

    /** Subclasses override this method to provide values for rows (i) and columns (j) */
    @Override
    protected Object getColumnValue(int i, int j) {
        return new Timestamp(getMillisFor(1980, 0, 1, i, i, j));
    }

    long T1980_01_01_12_30_30 = getMillisFor(1980, 0, 1, 12, 30, 30);

    public void testTimestampx() {
        setget("testTimestampx", "Timestampx", T1980_01_01_12_30_30, 0, T1980_01_01_12_30_30, 0);
        setget("testTimestampx", "Timestampx", T1980_01_01_12_30_30, 100000000, T1980_01_01_12_30_30, 0);
    }

    public void testTimestamp0() {
        setget("testTimestamp0", "Timestamp0", T1980_01_01_12_30_30, 0, T1980_01_01_12_30_30, 0);
        setget("testTimestamp0", "Timestamp0", T1980_01_01_12_30_30, 100000000, T1980_01_01_12_30_30, 0);
    }

    public void testTimestamp1() {
        setget("testTimestamp1", "Timestamp1", T1980_01_01_12_30_30 + 100L, 0, T1980_01_01_12_30_30 +100, 100000000);
        setget("testTimestamp1", "Timestamp1", T1980_01_01_12_30_30, 100000000, T1980_01_01_12_30_30 +100, 100000000);
        setget("testTimestamp1", "Timestamp1", T1980_01_01_12_30_30 + 110L, 0, T1980_01_01_12_30_30 + 100L, 100000000);
        setget("testTimestamp1", "Timestamp1", T1980_01_01_12_30_30, 110000000, T1980_01_01_12_30_30 + 100L, 100000000);
    }

    public void testTimestamp2() {
        setget("testTimestamp2", "Timestamp2", T1980_01_01_12_30_30, 220000000, T1980_01_01_12_30_30 + 220L, 220000000);
        setget("testTimestamp2", "Timestamp2", T1980_01_01_12_30_30 + 220L, 0, T1980_01_01_12_30_30 + 220L, 220000000);
        setget("testTimestamp2", "Timestamp2", T1980_01_01_12_30_30, 222000000, T1980_01_01_12_30_30 + 220L, 220000000);
        setget("testTimestamp2", "Timestamp2", T1980_01_01_12_30_30 + 222L, 0, T1980_01_01_12_30_30 + 220L, 220000000);
    }

    public void testTimestamp3() {
        setget("testTimestamp3", "Timestamp3", T1980_01_01_12_30_30, 333000000, T1980_01_01_12_30_30 + 333L, 333000000);
        setget("testTimestamp3", "Timestamp3", T1980_01_01_12_30_30 + 333L, 0, T1980_01_01_12_30_30 + 333L, 333000000);
        setget("testTimestamp3", "Timestamp3", T1980_01_01_12_30_30, 333300000, T1980_01_01_12_30_30 + 333L, 333000000);
    }

    public void testTimestamp4() {
        setget("testTimestamp4", "Timestamp4", T1980_01_01_12_30_30, 444000000, T1980_01_01_12_30_30 + 444L, 444000000);
        setget("testTimestamp4", "Timestamp4", T1980_01_01_12_30_30, 444400000, T1980_01_01_12_30_30 + 444L, 444000000);
    }

    public void testTimestamp5() {
        setget("testTimestamp5", "Timestamp5", T1980_01_01_12_30_30, 555000000, T1980_01_01_12_30_30 + 555L, 555000000);
        setget("testTimestamp5", "Timestamp5", T1980_01_01_12_30_30, 555550000, T1980_01_01_12_30_30 + 555L, 555000000);
    }

    public void testTimestamp6() {
        setget("testTimestamp6", "Timestamp6", T1980_01_01_12_30_30, 666000000, T1980_01_01_12_30_30 + 666L, 666000000);
        setget("testTimestamp6", "Timestamp6", T1980_01_01_12_30_30, 666666600, T1980_01_01_12_30_30 + 666L, 666000000);
    }

    protected void setget(String where, String property,
            long millisIn, int nanosIn, long expectedMillis, int expectedNanos) {
        Method getMethod = getDeclaredMethod(Timestamp2AsSqlTimestampTypes.class, "get"+property);
        Method setMethod = getDeclaredMethod(Timestamp2AsSqlTimestampTypes.class, "set"+property, Timestamp.class);
        Timestamp2AsSqlTimestampTypes instance = session.newInstance(Timestamp2AsSqlTimestampTypes.class);
        Timestamp source = new Timestamp(millisIn);
        if (nanosIn != 0) source.setNanos(nanosIn);
        invoke(setMethod, instance, source);
        Timestamp field = (Timestamp)invoke(getMethod, instance);
        long resultMillis = field.getTime();
        int resultNanos = field.getNanos();
        errorIfNotEqual(where + " result millis", expectedMillis, resultMillis);
        errorIfNotEqual(where + " result nanos", expectedNanos, resultNanos);
        if (expectedMillis != resultMillis || expectedNanos != resultNanos) {
            System.out.println(where + " source.toString " + source);
            System.out.println(where + " source.getTime() " + source.getTime());
            System.out.println(where + " source.getNanos() " + source.getNanos());
            System.out.println(where + " instance.toString " + field);
            System.out.println(where + " instance.getTime() " + field.getTime());
            System.out.println(where + " instance.getNanos() " + field.getNanos());
        }
        session.persist(instance);
        failOnError();
    }

    protected Object invoke(Method method, Object instance) {
        try {
            return method.invoke(instance);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }
    protected Object invoke(Method method, Object instance, Object param) {
        try {
            return method.invoke(instance, param);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }
    protected Method getDeclaredMethod(Class clz, String name) {
        try {
            return clz.getDeclaredMethod(name);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    protected Method getDeclaredMethod(Class clz, String name, Class param) {
        try {
            return clz.getDeclaredMethod(name, param);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }


//    public void testWriteJDBCReadNDB() {
//        writeJDBCreadNDB();
//        failOnError();
//    }
//
//    public void testWriteNDBReadJDBC() {
//        writeNDBreadJDBC();
//        failOnError();
//   }
//
//    public void testWriteJDBCReadJDBC() {
//        writeJDBCreadJDBC();
//        failOnError();
//    }
//
//    public void testWriteNDBReadNDB() {
//        writeNDBreadNDB();
//        failOnError();
//   }
//
//   static ColumnDescriptor not_null_hash = new ColumnDescriptor
//            ("timestamp_not_null_hash", new InstanceHandler() {
//        public void setFieldValue(IdBase instance, Object value) {
//            ((TimestampAsSqlTimestampTypes)instance).setTimestamp_not_null_hash((Timestamp)value);
//        }
//        public Object getFieldValue(IdBase instance) {
//            return ((TimestampAsSqlTimestampTypes)instance).getTimestamp_not_null_hash();
//        }
//        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
//                throws SQLException {
//            preparedStatement.setTimestamp(j, (Timestamp)value);
//        }
//        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
//            return rs.getTimestamp(j);
//        }
//    });
//
//    static ColumnDescriptor not_null_btree = new ColumnDescriptor
//            ("timestamp_not_null_btree", new InstanceHandler() {
//        public void setFieldValue(IdBase instance, Object value) {
//            ((TimestampAsSqlTimestampTypes)instance).setTimestamp_not_null_btree((Timestamp)value);
//        }
//        public Object getFieldValue(IdBase instance) {
//            return ((TimestampAsSqlTimestampTypes)instance).getTimestamp_not_null_btree();
//        }
//        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
//                throws SQLException {
//            preparedStatement.setTimestamp(j, (Timestamp)value);
//        }
//        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
//            return rs.getTimestamp(j);
//        }
//    });
//    static ColumnDescriptor not_null_both = new ColumnDescriptor
//            ("timestamp_not_null_both", new InstanceHandler() {
//        public void setFieldValue(IdBase instance, Object value) {
//            ((TimestampAsSqlTimestampTypes)instance).setTimestamp_not_null_both((Timestamp)value);
//        }
//        public Timestamp getFieldValue(IdBase instance) {
//            return ((TimestampAsSqlTimestampTypes)instance).getTimestamp_not_null_both();
//        }
//        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
//                throws SQLException {
//            preparedStatement.setTimestamp(j, (Timestamp)value);
//        }
//        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
//            return rs.getTimestamp(j);
//        }
//    });
//    static ColumnDescriptor not_null_none = new ColumnDescriptor
//            ("timestamp_not_null_none", new InstanceHandler() {
//        public void setFieldValue(IdBase instance, Object value) {
//            ((TimestampAsSqlTimestampTypes)instance).setTimestamp_not_null_none((Timestamp)value);
//        }
//        public Timestamp getFieldValue(IdBase instance) {
//            return ((TimestampAsSqlTimestampTypes)instance).getTimestamp_not_null_none();
//        }
//        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
//                throws SQLException {
//            preparedStatement.setTimestamp(j, (Timestamp)value);
//        }
//        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
//            return rs.getTimestamp(j);
//        }
//    });
//
//    protected static ColumnDescriptor[] columnDescriptors = new ColumnDescriptor[] {
//            not_null_hash,
//            not_null_btree,
//            not_null_both,
//            not_null_none
//        };
//
//    @Override
//    protected ColumnDescriptor[] getColumnDescriptors() {
//        return columnDescriptors;
//    }

}
