/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.
   Use is subject to license terms.

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

import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.List;

import testsuite.clusterj.model.IdBase;
import testsuite.clusterj.model.BinaryTypes;

/** Test that Timestamps can be read and written. 
 * case 1: Write using JDBC, read using NDB.
 * case 2: Write using NDB, read using JDBC.
 * Schema
 *
drop table if exists binarytypes;
create table binarytypes (
 id int not null primary key,

 binary1 binary(1),
 binary2 binary(2),
 binary4 binary(4),
 binary8 binary(8),
 binary16 binary(16),
 binary32 binary(32),
 binary64 binary(64),
 binary128 binary(128)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

 */
public class BinaryTypesTest extends AbstractClusterJModelTest {

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
        return "binarytypes";
    }

    /** Subclasses override this method to provide the model class for the test */
    @Override
    Class<? extends IdBase> getModelClass() {
        return BinaryTypes.class;
    }

    /** Subclasses override this method to provide values for rows (i) and columns (j) */
    @Override
    protected Object getColumnValue(int i, int j) {
        // first calculate the length of the data
        int length = (int)Math.pow(2, j);
        byte[] data = new byte[length];
        // fill in the data, increasing by one for each row, column, and byte in the byte[]
        for (int d = 0; d < length; ++d) {
            data[d] = (byte)(i + j + d);
        }
        return data;
    }

    /** Verify that the actual results match the expected results. If not, use the multiple error
     * reporting method errorIfNotEqual defined in the superclass.
     * @param where the location of the verification of results, normally the name of the test method
     * @param expecteds the expected results
     * @param actuals the actual results
     */
    @Override
    protected void verify(String where, List<Object[]> expecteds, List<Object[]> actuals) {
        for (int i = 0; i < expecteds.size(); ++i) {
            Object[] expected = expecteds.get(i);
            Object[] actual = actuals.get(i);
            errorIfNotEqual(where + " got failure on id for row " + i, i, actual[0]);
            for (int j = 1; j < expected.length; ++j) {
                // verify each object in the array
                byte[] expectedColumn = (byte[])(expected)[j];
                byte[] actualColumn = (byte[])(actual)[j];
                errorIfNotEqual(where + " got failure on length of data for row " + i,
                        expectedColumn.length, actualColumn.length);
                if (expectedColumn.length == actualColumn.length) {
                    // now compare byte by byte
                    for (j = 0; j < expectedColumn.length; ++j)
                        errorIfNotEqual(where + " got failure on comparison of data for row "
                                + i + " column " + j,
                                expectedColumn[j], actualColumn[j]);
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

   static ColumnDescriptor binary1 = new ColumnDescriptor
            ("binary1", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((BinaryTypes)instance).setBinary1((byte[])value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((BinaryTypes)instance).getBinary1();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setBytes(j, (byte[])value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getBytes(j);
        }
    });

   static ColumnDescriptor binary2 = new ColumnDescriptor
            ("binary2", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((BinaryTypes)instance).setBinary2((byte[])value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((BinaryTypes)instance).getBinary2();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setBytes(j, (byte[])value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getBytes(j);
        }
    });

   static ColumnDescriptor binary4 = new ColumnDescriptor
            ("binary4", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((BinaryTypes)instance).setBinary4((byte[])value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((BinaryTypes)instance).getBinary4();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setBytes(j, (byte[])value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getBytes(j);
        }
    });

   static ColumnDescriptor binary8 = new ColumnDescriptor
            ("binary8", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((BinaryTypes)instance).setBinary8((byte[])value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((BinaryTypes)instance).getBinary8();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setBytes(j, (byte[])value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getBytes(j);
        }
    });

   static ColumnDescriptor binary16 = new ColumnDescriptor
            ("binary16", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((BinaryTypes)instance).setBinary16((byte[])value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((BinaryTypes)instance).getBinary16();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setBytes(j, (byte[])value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getBytes(j);
        }
    });

   static ColumnDescriptor binary32 = new ColumnDescriptor
            ("binary32", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((BinaryTypes)instance).setBinary32((byte[])value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((BinaryTypes)instance).getBinary32();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setBytes(j, (byte[])value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getBytes(j);
        }
    });

   static ColumnDescriptor binary64 = new ColumnDescriptor
            ("binary64", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((BinaryTypes)instance).setBinary64((byte[])value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((BinaryTypes)instance).getBinary64();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setBytes(j, (byte[])value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getBytes(j);
        }
    });

   static ColumnDescriptor binary128 = new ColumnDescriptor
            ("binary128", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((BinaryTypes)instance).setBinary128((byte[])value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((BinaryTypes)instance).getBinary128();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setBytes(j, (byte[])value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getBytes(j);
        }
    });

    protected static ColumnDescriptor[] columnDescriptors = new ColumnDescriptor[] {
            binary1,
            binary2,
            binary4,
            binary8,
            binary16,
            binary32,
            binary64,
            binary128
        };

    @Override
    protected ColumnDescriptor[] getColumnDescriptors() {
        return columnDescriptors;
    }

}
