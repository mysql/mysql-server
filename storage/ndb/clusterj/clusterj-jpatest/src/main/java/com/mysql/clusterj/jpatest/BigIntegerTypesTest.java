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

import java.math.BigDecimal;
import java.math.BigInteger;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;

import com.mysql.clusterj.jpatest.model.BigIntegerTypes;
import com.mysql.clusterj.jpatest.model.IdBase;


public class BigIntegerTypesTest extends AbstractJPABaseTest {

    /** Test all BigIntegerTypes columns.
drop table if exists bigintegertypes;
create table bigintegertypes (
 id int not null primary key,

 decimal_null_hash decimal(10),
 decimal_null_btree decimal(10),
 decimal_null_both decimal(10),
 decimal_null_none decimal(10)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_decimal_null_hash using hash on bigintegertypes(decimal_null_hash);
create index idx_decimal_null_btree on bigintegertypes(decimal_null_btree);
create unique index idx_decimal_null_both on bigintegertypes(decimal_null_both);

     */

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
        return "bigintegertypes";
    }

    /** Subclasses override this method to provide the model class for the test */
    @Override
    protected Class<? extends IdBase> getModelClass() {
        return BigIntegerTypes.class;
    }

    /** Subclasses must override this method to implement the model factory for the test */
    protected IdBase getNewInstance(Class<? extends IdBase> modelClass) {
        return new BigIntegerTypes();
    }

    /** Subclasses override this method to provide values for rows (i) and columns (j) */
    @Override
    protected Object getColumnValue(int i, int j) {
        return BigInteger.valueOf(i * 10000 + j);
    }

   static ColumnDescriptor decimal_null_hash = new ColumnDescriptor
            ("decimal_null_hash", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((BigIntegerTypes)instance).setDecimal_null_hash((BigInteger)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((BigIntegerTypes)instance).getDecimal_null_hash();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setBigDecimal(j, new BigDecimal((BigInteger)value));
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getBigDecimal(j).toBigIntegerExact();
        }
    });

    static ColumnDescriptor decimal_null_btree = new ColumnDescriptor
            ("decimal_null_btree", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((BigIntegerTypes)instance).setDecimal_null_btree((BigInteger)value);
        }
        public Object getFieldValue(IdBase instance) {
            return ((BigIntegerTypes)instance).getDecimal_null_btree();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setBigDecimal(j, new BigDecimal((BigInteger)value));
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getBigDecimal(j).toBigIntegerExact();
        }
    });
    static ColumnDescriptor decimal_null_both = new ColumnDescriptor
            ("decimal_null_both", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((BigIntegerTypes)instance).setDecimal_null_both((BigInteger)value);
        }
        public BigInteger getFieldValue(IdBase instance) {
            return ((BigIntegerTypes)instance).getDecimal_null_both();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setBigDecimal(j, new BigDecimal((BigInteger)value));
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getBigDecimal(j).toBigIntegerExact();
        }
    });
    static ColumnDescriptor decimal_null_none = new ColumnDescriptor
            ("decimal_null_none", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((BigIntegerTypes)instance).setDecimal_null_none((BigInteger)value);
        }
        public BigInteger getFieldValue(IdBase instance) {
            return ((BigIntegerTypes)instance).getDecimal_null_none();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
                throws SQLException {
            preparedStatement.setBigDecimal(j, new BigDecimal((BigInteger)value));
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getBigDecimal(j).toBigIntegerExact();
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
