/*
 *  Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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

package jdbctest;

public class BigIntegerTypesTest extends testsuite.clusterj.BigIntegerTypesTest {

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

    @Override
    protected boolean getCleanupAfterTest() {
        return false;
    }

    /** One of two tests in the superclass that we don't want to run */
    @Override
    public void testWriteJDBCReadNDB() {
    }

    /** One of two tests in the superclass that we don't want to run */
    @Override
    public void testWriteNDBReadJDBC() {
   }

    /** The test we want */
    public void testWriteJDBCReadJDBC() {
        writeJDBCreadJDBC();
        failOnError();
    }

}
