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

import java.math.BigDecimal;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;

import testsuite.clusterj.model.DecimalTypes;
import testsuite.clusterj.model.IdBase;

public class DecimalTypesTest extends AbstractClusterJModelTest {

    /** Test all DecimalTypes columns.
drop table if exists decimaltypes;
create table decimaltypes (
 id int not null primary key,

 decimal_null_hash decimal(10,5),
 decimal_null_btree decimal(10,5),
 decimal_null_both decimal(10,5),
 decimal_null_none decimal(10,5)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_decimal_null_hash using hash on decimaltypes(decimal_null_hash);
create index idx_decimal_null_btree on decimaltypes(decimal_null_btree);
create unique index idx_decimal_null_both on decimaltypes(decimal_null_both);

     */

    /** One of two main tests */
    public void testWriteJDBCReadNDB() {
        writeJDBCreadNDB();
        failOnError();
    }

    /** One of two main tests */
    public void testWriteNDBReadJDBC() {
        writeNDBreadJDBC();
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
        return "decimaltypes";
    }

    /** Subclasses override this method to provide the model class for the test */
    @Override
    protected Class<? extends IdBase> getModelClass() {
        return DecimalTypes.class;
    }

    /** Subclasses override this method to provide values for rows (i) and columns (j) */
    @Override
    protected Object getColumnValue(int i, int j) {
        return BigDecimal.valueOf(i).add(BigDecimal.valueOf(j, 5));
    }

   static ColumnDescriptor decimal_null_hash = new ColumnDescriptor
            ("decimal_null_hash", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((DecimalTypes)instance).setDecimal_null_hash((BigDecimal)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((DecimalTypes)instance).getDecimal_null_hash();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setBigDecimal(j, (BigDecimal)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getBigDecimal(j);
        }
    });

    static ColumnDescriptor decimal_null_btree = new ColumnDescriptor
            ("decimal_null_btree", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((DecimalTypes)instance).setDecimal_null_btree((BigDecimal)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((DecimalTypes)instance).getDecimal_null_btree();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setBigDecimal(j, (BigDecimal)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getBigDecimal(j);
        }
    });
    static ColumnDescriptor decimal_null_both = new ColumnDescriptor
            ("decimal_null_both", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((DecimalTypes)instance).setDecimal_null_both((BigDecimal)value);
        }
        public BigDecimal getFieldValue(IdBase instance) {
            return ((DecimalTypes)instance).getDecimal_null_both();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setBigDecimal(j, (BigDecimal)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getBigDecimal(j);
        }
    });
    static ColumnDescriptor decimal_null_none = new ColumnDescriptor
            ("decimal_null_none", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((DecimalTypes)instance).setDecimal_null_none((BigDecimal)value);
        }
        public BigDecimal getFieldValue(IdBase instance) {
            return ((DecimalTypes)instance).getDecimal_null_none();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setBigDecimal(j, (BigDecimal)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getBigDecimal(j);
        }
    });

    protected static ColumnDescriptor[] columnDescriptors = new ColumnDescriptor[] {
        decimal_null_hash,
        decimal_null_btree,
        decimal_null_both,
        decimal_null_none
        };

    @Override
    protected ColumnDescriptor[] getColumnDescriptors() {
        return columnDescriptors;
    }

}
