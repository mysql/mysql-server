/*
  Copyright (c) 2010, 2024, Oracle and/or its affiliates.
   Use is subject to license terms

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

package com.mysql.cluster.crund;

import java.sql.*;

/**
 * A simple utility class for testing connecting to a JDBC database.
 */
public class JdbcDriverTest {
    static public void main(String[] args) throws Exception {
        System.out.println("main()");

        System.out.println();
        System.out.println("properties:");
        final String driver = System.getProperty("jdbc.driver");
        final String url = System.getProperty("jdbc.url");
        final String user = System.getProperty("jdbc.user");
        final String password = System.getProperty("jdbc.password");
        System.out.println("jdbc.driver: " + driver);
        System.out.println("jdbc.url: " + url);
        System.out.println("jdbc.user: " + user);
        System.out.println("jdbc.password: " + password);

        System.out.println();
        System.out.println("load jdbc driver ...");
        if (driver == null) {
            throw new RuntimeException("Missing property: jdbc.driver");
        }
        try {
            //Class.forName(driver);
            Class.forName(driver).newInstance();
        } catch (ClassNotFoundException e) {
            System.out.println("Cannot load JDBC driver '" + driver
                               + "' from classpath '"
                               + System.getProperty("java.class.path") + "'");
            throw e;
        }

        System.out.println();
        System.out.println("testing connection ...");
        if (url == null) {
            throw new RuntimeException("Missing property: jdbc.url");
        }
        try {
            Connection conn = DriverManager.getConnection(url, user, password);
            System.out.println("conn = " + conn);
            conn.close();
        } catch (SQLException e) {
            System.out.println("Cannot connect to database, exception: "
                               + e.getMessage());
            throw e;
        }

        System.out.println();
        System.out.println("main(): done.");
    }
}
