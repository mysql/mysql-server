/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj.openjpatest;

import com.mysql.clusterj.jpatest.AbstractJPABaseTest;
import com.mysql.clusterj.jpatest.JpaLoad;
import com.mysql.clusterj.jpatest.SlowTest;

/**
 *
 */
@SlowTest
public class CrundTest extends AbstractJPABaseTest {

    @Override
    public void setUp() {
//        super.setUp();
    }

    public void testCrundOpenjpaMysql() {
        String argumentString =
                "-p crundOpenjpaMysql.properties " +
                "-p crundRun.properties";
        String[] args = argumentString.split(" ");
        JpaLoad.main(args);
    }

    public void testCrundOpenjpaClusterj() {
        String argumentString =
                "-p crundOpenjpaClusterj.properties " +
                "-p crundRun.properties";
        String[] args = argumentString.split(" ");
        JpaLoad.main(args);
    }

}
