/*
   Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

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
import java.sql.Time;

import testsuite.clusterj.model.IdBase;
import testsuite.clusterj.model.TimeAsSqlTimeTypes;

/** Test that Times can be read and written. 
 * case 1: Write using JDBC, read using NDB.
 * case 2: Write using NDB, read using JDBC.
 * Schema
 *
drop table if exists timetypes;
create table timetypes (
 id int not null primary key,

 time_null_hash time,
 time_null_btree time,
 time_null_both time,
 time_null_none time,

 time_not_null_hash time,
 time_not_null_btree time,
 time_not_null_both time,
 time_not_null_none time

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_time_null_hash using hash on timetypes(time_null_hash);
create index idx_time_null_btree on timetypes(time_null_btree);
create unique index idx_time_null_both on timetypes(time_null_both);

create unique index idx_time_not_null_hash using hash on timetypes(time_not_null_hash);
create index idx_time_not_null_btree on timetypes(time_not_null_btree);
create unique index idx_time_not_null_both on timetypes(time_not_null_both);
 */
public class TimeAsSqlTimeTypesTest extends AbstractClusterJModelTest {

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
        return "timetypes";
    }

    /** Subclasses override this method to provide the model class for the test */
    @Override
    Class<? extends IdBase> getModelClass() {
        return TimeAsSqlTimeTypes.class;
    }

    /** Subclasses override this method to provide values for rows (i) and columns (j) */
    @Override
    protected Object getColumnValue(int i, int j) {
        return new Time(getMillisFor(0, i, i, j));
    }

    public void testWriteJDBCReadNDB() {
        writeJDBCreadNDB();
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

    public void testWriteNDBReadJDBC() {
        writeNDBreadJDBC();
        failOnError();
    }

    static ColumnDescriptor not_null_hash = new ColumnDescriptor
            ("time_not_null_hash", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((TimeAsSqlTimeTypes)instance).setTime_not_null_hash((Time)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((TimeAsSqlTimeTypes)instance).getTime_not_null_hash();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setTime(j, (Time)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getTime(j);
        }
    });

    static ColumnDescriptor not_null_btree = new ColumnDescriptor
            ("time_not_null_btree", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((TimeAsSqlTimeTypes)instance).setTime_not_null_btree((Time)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((TimeAsSqlTimeTypes)instance).getTime_not_null_btree();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setTime(j, (Time)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getTime(j);
        }
    });

    static ColumnDescriptor not_null_both = new ColumnDescriptor
            ("time_not_null_both", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((TimeAsSqlTimeTypes)instance).setTime_not_null_both((Time)value);
        }
        public Time getFieldValue(IdBase instance) {
            return ((TimeAsSqlTimeTypes)instance).getTime_not_null_both();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setTime(j, (Time)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getTime(j);
        }
    });

    static ColumnDescriptor not_null_none = new ColumnDescriptor
            ("time_not_null_none", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((TimeAsSqlTimeTypes)instance).setTime_not_null_none((Time)value);
        }
        public Time getFieldValue(IdBase instance) {
            return ((TimeAsSqlTimeTypes)instance).getTime_not_null_none();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setTime(j, (Time)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getTime(j);
        }
    });

    protected static ColumnDescriptor[] columnDescriptors = new ColumnDescriptor[] {
            not_null_hash,
            not_null_btree,
            not_null_both,
            not_null_none
        };

    @Override
    protected ColumnDescriptor[] getColumnDescriptors() {
        return columnDescriptors;
    }

}
