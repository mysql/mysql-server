/*
   Copyright 2010 Sun Microsystems, Inc.
   All rights reserved. Use is subject to license terms.

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

package com.mysql.clusterj.jpatest;

import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Timestamp;

import com.mysql.clusterj.jpatest.model.TimestampAsSqlTimestampTypes;
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
public class TimestampAsSqlTimestampTest extends AbstractJPABaseTest {

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
        return "timestamptypes";
    }

    /** Subclasses override this method to provide the model class for the test */
    @Override
    protected Class<? extends IdBase> getModelClass() {
        return TimestampAsSqlTimestampTypes.class;
    }

    /** Subclasses override this method to provide values for rows (i) and columns (j) */
    @Override
    protected Object getColumnValue(int i, int j) {
        return new Timestamp(getMillisFor(1980, 0, i + 1, 0, 0, j));
    }

    @Override
    /** Subclasses must override this method to implement the model factory for the test */
    protected IdBase getNewInstance(Class<? extends IdBase> modelClass) {
        return new TimestampAsSqlTimestampTypes();
    }

    public void testWriteJDBCReadJPA() {
        // writeJDBCreadJPA();
        // failOnError();
    }

    public void testWriteJPAReadJDBC() {
        writeJPAreadJDBC();
        failOnError();
   }

    public void testWriteJDBCReadJDBC() {
//        writeJDBCreadJDBC();
//        failOnError();
    }

    public void testWriteJPAReadJPA() {
        writeJPAreadJPA();
        failOnError();
   }

   static ColumnDescriptor not_null_hash = new ColumnDescriptor
            ("timestamp_not_null_hash", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((TimestampAsSqlTimestampTypes)instance).setTimestamp_not_null_hash((Timestamp)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((TimestampAsSqlTimestampTypes)instance).getTimestamp_not_null_hash();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setTimestamp(j, (Timestamp)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getTimestamp(j);
        }
    });

    static ColumnDescriptor not_null_btree = new ColumnDescriptor
            ("timestamp_not_null_btree", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((TimestampAsSqlTimestampTypes)instance).setTimestamp_not_null_btree((Timestamp)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((TimestampAsSqlTimestampTypes)instance).getTimestamp_not_null_btree();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setTimestamp(j, (Timestamp)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getTimestamp(j);
        }
    });

    static ColumnDescriptor not_null_both = new ColumnDescriptor
            ("timestamp_not_null_both", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((TimestampAsSqlTimestampTypes)instance).setTimestamp_not_null_both((Timestamp)value);
        }
        public Timestamp getFieldValue(IdBase instance) {
            return ((TimestampAsSqlTimestampTypes)instance).getTimestamp_not_null_both();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setTimestamp(j, (Timestamp)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getTimestamp(j);
        }
    });

    static ColumnDescriptor not_null_none = new ColumnDescriptor
            ("timestamp_not_null_none", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((TimestampAsSqlTimestampTypes)instance).setTimestamp_not_null_none((Timestamp)value);
        }
        public Timestamp getFieldValue(IdBase instance) {
            return ((TimestampAsSqlTimestampTypes)instance).getTimestamp_not_null_none();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setTimestamp(j, (Timestamp)value);
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
