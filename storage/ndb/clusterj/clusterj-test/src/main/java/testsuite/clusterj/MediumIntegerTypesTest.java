/*
   Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

import testsuite.clusterj.model.MediumIntegerTypes;
import testsuite.clusterj.model.IdBase;

/**
 * 1. Test that MEDIUMINT type can be read and written.
 *   a) Write and read using NDB
 *   b) Write and read using JDBC
 *   c) Write using NDB and read using JDBC
 *   d) Write using JDBC and read using NDB
 * 2. Test the boundaries of MEDIUMINT type.
 */
public class MediumIntegerTypesTest extends AbstractClusterJModelTest {

    static final int UPPER_BOUND = (int)Math.pow(2, 23) - 1;
    static final int LOWER_BOUND = -((int)Math.pow(2, 23));

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        tx = session.currentTransaction();

        tx.begin();
        session.deletePersistentAll(MediumIntegerTypes.class);
        try {
            tx.commit();
        } catch (Exception ex) {
            // ignore exceptions -- might not be any instances to delete
        }
        addTearDownClasses(MediumIntegerTypes.class);
    }

    public void testWriteNDBReadNDB() {
        writeNDBreadNDB();
        failOnError();
    }

    public void testWriteJDBCReadJDBC() {
        writeJDBCreadJDBC();
        failOnError();
    }

    public void testWriteJDBCReadNDB() {
        writeJDBCreadNDB();
        failOnError();
    }

    public void testWriteNDBReadJDBC() {
        writeNDBreadJDBC();
        failOnError();
    }

    public void testBoundaries() {
        writeRead(1, UPPER_BOUND);
        writeRead(2, LOWER_BOUND);

        writeFail(3, UPPER_BOUND + 1, ".*Out of range value.*");
        writeFail(4, LOWER_BOUND - 1, ".*Out of range value.*");
        failOnError();
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
        return "mediumintegertypes";
    }

    /** Subclasses override this method to provide the model class for the test */
    @Override
    Class<? extends IdBase> getModelClass() {
        return MediumIntegerTypes.class;
    }

    /** Subclasses override this method to provide values for rows (i) and columns (j) */
    @Override
    protected Object getColumnValue(int i, int j) {
        return 100000  * i + j;
    }

    /**
     * A test to write the given int to all columns in columnDescriptors,
     * and read back via find to verify it with the expected input.
     */
    protected void writeRead(int id, int output){
        MediumIntegerTypes instance = session.newInstance(MediumIntegerTypes.class);
        instance.setId(id);
        for (ColumnDescriptor columnDescriptor: columnDescriptors) {
            columnDescriptor.setFieldValue(instance, output);
        }
        session.currentTransaction().begin();
        session.persist(instance);
        session.currentTransaction().commit();

        session.currentTransaction().begin();
        instance = session.find(MediumIntegerTypes.class,id);
        session.currentTransaction().commit();

        for (ColumnDescriptor columnDescriptor: columnDescriptors) {
            errorIfNotEqual("Failure on reading data from " + columnDescriptor.getColumnName(), output,
                            columnDescriptor.getFieldValue(instance));
        }
    }

    /**
     * Run a negative test that tries to write the output int to all the columns
     * and expects the resulting exception to contain the exception pattern
     */
    protected void writeFail(int id, int output, String exceptionPattern){
        MediumIntegerTypes instance = session.newInstance(MediumIntegerTypes.class);
        Exception caughtException = null;
        for (ColumnDescriptor columnDescriptor: columnDescriptors) {
            try{
                columnDescriptor.setFieldValue(instance, output);
            }catch(Exception ex){
                caughtException = ex;
            }
            verifyException("Writing invalid values to the column",
                    caughtException, exceptionPattern);
        }
    }

    static ColumnDescriptor medium_null_hash = new ColumnDescriptor
            ("medium_null_hash", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((MediumIntegerTypes)instance).setMedium_null_hash((Integer)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((MediumIntegerTypes)instance).getMedium_null_hash();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setInt(j, (Integer)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getInt(j);
        }
    });

    static ColumnDescriptor medium_null_btree = new ColumnDescriptor
            ("medium_null_btree", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((MediumIntegerTypes)instance).setMedium_null_btree((Integer)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((MediumIntegerTypes)instance).getMedium_null_btree();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setInt(j, (Integer)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getInt(j);
        }
    });

    static ColumnDescriptor medium_null_both = new ColumnDescriptor
            ("medium_null_both", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((MediumIntegerTypes)instance).setMedium_null_both((Integer)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((MediumIntegerTypes)instance).getMedium_null_both();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setInt(j, (Integer)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getInt(j);
        }
    });

    static ColumnDescriptor medium_null_none = new ColumnDescriptor
            ("medium_null_none", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((MediumIntegerTypes)instance).setMedium_null_none((Integer)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((MediumIntegerTypes)instance).getMedium_null_none();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setInt(j, (Integer)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getInt(j);
        }
    });

    static ColumnDescriptor medium_not_null_hash = new ColumnDescriptor
            ("medium_not_null_hash", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((MediumIntegerTypes)instance).setMedium_not_null_hash((Integer)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((MediumIntegerTypes)instance).getMedium_not_null_hash();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setInt(j, (Integer)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getInt(j);
        }
    });

    static ColumnDescriptor medium_not_null_btree = new ColumnDescriptor
            ("medium_not_null_btree", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((MediumIntegerTypes)instance).setMedium_not_null_btree((Integer)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((MediumIntegerTypes)instance).getMedium_not_null_btree();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setInt(j, (Integer)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getInt(j);
        }
    });

    static ColumnDescriptor medium_not_null_both = new ColumnDescriptor
            ("medium_not_null_both", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((MediumIntegerTypes)instance).setMedium_not_null_both((Integer)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((MediumIntegerTypes)instance).getMedium_not_null_both();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setInt(j, (Integer)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getInt(j);
        }
    });

    static ColumnDescriptor medium_not_null_none = new ColumnDescriptor
            ("medium_not_null_none", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((MediumIntegerTypes)instance).setMedium_not_null_none((Integer)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((MediumIntegerTypes)instance).getMedium_not_null_none();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setInt(j, (Integer)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getInt(j);
        }
    });

    protected static ColumnDescriptor[] columnDescriptors = new ColumnDescriptor[] {
        medium_null_hash,
        medium_null_btree,
        medium_null_both,
        medium_null_none,
        medium_not_null_hash,
        medium_not_null_btree,
        medium_not_null_both,
        medium_not_null_none
        };

    @Override
    protected ColumnDescriptor[] getColumnDescriptors() {
        return columnDescriptors;
    }
}

