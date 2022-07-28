/*
   Copyright (c) 2013, 2022, Oracle and/or its affiliates.

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

import com.mysql.clusterj.DynamicObject;
import com.mysql.clusterj.annotation.PersistenceCapable;

import testsuite.clusterj.model.IdBase;

/** Test Byte key
 * Schema
 *
drop table if exists bytepk;
create table bytepk (
 id tinyint not null primary key,
 byte_null_none tinyint,
 byte_null_btree tinyint,
 byte_null_hash tinyint,
 byte_null_both tinyint,
 key idx_byte_null_btree (byte_null_btree),
 unique key idx_byte_null_both (byte_null_both),
 unique key idx_byte_null_hash (byte_null_hash) using hash
 ) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

 */
public class DynamicBytePKTest extends AbstractQueryTest {

    static int NUMBER_OF_INSTANCES = 10;

    @Override
    protected boolean getDebug() {
        return false;
    }

    @Override
    protected int getNumberOfInstances() {
        return NUMBER_OF_INSTANCES;
    }

    /** Subclasses may override this method to convert an int into a key value */
    @Override
    protected Object convertToKey(int i) {
        return Byte.valueOf((byte)i);
    }

    @Override
    protected String getTableName() {
        return "bytepk";
    }

    /** Subclasses override this method to provide the model class for the test */
    @Override
    Class<? extends IdBase> getModelClass() {
        return DynamicBytePK.class;
    }

    @Override
    protected Class<?> getInstanceType() {
        return DynamicBytePK.class;
    }

    @Override
    protected void createInstances(int number) {
        for (int i = 0; i < number; ++i) {
            DynamicBytePK instance = session.newInstance(DynamicBytePK.class);
            instance.set(0, i);
            instance.set(1, i);
            instance.set(2, i);
            instance.set(3, i);
            instance.set(4, i);
            instances.add(instance);
        }
    }

    /** Subclasses override this method to provide values for rows (i) and columns (j) */
    @Override
    protected Object getColumnValue(int i, int j) {
        return (byte)i;
    }

    public void test() {
        testQuery();
        testWriteJDBCReadNDB();
        testWriteNDBReadJDBC();
        testWriteJDBCReadJDBC();
        testWriteNDBReadNDB();
        failOnError();
    }
    /** This has to be the first test because it assumes the test instances have been loaded */
    private void testQuery() {
        equalQuery("id", "PRIMARY", 8, 8);
        greaterEqualQuery("id", "PRIMARY", 7, 7, 8, 9);
        greaterThanQuery("id", "PRIMARY", 6, 7, 8, 9);
        lessEqualQuery("id", "PRIMARY", 4, 4, 3, 2, 1, 0);
        lessThanQuery("id", "PRIMARY", 4, 3, 2, 1, 0);
        betweenQuery("id", "PRIMARY", 4, 6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("id", "PRIMARY", 4, 6, 4, 5, 6);
        greaterThanAndLessEqualQuery("id", "PRIMARY", 4, 6, 5, 6);
        greaterEqualAndLessThanQuery("id", "PRIMARY", 4, 6, 4, 5);
        greaterThanAndLessThanQuery("id", "PRIMARY", 4, 6, 5);
    }

    private void testWriteJDBCReadNDB() {
        writeJDBCreadNDB();
    }

    private void testWriteNDBReadJDBC() {
        writeNDBreadJDBC();
   }

    private void testWriteJDBCReadJDBC() {
        writeJDBCreadJDBC();
    }

    private void testWriteNDBReadNDB() {
        writeNDBreadNDB();
   }

    static ColumnDescriptor byte_null_hash = new ColumnDescriptor
            ("byte_null_hash", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((DynamicBytePK)instance).setByte_null_hash((Byte)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((DynamicBytePK)instance).getByte_null_hash();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setByte(j, (Byte)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getByte(j);
        }
    });

    static ColumnDescriptor byte_null_btree = new ColumnDescriptor
            ("byte_null_btree", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((DynamicBytePK)instance).setByte_null_btree((Byte)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((DynamicBytePK)instance).getByte_null_btree();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setByte(j, (Byte)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getByte(j);
        }
    });
    static ColumnDescriptor byte_null_both = new ColumnDescriptor
            ("byte_null_both", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((DynamicBytePK)instance).setByte_null_both((Byte)value);
        }
        public Byte getFieldValue(IdBase instance) {
            return ((DynamicBytePK)instance).getByte_null_both();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setByte(j, (Byte)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getByte(j);
        }
    });
    static ColumnDescriptor byte_null_none = new ColumnDescriptor
            ("byte_null_none", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((DynamicBytePK)instance).setByte_null_none((Byte)value);
        }
        public Byte getFieldValue(IdBase instance) {
            return ((DynamicBytePK)instance).getByte_null_none();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setByte(j, (Byte)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getByte(j);
        }
    });

    protected static ColumnDescriptor[] columnDescriptors = new ColumnDescriptor[] {
        byte_null_none,
        byte_null_btree,
        byte_null_hash,
        byte_null_both
        };

    @Override
    protected ColumnDescriptor[] getColumnDescriptors() {
        return columnDescriptors;
    }

    @PersistenceCapable(table="bytepk")
    public static class DynamicBytePK extends DynamicObject implements IdBase {

        public DynamicBytePK() {}

        public int getId() {
            return ((Number)get(0)).intValue();
        }

        public void setId(int id) {
            set(0, (byte)id);
        }

        public Byte getByte_null_none() {
            return (Byte)get(1);
        }

        public void setByte_null_none(Byte value) {
            set (1, value);
        }

        public Byte getByte_null_btree() {
            return (Byte)get(2);
        }

        public void setByte_null_btree(Byte value) {
            set (2, value);
        }

        public Byte getByte_null_hash() {
            return (Byte)get(3);
        }

        public void setByte_null_hash(Byte value) {
            set (3, value);
        }

        public Byte getByte_null_both() {
            return (Byte)get(4);
        }

        public void setByte_null_both(Byte value) {
            set (4, value);
        }

    }
}
