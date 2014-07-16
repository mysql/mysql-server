/*
   Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;

import com.mysql.clusterj.DynamicObject;
import com.mysql.clusterj.annotation.PersistenceCapable;

import testsuite.clusterj.model.IdBase;

/** Test ShortShortkey
 * Schema
 *
drop table if exists shortpk;
create table shortpk (
 id smallint not null primary key,
 short_null_none smallint,
 short_null_btree smallint,
 short_null_hash smallint,
 short_null_both smallint,
 key idx_short_null_btree (short_null_btree),
 unique key idx_short_null_both (short_null_both),
 unique key idx_short_null_hash (short_null_hash) using hash
 ) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

 */
public class DynamicShortPKTest extends AbstractQueryTest {

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
        return Short.valueOf((short)i);
    }

    @Override
    protected String getTableName() {
        return "shortpk";
    }

    /** Subclasses override this method to provide the model class for the test */
    @Override
    Class<? extends IdBase> getModelClass() {
        return DynamicShortPK.class;
    }

    @Override
    protected Class<?> getInstanceType() {
        return DynamicShortPK.class;
    }

    @Override
    protected void createInstances(int number) {
        for (int i = 0; i < number; ++i) {
            DynamicShortPK instance = session.newInstance(DynamicShortPK.class);
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
        return (short)i;
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

    static ColumnDescriptor short_null_hash = new ColumnDescriptor
            ("short_null_hash", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((DynamicShortPK)instance).setShort_null_hash((Short)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((DynamicShortPK)instance).getShort_null_hash();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setShort(j, (Short)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getShort(j);
        }
    });

    static ColumnDescriptor short_null_btree = new ColumnDescriptor
            ("short_null_btree", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((DynamicShortPK)instance).setShort_null_btree((Short)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((DynamicShortPK)instance).getShort_null_btree();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setShort(j, (Short)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getShort(j);
        }
    });
    static ColumnDescriptor short_null_both = new ColumnDescriptor
            ("short_null_both", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((DynamicShortPK)instance).setShort_null_both((Short)value);
        }
        public Short getFieldValue(IdBase instance) {
            return ((DynamicShortPK)instance).getShort_null_both();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setShort(j, (Short)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getShort(j);
        }
    });
    static ColumnDescriptor short_null_none = new ColumnDescriptor
            ("short_null_none", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((DynamicShortPK)instance).setShort_null_none((Short)value);
        }
        public Short getFieldValue(IdBase instance) {
            return ((DynamicShortPK)instance).getShort_null_none();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setShort(j, (Short)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getShort(j);
        }
    });

    protected static ColumnDescriptor[] columnDescriptors = new ColumnDescriptor[] {
        short_null_none,
        short_null_btree,
        short_null_hash,
        short_null_both
        };

    @Override
    protected ColumnDescriptor[] getColumnDescriptors() {
        return columnDescriptors;
    }

    @PersistenceCapable(table="shortpk")
    public static class DynamicShortPK extends DynamicObject implements IdBase {

        public DynamicShortPK() {}

        public int getId() {
            return ((Number)get(0)).intValue();
        }

        public void setId(int id) {
            set(0, (short)id);
        }

        public Short getShort_null_none() {
            return (Short)get(1);
        }

        public void setShort_null_none(Short value) {
            set (1, value);
        }

        public Short getShort_null_btree() {
            return (Short)get(2);
        }

        public void setShort_null_btree(Short value) {
            set (2, value);
        }

        public Short getShort_null_hash() {
            return (Short)get(3);
        }

        public void setShort_null_hash(Short value) {
            set (3, value);
        }

        public Short getShort_null_both() {
            return (Short)get(4);
        }

        public void setShort_null_both(Short value) {
            set (4, value);
        }

    }
}
