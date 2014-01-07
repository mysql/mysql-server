/*
  Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.cluster.crund;

import java.io.PrintWriter;

abstract public class Load {
    // console
    static protected final PrintWriter out = Driver.out;
    static protected final PrintWriter err = Driver.err;

    // short descriptor
    protected String name;
    public String getName() {
        return name;
    }

    // intializers/finalizers
    abstract public void init() throws Exception;
    abstract public void close() throws Exception;

    // datastore operations
    abstract public void initConnection() throws Exception;
    abstract public void closeConnection() throws Exception;
    abstract public void clearData() throws Exception;

    // benchmark operations
    abstract public void runOperations(int nOps) throws Exception;
}
