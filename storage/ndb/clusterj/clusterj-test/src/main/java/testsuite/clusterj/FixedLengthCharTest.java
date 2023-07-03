/*
 *  Copyright (c) 2014, 2022, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is also distributed with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have included with MySQL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License, version 2.0, for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package testsuite.clusterj;

import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;

import testsuite.clusterj.AbstractClusterJModelTest;
import testsuite.clusterj.model.CharsetSwedishUtf8;
import testsuite.clusterj.model.IdBase;

/** 
 * Test the boundaries of the fixed width char columns in charserswedishutf8 table.
 * 1. For each Column, create a columnDescriptor and test it for the following :
 *   a) write and read strings of maximum length of the column.
 *   b) verify the written string through query.
 *   c) write strings whose length exceed the maximum and get failure message.
 */
public class FixedLengthCharTest extends AbstractClusterJModelTest{

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        tx = session.currentTransaction();

        tx.begin();
        session.deletePersistentAll(CharsetSwedishUtf8.class);
        try {
            tx.commit();
        } catch (Exception ex) {
            // ignore exceptions -- might not be any instances to delete
        }
        addTearDownClasses(CharsetSwedishUtf8.class);
    }

    public void testUtf8(){
        writeRead(1, utf_column, "aaa");
        writeRead(2, utf_column, "aaaa");	
        writeRead(3, utf_column, "aaaaaaaaaaaa");
        writeRead(4, utf_column, "\u20AC\u20AC\u20AC\u20AC");
        
        writeRead(5, utf_column, "aaaa  ", "aaaa");
        writeRead(6, utf_column, "\u20AC\u20AC\u20AC  ", "\u20AC\u20AC\u20AC");

        writeFail(20, utf_column, "aaaaaaaaaaaaa", ".*Data length 13 too long.*");

        failOnError();
    }
    
    public void testSwedish(){
        writeRead(7, swedish_column, "aaa");
        writeRead(8, swedish_column, "aaaa");
        
        writeRead(9, swedish_column, "aa  ", "aa");

        writeFail(21, swedish_column, "aaaaa", ".*Data length 5 too long..*");

        failOnError();
    }

    /**
     * A test to write the given string to the column specified in columnDescriptor, 
     * and read back via find to verify it with the expected input.
     */
    protected void writeRead(int id, ColumnDescriptor columnDescriptor, String output, String expected){
        CharsetSwedishUtf8 instance = session.newInstance(CharsetSwedishUtf8.class);
        instance.setId(id);
        columnDescriptor.setFieldValue(instance, output);
        session.currentTransaction().begin();
        session.persist(instance);
        session.currentTransaction().commit();

        session.currentTransaction().begin();
        instance = session.find(CharsetSwedishUtf8.class,id);
        session.currentTransaction().commit();

        errorIfNotEqual("Failure on reading data from " + columnDescriptor.getColumnName(), expected,
                        columnDescriptor.getFieldValue(instance));
    }

    /**
     * A test to write the given string to the column specified in columnDescriptor, 
     * and read back via find and JDBC query to verify it with the given output.
     */
    protected void writeRead(int id, ColumnDescriptor columnDescriptor, String output){
        writeRead(id, columnDescriptor, output, output);
        //querying and verifying the result
        queryAndVerifyResults(columnDescriptor.getColumnName() + " = " + output, columnDescriptors,
                              columnDescriptor.getColumnName() + " = ?", new String[] {output}, id);
    }

    /**
     * Run a negative test that tries to write the output string
     * and expects the resulting exception to contain the exception pattern
     */
    protected void writeFail(int id, ColumnDescriptor columnDescriptor, String output, String exceptionPattern){
        CharsetSwedishUtf8 instance = session.newInstance(CharsetSwedishUtf8.class);
        Exception caughtException = null;
        try{
            columnDescriptor.setFieldValue(instance, output);
        }catch(Exception ex){
            caughtException = ex;
        }
        verifyException("Writing invalid values to the column",
                caughtException, exceptionPattern);
    }

    static ColumnDescriptor utf_column = new ColumnDescriptor("utfcolumn", new InstanceHandler() {
        public void setFieldValue(IdBase instance, Object value) {
            ((CharsetSwedishUtf8)instance).setUtfColumn((String)value);
        }
        public String getFieldValue(IdBase instance) {
			return ((CharsetSwedishUtf8)instance).getUtfColumn();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
            throws SQLException {
            preparedStatement.setString(j, (String)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getString(j);
        }
    });

    static ColumnDescriptor swedish_column = new ColumnDescriptor("swedishcolumn", new InstanceHandler(){
        public void setFieldValue(IdBase instance, Object value) {
            ((CharsetSwedishUtf8)instance).setSwedishColumn((String)value);
        }
        public String getFieldValue(IdBase instance) {
            return ((CharsetSwedishUtf8)instance).getSwedishColumn();
        }
        public void setPreparedStatementValue(PreparedStatement preparedStatement, int j, Object value)
            throws SQLException {
            preparedStatement.setString(j, (String)value);
        }
        public Object getResultSetValue(ResultSet rs, int j) throws SQLException {
            return rs.getString(j);
        }
    });

    protected static ColumnDescriptor[] columnDescriptors = new ColumnDescriptor[] {
        utf_column,
        swedish_column
    };

    @Override
    protected ColumnDescriptor[] getColumnDescriptors() {
        return columnDescriptors;
    }

    @Override
    protected String getTableName() {
        return "charsetswedishutf8";
    }
}

