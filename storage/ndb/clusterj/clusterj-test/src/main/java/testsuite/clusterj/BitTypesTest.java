/*
  Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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

import com.mysql.clusterj.Query;
import com.mysql.clusterj.query.QueryBuilder;
import com.mysql.clusterj.query.QueryDomainType;
import com.mysql.clusterj.query.Predicate;

import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

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
    @Override
    protected boolean getCleanupAfterTest() {
        return false;
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
                    data = 3 & ((data * 2) + (int)(Math.random() * 2));
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
                    data = 0xF & ((data * 2) + (int)(Math.random() * 2));
                }
                if (getDebug()) System.out.println("BitTypesTest.getColumnValue Column data for " + i + ", " + j
                        + " " + columnDescriptors[j].getColumnName()
                        + "  is (short)" + data);
                return Short.valueOf((short)data);
            }
            case 8: 
            case 32: { // int
                int mask = (length == 8) ? 0xFF: 0xFFFFFFFF;
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
                return Integer.valueOf(mask & data);
            }
            case 16:
            case 64: { // long
                long mask = (length == 16) ? 0xFFFFL : 0xFFFFFFFFFFFFFFFFL;
                long data = 0;
                // fill in the data, increasing by one for each row, column, and bit in the data
                for (int d = 0; d < length / 8; ++d) {
                    data = (data * 256) + (i * 16) + d;
                }
                if (getDebug()) System.out.println("BitTypesTest.getColumnValue Column data for " + i + ", " + j
                        + " " + columnDescriptors[j].getColumnName()
                        + "  is (long)" + data);
                return Long.valueOf(mask & data);
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

    protected void complexQuery(Boolean bit1value, Byte bit2value, Short bit4value,
            Integer bit8value, Long bit16value, Integer bit32value, Long bit64value, int expectedId) {
        QueryBuilder qb = session.getQueryBuilder();
        QueryDomainType<BitTypes> qdt = qb.createQueryDefinition(BitTypes.class);
        Predicate p = qdt.get("id").greaterEqual(qdt.param("pid"));
        Map<String, Object> params = new HashMap<String, Object>(8);
        if (bit1value != null) {
            p = p.and(qdt.get("bit1").equal(qdt.param("param1")));
            params.put("param1", bit1value);
        }
        if (bit2value != null) {
            p = p.and(qdt.get("bit2").equal(qdt.param("param2")));
            params.put("param2", bit2value);
        }
        if (bit4value != null) {
            p = p.and(qdt.get("bit4").equal(qdt.param("param4")));
            params.put("param4", bit4value);
        }
        if (bit8value != null) {
            p = p.and(qdt.get("bit8").equal(qdt.param("param8")));
            params.put("param8", bit8value);
        }
        if (bit16value != null) {
            p = p.and(qdt.get("bit16").equal(qdt.param("param16")));
            params.put("param16", bit16value);
        }
        if (bit32value != null) {
            p = p.and(qdt.get("bit32").equal(qdt.param("param32")));
            params.put("param32", bit32value);
        }
        if (bit64value != null) {
            p = p.and(qdt.get("bit64").equal(qdt.param("param64")));
            params.put("param64", bit64value);
        }
        qdt.where(p);
        Query<BitTypes> query = session.createQuery(qdt);
        query.setParameter("pid", 0);
        for (String key: params.keySet()) {
            query.setParameter(key, params.get(key));
        }
        List<BitTypes> results = query.getResultList();
        if (getDebug()) for (BitTypes result: results) {
            System.out.println("result id " + result.getId() +
            " bit1: " + result.getBit1() +
            " bit2: " + Integer.toHexString(result.getBit2()) +
            " bit4: " + Integer.toHexString(result.getBit4()) +
            " bit8: " + Integer.toHexString(result.getBit8()) +
            " bit16: " + Long.toHexString(result.getBit16()) +
            " bit32: " + Integer.toHexString(result.getBit32()) +
            " bit64: " + Long.toHexString(result.getBit64()));
        }
        if (results.size() != 1) {
            error("complexQuery mismatch result.size(): expected 1, actual "+ results.size());
            return;
        }
        BitTypes result = results.get(0);
        errorIfNotEqual("complexQuery mismatch result id", expectedId, result.getId());
    }

    public void testQuery() {
        generateInstances(getColumnDescriptors());
        removeAll(getModelClass());
        writeToNDB(columnDescriptors, instances);
        // get the "random" value for row 3 bit2
        Byte bit2For3 = (Byte)getExpected().get(3)[2];
        // get the "random" value for row 4 bit4
        Short bit4For4 = (Short)getExpected().get(4)[3];
        // get the "random" value for row 5 bit8
        Integer bit8For5 = (Integer)getExpected().get(5)[4];
        // get the "random" value for row 7 bit32 (columns are origin 1-index)
        Integer bit32For7 = (Integer)getExpected().get(7)[6];
        //           bit1   bit2      bit4       bit8       bit16    bit32      bit64 result
        //           bool   byte      short      int        long     int        long  int
        complexQuery(false, null,     null,      null,      0x1011L, null,      null, 1);
        complexQuery(false, bit2For3, null,      null,      0x3031L, null,      null, 3);
        complexQuery(true,  null,     bit4For4,  null,      0x4041L, null,      null, 4);
        complexQuery(false, null,     null,      bit8For5,  0x5051L, null,      null, 5);
        complexQuery(true,  null,     null,      null,      0x6061L, null,      null, 6);
        complexQuery(false, null,     null,      null,      null,    bit32For7, null, 7);
        complexQuery(false, null,     null,      null,      null,    null,      0x9091929394959697L, 9);
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
            return rs.getBoolean(j);
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
            return rs.getByte(j);
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
            return rs.getShort(j);
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
            return rs.getInt(j);
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
            return rs.getLong(j);
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
            return rs.getInt(j);
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
            return rs.getLong(j);
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
