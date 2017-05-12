/*
 *  Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj.jpatest;

import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Timestamp;
import java.util.Date;

import com.mysql.clusterj.jpatest.model.TimestampAsUtilDateTypes;
import com.mysql.clusterj.jpatest.model.IdBase;

/** Test that Timestamps can be read and written. 
 * case 1: Write using JDBC, read using NDB.
 * case 2: Write using NDB, read using JDBC.
 * Schema
 *
drop table if exists timestamptypes;
create table timestamptypes (
 id int not null primary key,

 timestamp_not_null_hash timestamp,
 timestamp_not_null_btree timestamp,
 timestamp_not_null_both timestamp,
 timestamp_not_null_none timestamp

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_timestamp_not_null_hash using hash on timestamptypes(timestamp_not_null_hash);
create index idx_timestamp_not_null_btree on timestamptypes(timestamp_not_null_btree);
create unique index idx_timestamp_not_null_both on timestamptypes(timestamp_not_null_both);

 */
@org.junit.Ignore
public class TimestampAsUtilDateTest extends AbstractJPABaseTest {

    @Override
    public void setUp() {
        super.setUp();
        getConnection();
        resetLocalSystemDefaultTimeZone(connection);
        connection = null;
        getConnection();
        setAutoCommit(connection, false);
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
        return "datetimetypes";
    }

    /** Subclasses override this method to provide the model class for the test */
    @Override
    protected Class<? extends IdBase> getModelClass() {
        return TimestampAsUtilDateTypes.class;
    }

    /** Subclasses override this method to provide values for rows (i) and columns (j) */
    @Override
    protected Object getColumnValue(int i, int j) {
        return new Date(getMillisFor(1980, 1, i + 1, 0, 0, j));
    }

    @Override
    /** Subclasses must override this method to implement the model factory for the test */
    protected IdBase getNewInstance(Class<? extends IdBase> modelClass) {
        return new TimestampAsUtilDateTypes();
    }

    public void testWriteJDBCReadJPA() {
         writeJDBCreadJPA();
         failOnError();
    }

    public void testWriteJPAReadJDBC() {
         writeJPAreadJDBC();
         failOnError();
   }

    public void testWriteJDBCReadJDBC() {
        writeJDBCreadJDBC();
        failOnError();
    }

    public void testWriteJPAReadJPA() {
        writeJPAreadJPA();
        failOnError();
   }

   static ColumnDescriptor not_null_hash = new ColumnDescriptor
            ("datetime_not_null_hash", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((TimestampAsUtilDateTypes)instance).setTimestamp_not_null_hash((Date)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((TimestampAsUtilDateTypes)instance).getTimestamp_not_null_hash();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            Timestamp timestamp = new Timestamp(((Date)value).getTime());
            preparedStatement.setTimestamp(j, timestamp);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getTimestamp(j);
        }
    });

    static ColumnDescriptor not_null_btree = new ColumnDescriptor
            ("datetime_not_null_btree", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((TimestampAsUtilDateTypes)instance).setTimestamp_not_null_btree((Date)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((TimestampAsUtilDateTypes)instance).getTimestamp_not_null_btree();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            Timestamp timestamp = new Timestamp(((Date)value).getTime());
            preparedStatement.setTimestamp(j, timestamp);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getTimestamp(j);
        }
    });

    static ColumnDescriptor not_null_both = new ColumnDescriptor
            ("datetime_not_null_both", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((TimestampAsUtilDateTypes)instance).setTimestamp_not_null_both((Date)value);
        }
        public Date getFieldValue(IdBase instance) {
            return ((TimestampAsUtilDateTypes)instance).getTimestamp_not_null_both();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            Timestamp timestamp = new Timestamp(((Date)value).getTime());
            preparedStatement.setTimestamp(j, timestamp);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getTimestamp(j);
        }
    });

    static ColumnDescriptor not_null_none = new ColumnDescriptor
            ("datetime_not_null_none", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((TimestampAsUtilDateTypes)instance).setTimestamp_not_null_none((Date)value);
        }
        public Date getFieldValue(IdBase instance) {
            return ((TimestampAsUtilDateTypes)instance).getTimestamp_not_null_none();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            Timestamp timestamp = new Timestamp(((Date)value).getTime());
            preparedStatement.setTimestamp(j, timestamp);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getTimestamp(j);
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
