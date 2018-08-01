/*
  Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

package testsuite.clusterj;

import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.List;
import java.math.BigInteger;

import testsuite.clusterj.model.BitTypes;
import testsuite.clusterj.model.IdBase;

/** Test that BIT types can be read and written. 
 * case 1: Write using JDBC, read using NDB.
 * case 2: Write using NDB, read using JDBC.
 * Schema
 *
drop table if exists bittypes;
create table bittypes (
 id int not null primary key,

 bit1 bit(1),
 bit2 bit(2),
 bit4 bit(4),
 bit8 bit(8),
 bit16 bit(16),
 bit32 bit(32),
 bit64 bit(64)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

 */
public class BitTypesTest extends AbstractClusterJModelTest {

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
        return "bittypes";
    }

    /** Subclasses override this method to provide the model class for the test */
    @Override
    Class<? extends IdBase> getModelClass() {
        return BitTypes.class;
    }

    /** Subclasses override this method to provide values for rows (i) and columns (j) */
    @Override
    protected Object getColumnValue(int i, int j) {
        // Note that i and j are both 0-index here to correspond to Java semantics
        // first calculate the length of the data
        int length = (int)Math.pow(2, j);
        switch (length) {
            case 1: { // boolean
                boolean data = (i % 2) == 0;
                if (getDebug()) System.out.println("BitTypesTest.getColumnValue Column data for " + i + ", " + j
                        + " " + columnDescriptors[j].getColumnName()
                        + "  is (boolean)" + data);
                return data;
            }
            case 2: { // byte
                int data = 0;
                // fill in the data, increasing by one for each row, column, and bit in the data
                for (int d = 0; d < length; ++d) {
                    data = (data * 2) + (int)(Math.random() * 2);
                }
                if (getDebug()) System.out.println("BitTypesTest.getColumnValue Column data for " + i + ", " + j
                        + " " + columnDescriptors[j].getColumnName()
                        + "  is (byte)" + data);
                return Byte.valueOf((byte)data);
            }
            case 4: { // short
                int data = 0;
                // fill in the data, increasing by one for each row, column, and bit in the data
                for (int d = 0; d < length; ++d) {
                    data = (data * 2) + (int)(Math.random() * 2);
                }
                if (getDebug()) System.out.println("BitTypesTest.getColumnValue Column data for " + i + ", " + j
                        + " " + columnDescriptors[j].getColumnName()
                        + "  is (short)" + data);
                return Short.valueOf((short)data);
            }
            case 8: 
            case 32: { // int
                int data = 0;
                // fill in the data, increasing by one for each row, column, and bit in the data
                for (int d = 0; d < length; ++d) {
                    data = (data * 2) + ((int)(Math.random() * 2));
                }
                if (getDebug()) System.out.println("BitTypesTest.getColumnValue Column data for " + i + ", " + j
                        + " " + columnDescriptors[j].getColumnName()
                        + "  is (int)" + data);
                // TODO bug in JDBC handling high bit
                data = Math.abs(data);
                return Integer.valueOf(data);
            }
            case 16:
            case 64: { // long
                long data = 0;
                // fill in the data, increasing by one for each row, column, and bit in the data
                for (int d = 0; d < length / 8; ++d) {
                    data = (data * 256) + (i * 16) + d;
                }
                if (getDebug()) System.out.println("BitTypesTest.getColumnValue Column data for " + i + ", " + j
                        + " " + columnDescriptors[j].getColumnName()
                        + "  is (long)" + data);
                return Long.valueOf(data);
            }
            default:
                fail("Bad length: " + length);
                return null;
        }
    }

    /** Verify that the actual results match the expected results. If not, use the multiple error
     * reporting method errorIfNotEqual defined in the superclass.
     * @param where the location of the verification of results, normally the name of the test method
     * @param expecteds the expected results
     * @param actuals the actual results
     */
    @Override
    protected void verify(String where, List<Object[]> expecteds, List<Object[]> actuals) {
        // note that here, i is 0-index but j is 1-index to correspond to JDBC semantics
        for (int i = 0; i < expecteds.size(); ++i) {
            Object[] expected = expecteds.get(i);
            Object[] actual = actuals.get(i);
            errorIfNotEqual(where + " got failure on id for row " + i, i, actual[0]);
            for (int j = 1; j < expected.length; ++j) {
                if (getDebug()) System.out.println("BitTypesTest.verify for " + i + ", " + j
                        + " " + columnDescriptors[j - 1].getColumnName()
                        + "  is (" + actual[j].getClass().getName() + ")" + actual[j]);
                switch (j) {
                    case 1: { // boolean
                        Boolean expectedColumn = (Boolean)expected[j];
                        Boolean actualColumn = (Boolean)actual[j];
                        errorIfNotEqual(where + " got failure on comparison of data for row "
                                + i + " column " + j + " " + columnDescriptors[j - 1].getColumnName(),
                                expectedColumn, actualColumn);
                        break;
                    }
                    case 2: { // byte
                        byte expectedColumn = (Byte)expected[j];
                        byte actualColumn = (Byte)actual[j];
                        // now compare bit by bit
                        errorIfNotEqual(where + " got failure on comparison of data for row "
                                + i + " column " + j + " " + columnDescriptors[j - 1].getColumnName(),
                                Integer.toHexString(expectedColumn), Integer.toHexString(actualColumn));
                        break;
                    }
                    case 3: { // short
                        short expectedColumn = (Short)expected[j];
                        short actualColumn = (Short)actual[j];
                        // now compare bit by bit
                        errorIfNotEqual(where + " got failure on comparison of data for row "
                                + i + " column " + j + " " + columnDescriptors[j - 1].getColumnName(),
                                Integer.toHexString(expectedColumn), Integer.toHexString(actualColumn));
                        break;
                    }
                    case 4:
                    case 6: { // int
                        int expectedColumn = (Integer)expected[j];
                        int actualColumn = (Integer)actual[j];
                        // now compare bit by bit
                        errorIfNotEqual(where + " got failure on comparison of data for row "
                                + i + " column " + j + " " + columnDescriptors[j - 1].getColumnName(),
                                Integer.toHexString(expectedColumn), Integer.toHexString(actualColumn));
                        break;
                    }
                    case 5:
                    case 7: { // long
                        long expectedColumn = (Long)expected[j];
                        long actualColumn = (Long)actual[j];
                        // now compare bit by bit
                        errorIfNotEqual(where + " got failure on comparison of data for row "
                                + i + " column " + j + " " + columnDescriptors[j - 1].getColumnName(),
                                Long.toHexString(expectedColumn), Long.toHexString(actualColumn));
                        break;
                   }
                    default:
                        fail("Bad value for j: " + j);
                }
            }
        }
    }

    public void testWriteJDBCReadNDB() {
        writeJDBCreadNDB();
        failOnError();
    }

    public void testWriteNDBReadJDBC() {
        writeNDBreadJDBC();
        failOnError();
   }

    public void testWriteNDBReadNDB() {
        writeNDBreadNDB();
        failOnError();
   }

    public void testWriteJDBCReadJDBC() {
        writeJDBCreadJDBC();
        failOnError();
   }

   static ColumnDescriptor bit1 = new ColumnDescriptor
            ("bit1", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((BitTypes)instance).setBit1((Boolean)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((BitTypes)instance).getBit1();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setBoolean(j, (Boolean)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            String value = rs.getString(j);
            if (value.length() == 0) {
                value = "0";
            }
            return (Byte.parseByte(value) == 0x01)?Boolean.TRUE:Boolean.FALSE;
        }
    });

   static ColumnDescriptor bit2 = new ColumnDescriptor
            ("bit2", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((BitTypes)instance).setBit2((Byte)value);
        }
        public Object getFieldValue(IdBase instance) {
            return (Byte)((BitTypes)instance).getBit2();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setByte(j, (Byte)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            String value = rs.getString(j);
            if (value.length() == 0) {
                value = "0";
            }
            return Byte.parseByte(value);
        }
    });

   static ColumnDescriptor bit4 = new ColumnDescriptor
            ("bit4", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((BitTypes)instance).setBit4((Short)value);
        }
        public Object getFieldValue(IdBase instance) {
            return (Short)((BitTypes)instance).getBit4();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setShort(j, (Short)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            String value = rs.getString(j);
            if (value.length() == 0) {
                value = "0";
            }
            return Short.parseShort(value);
        }
    });

   static ColumnDescriptor bit8 = new ColumnDescriptor
            ("bit8", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((BitTypes)instance).setBit8((Integer)value);
        }
        public Object getFieldValue(IdBase instance) {
            return (Integer)((BitTypes)instance).getBit8();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setInt(j, (Integer)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            String value = rs.getString(j);
            if (value.length() == 0) {
                value = "0";
            }
            return Integer.parseInt(value) & 0xff;
        }
    });

   static ColumnDescriptor bit16 = new ColumnDescriptor
            ("bit16", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((BitTypes)instance).setBit16((Long)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((BitTypes)instance).getBit16();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setLong(j, (Long)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            String value = rs.getString(j);
            if (value.length() == 0) {
                value = "0";
            }
            return Long.parseLong(value);
        }
    });

   static ColumnDescriptor bit32 = new ColumnDescriptor
            ("bit32", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((BitTypes)instance).setBit32((Integer)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((BitTypes)instance).getBit32();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setInt(j, (Integer)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            String value = rs.getString(j);
            if (value.length() == 0) {
                value = "0";
            }
            /* parsing 32 bit array into int will throw exception in Java 8.
               so load it into long and then convert it to int. */
            return (int)Long.parseLong(value);
        }
    });

   static ColumnDescriptor bit64 = new ColumnDescriptor
            ("bit64", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((BitTypes)instance).setBit64((Long)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((BitTypes)instance).getBit64();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setLong(j, (Long)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            String value = rs.getString(j);
            if (value.length() == 0) {
                value = "0";
            }
            /* parsing 64 bit array into long will throw exception in Java 8.
               So load it into BigInt and then convert it to long. */
            return new BigInteger(value).longValue();
        }
    });

    protected static ColumnDescriptor[] columnDescriptors = new ColumnDescriptor[] {
            bit1,
            bit2,
            bit4,
            bit8,
            bit16,
            bit32,
            bit64
        };

    @Override
    protected ColumnDescriptor[] getColumnDescriptors() {
        return columnDescriptors;
    }

}
