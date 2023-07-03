/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

import java.util.Date;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;

import testsuite.clusterj.model.DateAsUtilDateTypes;
import testsuite.clusterj.model.IdBase;

/** Test that Dates can be read and written. 
 * case 1: Write using JDBC, read using NDB.
 * case 2: Write using NDB, read using JDBC.
 * Schema
 *
drop table if exists datetypes;
create table datetypes (
 id int not null primary key,

 date_null_hash date,
 date_null_btree date,
 date_null_both date,
 date_null_none date,

 date_not_null_hash date,
 date_not_null_btree date,
 date_not_null_both date,
 date_not_null_none date

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_date_null_hash using hash on datetypes(date_null_hash);
create index idx_date_null_btree on datetypes(date_null_btree);
create unique index idx_date_null_both on datetypes(date_null_both);

create unique index idx_date_not_null_hash using hash on datetypes(date_not_null_hash);
create index idx_date_not_null_btree on datetypes(date_not_null_btree);
create unique index idx_date_not_null_both on datetypes(date_not_null_both);

 */
public class DateAsUtilDateTypesTest extends AbstractClusterJModelTest {

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
        return "datetypes";
    }

    /** Subclasses override this method to provide the model class for the test */
    @Override
    Class<? extends IdBase> getModelClass() {
        return DateAsUtilDateTypes.class;
    }

    /** Subclasses override this method to provide values for rows (i) and columns (j) */
    @Override
    protected Object getColumnValue(int i, int j) {
        return new Date(getMillisFor(1980, i, j + 1));
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
            ("date_not_null_hash", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((DateAsUtilDateTypes)instance).setDate_not_null_hash((Date)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((DateAsUtilDateTypes)instance).getDate_not_null_hash();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            java.sql.Date date = new java.sql.Date(((Date)value).getTime());
            preparedStatement.setDate(j, date);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getDate(j);
        }
    });

    static ColumnDescriptor not_null_btree = new ColumnDescriptor
            ("date_not_null_btree", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((DateAsUtilDateTypes)instance).setDate_not_null_btree((Date)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((DateAsUtilDateTypes)instance).getDate_not_null_btree();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            java.sql.Date date = new java.sql.Date(((Date)value).getTime());
            preparedStatement.setDate(j, date);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getDate(j);
        }
    });
    static ColumnDescriptor not_null_both = new ColumnDescriptor
            ("date_not_null_both", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((DateAsUtilDateTypes)instance).setDate_not_null_both((Date)value);
        }
        public Date getFieldValue(IdBase instance) {
            return ((DateAsUtilDateTypes)instance).getDate_not_null_both();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            java.sql.Date date = new java.sql.Date(((Date)value).getTime());
            preparedStatement.setDate(j, date);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getDate(j);
        }
    });
    static ColumnDescriptor not_null_none = new ColumnDescriptor
            ("date_not_null_none", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((DateAsUtilDateTypes)instance).setDate_not_null_none((Date)value);
        }
        public Date getFieldValue(IdBase instance) {
            return ((DateAsUtilDateTypes)instance).getDate_not_null_none();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            java.sql.Date date = new java.sql.Date(((Date)value).getTime());
            preparedStatement.setDate(j, date);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getDate(j);
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
