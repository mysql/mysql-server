/*
   Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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

import testsuite.clusterj.model.AllPrimitives;
import testsuite.clusterj.model.IdBase;

public class DeleteQueryAllPrimitivesTest extends AbstractQueryTest {

    @Override
    public Class getInstanceType() {
        return AllPrimitives.class;
    }

    @Override
    void createInstances(int number) {
        createAllPrimitivesInstances(10);
    }

    /** Test all single-predicate queries using AllPrimitives.
drop table if exists allprimitives;
create table allprimitives (
 id int not null primary key,

 int_not_null_hash int not null,
 int_not_null_btree int not null,
 int_not_null_both int not null,
 int_not_null_none int not null,
 int_null_hash int,
 int_null_btree int,
 int_null_both int,
 int_null_none int,

 byte_not_null_hash tinyint not null,
 byte_not_null_btree tinyint not null,
 byte_not_null_both tinyint not null,
 byte_not_null_none tinyint not null,
 byte_null_hash tinyint,
 byte_null_btree tinyint,
 byte_null_both tinyint,
 byte_null_none tinyint,

 short_not_null_hash smallint not null,
 short_not_null_btree smallint not null,
 short_not_null_both smallint not null,
 short_not_null_none smallint not null,
 short_null_hash smallint,
 short_null_btree smallint,
 short_null_both smallint,
 short_null_none smallint,

 long_not_null_hash bigint not null,
 long_not_null_btree bigint not null,
 long_not_null_both bigint not null,
 long_not_null_none bigint not null,
 long_null_hash bigint,
 long_null_btree bigint,
 long_null_both bigint,
 long_null_none bigint
     */

    public void testDeleteEqualByPrimaryKey() {
        deleteEqualQuery("id", "PRIMARY", 8, 1);
        deleteEqualQuery("id", "PRIMARY", 8, 0);
        equalQuery("id", "PRIMARY", 8);
        failOnError();
    }

    public void testDeleteEqualByUniqueKey() {
        deleteEqualQuery("int_not_null_hash", "idx_int_not_null_hash", 8, 1);
        deleteEqualQuery("int_not_null_hash", "idx_int_not_null_hash", 8, 0);
        equalQuery("int_not_null_hash", "idx_int_not_null_hash", 8);
        failOnError();
    }

    public void testDeleteEqualByBtreeIndex() {
        deleteEqualQuery("int_not_null_btree", "idx_int_not_null_btree", 8, 1);
        deleteEqualQuery("int_not_null_btree", "idx_int_not_null_btree", 8, 0);
        equalQuery("int_not_null_btree", "idx_int_not_null_btree", 8);
        failOnError();
    }

    public void testDeleteEqualByTableScan() {
        deleteEqualQuery("int_not_null_none", "none", 8, 1);
        deleteEqualQuery("int_not_null_none", "none", 8, 0);
        equalQuery("int_not_null_none", "none", 8);
        failOnError();
    }

    public void testDeleteRangeByBtreeIndex() {
        deleteGreaterThanAndLessThanQuery("int_not_null_btree", "idx_int_not_null_btree", 4, 7, 2);
        deleteGreaterThanAndLessThanQuery("int_not_null_btree", "idx_int_not_null_btree", 4, 7, 0);
        betweenQuery("int_not_null_btree", "idx_int_not_null_btree", 3, 8, 3, 4, 7, 8);
        failOnError();
    }

    public void testDeleteRangeByTableScan() {
        deleteGreaterThanAndLessThanQuery("int_not_null_none", "none", 4, 7, 2);
        deleteGreaterThanAndLessThanQuery("int_not_null_none", "none", 4, 7, 0);
        betweenQuery("int_not_null_btree", "idx_int_not_null_btree", 3, 8, 3, 4, 7, 8);
        failOnError();
    }

    public void testDeleteEqualByPrimaryKeyAutotransaction() {
        setAutotransaction(true);
        deleteEqualQuery("id", "PRIMARY", 8, 1);
        deleteEqualQuery("id", "PRIMARY", 8, 0);
        equalQuery("id", "PRIMARY", 8);
        failOnError();
    }

    public void testDeleteEqualByUniqueKeyAutotransaction() {
        setAutotransaction(true);
        deleteEqualQuery("int_not_null_hash", "idx_int_not_null_hash", 8, 1);
        deleteEqualQuery("int_not_null_hash", "idx_int_not_null_hash", 8, 0);
        equalQuery("int_not_null_hash", "idx_int_not_null_hash", 8);
        failOnError();
    }

    public void testDeleteEqualByBtreeIndexAutotransaction() {
        setAutotransaction(true);
        deleteEqualQuery("int_not_null_btree", "idx_int_not_null_btree", 8, 1);
        deleteEqualQuery("int_not_null_btree", "idx_int_not_null_btree", 8, 0);
        equalQuery("int_not_null_btree", "idx_int_not_null_btree", 8);
        failOnError();
    }

    public void testDeleteEqualByTableScanAutotransaction() {
        setAutotransaction(true);
        deleteEqualQuery("int_not_null_none", "none", 8, 1);
        deleteEqualQuery("int_not_null_none", "none", 8, 0);
        equalQuery("int_not_null_none", "none", 8);
        failOnError();
    }

    public void testDeleteRangeByBtreeIndexAutotransaction() {
        setAutotransaction(true);
        deleteGreaterThanAndLessThanQuery("int_not_null_btree", "idx_int_not_null_btree", 4, 7, 2);
        deleteGreaterThanAndLessThanQuery("int_not_null_btree", "idx_int_not_null_btree", 4, 7, 0);
        betweenQuery("int_not_null_btree", "idx_int_not_null_btree", 3, 8, 3, 4, 7, 8);
        failOnError();
    }

    public void testDeleteRangeByTableScanAutotransaction() {
        setAutotransaction(true);
        deleteGreaterThanAndLessThanQuery("int_not_null_none", "none", 4, 7, 2);
        deleteGreaterThanAndLessThanQuery("int_not_null_none", "none", 4, 7, 0);
        betweenQuery("int_not_null_btree", "idx_int_not_null_btree", 3, 8, 3, 4, 7, 8);
        failOnError();
    }

}
