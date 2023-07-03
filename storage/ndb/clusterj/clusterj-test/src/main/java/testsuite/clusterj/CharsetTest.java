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

import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.Charset;
import java.nio.charset.CharsetEncoder;
import java.nio.charset.CoderResult;

import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;

import java.util.ArrayList;
import java.util.List;
import java.util.Properties;

import testsuite.clusterj.model.CharsetLatin1;
import testsuite.clusterj.model.CharsetBig5;
import testsuite.clusterj.model.CharsetModel;
import testsuite.clusterj.model.CharsetSjis;
import testsuite.clusterj.model.CharsetUtf8;

/** Test that all characters in supported character sets can be read and written.

 * 1. Identify which character sets to test.
 * 2. For each character set, create a table with an id column and three VARCHAR columns 
 *    (one with length < 256 another with length > 256, and a third with length > 8000)
 *    with the test character set.
 * 3. For each table, write a persistent interface that maps the table.
 * 4. For each persistent interface:
 *   a) create an empty list of String
 *   b) create a CharBuffer containing all mappable characters for the character set from the range 0:65535
 *   c) map the CharBuffer to a ByteBuffer of length equal to the size of the VARCHAR column
 *   d) create a String from the characters in the CharBuffer that could fit into the column
 *   e) add the String to the list of String
 *   f) continue from c) until all characters have been represented in the list of String
 *   g) use JDBC or clusterj to write a row in the database for each String in the list
 *   h) use JDBC or clusterj to read all rows and compare the String to the list of Strings
 *   i) remove all rows of the table
 *
 */
public class CharsetTest extends AbstractClusterJTest {

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        setAutoCommit(connection, false);
    }

    @Override
    public Properties modifyProperties() {
        // Modify JDBC properties to add server character encoding
        Properties modifiedProps = new Properties();
        modifiedProps.putAll(props);
        // "utf8" here is a Java, rather than MySQL, setting.
        modifiedProps.put("characterEncoding", "utf8");
        return modifiedProps;
    }

    public void testLatin1() {
        testCharset("windows-1252", "charsetlatin1", CharsetLatin1.class);
    }

    public void testUtf8() {
        testCharset("UTF-8", "charsetutf8", CharsetUtf8.class);
    }

    public void testSjis() {
        testCharset("SJIS", "charsetsjis", CharsetSjis.class);
    }

    public void testBig5() {
        testCharset("big5", "charsetbig5", CharsetBig5.class);
    }

    protected void testCharset(String charsetName, String tableName,
            Class<? extends CharsetModel> modelClass) {

        writeJDBCreadJDBC(charsetName, tableName, modelClass, ColumnDescriptor.SMALL);
        writeJDBCreadJDBC(charsetName, tableName, modelClass, ColumnDescriptor.MEDIUM);
        writeJDBCreadJDBC(charsetName, tableName, modelClass, ColumnDescriptor.LARGE);

        writeJDBCreadNDB(charsetName, tableName, modelClass, ColumnDescriptor.SMALL);
        writeJDBCreadNDB(charsetName, tableName, modelClass, ColumnDescriptor.MEDIUM);
        writeJDBCreadNDB(charsetName, tableName, modelClass, ColumnDescriptor.LARGE);

        writeNDBreadJDBC(charsetName, tableName, modelClass, ColumnDescriptor.SMALL);
        writeNDBreadJDBC(charsetName, tableName, modelClass, ColumnDescriptor.MEDIUM);
        writeNDBreadJDBC(charsetName, tableName, modelClass, ColumnDescriptor.LARGE);

        writeNDBreadNDB(charsetName, tableName, modelClass, ColumnDescriptor.SMALL);
        writeNDBreadNDB(charsetName, tableName, modelClass, ColumnDescriptor.MEDIUM);
        writeNDBreadNDB(charsetName, tableName, modelClass, ColumnDescriptor.LARGE);

        failOnError();
    }

    protected void writeJDBCreadJDBC(String charsetName, String tableName, Class<? extends CharsetModel> modelClass,
            ColumnDescriptor columnDescriptor) {
        List<String> result = null;
        List<String> strings = generateStrings(columnDescriptor, charsetName);
        List<CharsetModel> instances = generateInstances(columnDescriptor, modelClass, strings);
        writeToJDBC(columnDescriptor, tableName, instances);
        result = readFromJDBC(columnDescriptor, tableName);
        if (debug) System.out.println("Returned results of size " + result.size());
        verify("writeJDBCreadJDBC ", strings, result, columnDescriptor);
        removeAll(modelClass);
    }

    protected void writeJDBCreadNDB(String charsetName, String tableName, Class<? extends CharsetModel> modelClass,
            ColumnDescriptor columnDescriptor) {
        List<String> result = null;
        List<String> strings = generateStrings(columnDescriptor, charsetName);
        List<CharsetModel> instances = generateInstances(columnDescriptor, modelClass, strings);
        writeToJDBC(columnDescriptor, tableName, instances);
        result = readFromNDB(columnDescriptor, modelClass);
        if (debug) System.out.println("Returned results of size " + result.size());
        verify("writeJDBCreadNDB ", strings, result, columnDescriptor);
        removeAll(modelClass);
    }

    protected void writeNDBreadJDBC(String charsetName, String tableName, Class<? extends CharsetModel> modelClass,
            ColumnDescriptor columnDescriptor) {
        List<String> result = null;
        List<String> strings = generateStrings(columnDescriptor, charsetName);
        List<CharsetModel> instances = generateInstances(columnDescriptor, modelClass, strings);
        writeToNDB(columnDescriptor, instances);
        result = readFromJDBC(columnDescriptor, tableName);
        if (debug) System.out.println("Returned results of size " + result.size());
        verify("writeNDBreadJDBC ", strings, result, columnDescriptor);
        removeAll(modelClass);
    }

    protected void writeNDBreadNDB(String charsetName, String tableName, Class<? extends CharsetModel> modelClass,
            ColumnDescriptor columnDescriptor) {
        List<String> result = null;
        List<String> strings = generateStrings(columnDescriptor, charsetName);
        List<CharsetModel> instances = generateInstances(columnDescriptor, modelClass, strings);
        writeToNDB(columnDescriptor, instances);
        result = readFromNDB(columnDescriptor, modelClass);
        if (debug) System.out.println("Returned results of size " + result.size());
        verify("writeNDBreadNDB ", strings, result, columnDescriptor);
        removeAll(modelClass);
    }

    private void verify(String where, List<String> expecteds, List<String> actuals, ColumnDescriptor columnDescriptor) {
        int maxErrors = 10;
        for (int i = 0; i < expecteds.size(); ++i) {
            String expected = expecteds.get(i);
            String actual = actuals.get(i);
            if (actual == null) {
                error(where + columnDescriptor.columnName + " actual column " + i + " was null.");
            } else {
                int expectedLength = expected.length();
                int actualLength = actual.length();
                errorIfNotEqual(where + "got failure on size of column data for column width " + columnDescriptor.columnWidth + " at row " + i, expectedLength, actualLength);
                if (expectedLength != actualLength) 
                    continue;
                for (int j = 0; j < expected.length(); ++j) {
                    if (--maxErrors > 0) {
                        errorIfNotEqual("Failure to match column data for column width " + columnDescriptor.columnWidth + " at row " + i + " column " + j,
                                expected.codePointAt(j), actual.codePointAt(j));
                    }
                }
            }
        }
    }

    protected List<String> generateStrings(ColumnDescriptor columnDescriptor,
            String charsetName) {
        List<String> result = new ArrayList<String>();
        Charset charset = Charset.forName(charsetName);
        CharBuffer allChars = CharBuffer.allocate(65536);
        CharsetEncoder encoder = charset.newEncoder();
       // add all encodable characters to the buffer
        int count = 0;
        for (int i = 0; i < 65536; ++i) {
            Character ch = (char)i;
            if (encoder.canEncode(ch)) {
                allChars.append(ch);
                ++count;
            }
        }
        if (debug) System.out.print(charsetName + " has " + count + " encodable characters");
        allChars.flip();

        int width = columnDescriptor.getColumnWidth();
        // encode all the characters that fit into the output byte buffer
        boolean done = false;
        byte[] bytes = new byte[width];
        while (!done) {
            int begin = allChars.position();
            allChars.mark();
            ByteBuffer byteBuffer = ByteBuffer.wrap(bytes);
            CoderResult coderResult = encoder.encode(allChars, byteBuffer, false);
            int end = allChars.position();
            int length = end - begin;
            if (length == 0) {
                done = true;
                continue;
            }
            char[] chars = new char[length];
            allChars.reset();
            allChars.get(chars, 0, length);
            String encodable = String.copyValueOf(chars);
            result.add(encodable);
            if (coderResult.isUnderflow()) {
                done = true;
            }
        }
        if (debug) System.out.println(" in " + result.size() + " row(s) of size " + columnDescriptor.columnWidth);
        return result;
    }

    protected List<CharsetModel> generateInstances(ColumnDescriptor columnDescriptor,
            Class<? extends CharsetModel> modelClass, List<String> strings) {
        List<CharsetModel> result = new ArrayList<CharsetModel>();
        for (int i = 0; i < strings.size(); ++i) {
            CharsetModel instance = session.newInstance(modelClass);
            instance.setId(i);
            columnDescriptor.set(instance, strings.get(i));
            result.add(instance);
        }
        if (debug) System.out.println("Created " + result.size() + " instances of " + modelClass.getName());
        return result;
    }

    protected void writeToJDBC(ColumnDescriptor columnDescriptor,
            String tableName, List<CharsetModel> instances) {
        StringBuffer buffer = new StringBuffer("INSERT INTO ");
        buffer.append(tableName);
        buffer.append(" (id, ");
        buffer.append(columnDescriptor.getColumnName());
        buffer.append(") VALUES (?, ?)");
        String statement = buffer.toString();
        if (debug) System.out.println(statement);
        PreparedStatement preparedStatement = null;
        int i = 0;
        String value = "";
        try {
            preparedStatement = connection.prepareStatement(statement);
            if (debug) System.out.println(preparedStatement.toString());
            for (i = 0; i < instances.size(); ++i) {
                CharsetModel instance = instances.get(i);
                preparedStatement.setInt(1, instance.getId());
                value = columnDescriptor.get(instance);
                preparedStatement.setString(2, value);
                preparedStatement.execute();
            }
            connection.commit();
        } catch (SQLException e) {
            throw new RuntimeException("Failed to insert " + tableName + " at instance " + i + " errant string: " + dump(value), e);
        }
    }

    protected void writeToNDB(ColumnDescriptor columnDescriptor, List<CharsetModel> instances) {
        session.currentTransaction().begin();
        for (CharsetModel instance: instances) {
            session.makePersistent(instance);
        }
        session.currentTransaction().commit();
    }

    protected List<String> readFromNDB(ColumnDescriptor columnDescriptor, 
            Class<? extends CharsetModel> modelClass) {
        List<String> result = new ArrayList<String>();
        session.currentTransaction().begin();
        int i = 0;
        boolean done = false;
        while (!done) {
            CharsetModel instance = session.find(modelClass, i++);
            if (instance != null) {
                result.add(columnDescriptor.get(instance));
            } else {
                done = true;
            }
        }
        session.currentTransaction().commit();
        return result;
    }

    protected List<String> readFromJDBC(ColumnDescriptor columnDescriptor,
            String tableName) {
        List<String> result = new ArrayList<String>();
        StringBuffer buffer = new StringBuffer("SELECT id, ");
        buffer.append(columnDescriptor.getColumnName());
        buffer.append(" FROM ");
        buffer.append(tableName);
        buffer.append(" ORDER BY ID");
        String statement = buffer.toString();
        if (debug) System.out.println(statement);
        PreparedStatement preparedStatement = null;
        int i = 0;
        try {
            preparedStatement = connection.prepareStatement(statement);
            ResultSet rs = preparedStatement.executeQuery();
            while (rs.next()) {
                String columnData = rs.getString(2);
                result.add(columnData);
                ++i;
            }
            connection.commit();
        } catch (SQLException e) {
            throw new RuntimeException("Failed to read " + tableName + " at instance " + i, e);
        }
        return result;
    }

    protected enum ColumnDescriptor {
        SMALL(200, "smallcolumn", new InstanceHandler() {
            public void set(CharsetModel instance, String value) {
                instance.setSmallColumn(value);
            }
            public String get(CharsetModel instance) {
                return instance.getSmallColumn();
            }
        }),
        MEDIUM(500, "mediumcolumn", new InstanceHandler() {
            public void set(CharsetModel instance, String value) {
                instance.setMediumColumn(value);
            }
            public String get(CharsetModel instance) {
                return instance.getMediumColumn();
            }
        }),
        LARGE(10000, "largecolumn", new InstanceHandler() {
            public void set(CharsetModel instance, String value) {
                instance.setLargeColumn(value);
            }
            public String get(CharsetModel instance) {
                return instance.getLargeColumn();
            }
        });

        private int columnWidth;

        private String columnName;

        private InstanceHandler instanceHandler;

        public String getColumnName() {
            return columnName;
        }

        public String get(CharsetModel instance) {
            return instanceHandler.get(instance);
        }

        public void set(CharsetModel instance, String string) {
            this.instanceHandler.set(instance, string);
        }

        public int getColumnWidth() {
            return columnWidth;
        }

        private ColumnDescriptor(int width, String name, InstanceHandler instanceHandler) {
            this.columnWidth = width;
            this.columnName = name;
            this.instanceHandler = instanceHandler;
        }

        private interface InstanceHandler {
            void set(CharsetModel instance, String value);
            String get(CharsetModel instance);
            }

    }

    /** The instances for testing. */
    protected List<CharsetModel> charsetTypes = new ArrayList<CharsetModel>();

}
