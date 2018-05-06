/*
   Copyright 2010 Sun Microsystems, Inc.
   Use is subject to license terms.

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

public class QueryNotTest extends AbstractQueryTest {

    @Override
    public Class<?> getInstanceType() {
        return AllPrimitives.class;
    }

    @Override
    void createInstances(int number) {
        createAllPrimitivesInstances(10);
    }

    /** Test "not" queries using AllPrimitives.
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

    public void test() {
        btreeIndexScanInt();
        failOnError();
    }

    public void btreeIndexScanInt() {
        notEqualQuery("int_not_null_btree", "none", 8, 0, 1, 2, 3, 4, 5, 6, 7, 9);
        notNotEqualQuery("int_not_null_btree", "none", 8, 8);
        notNotNotEqualQuery("int_not_null_btree", "none", 8, 0, 1, 2, 3, 4, 5, 6, 7, 9);
        notGreaterEqualQuery("int_not_null_btree", "none", 7, 0, 1, 2, 3, 4, 5, 6);
        notGreaterThanQuery("int_not_null_btree", "none", 6, 0, 1, 2, 3, 4, 5, 6);
        notLessEqualQuery("int_not_null_btree", "none", 4, 5, 6, 7, 8, 9);
        notLessThanQuery("int_not_null_btree", "none", 4, 4, 5, 6, 7, 8, 9);
        notBetweenQuery("int_not_null_btree", "none", 4, 6, 0, 1, 2, 3, 7, 8, 9);
        greaterEqualAndNotGreaterEqualQuery("int_not_null_btree", "idx_int_not_null_btree", 4, 6, 4, 5);
        greaterThanAndNotGreaterEqualQuery("int_not_null_btree", "idx_int_not_null_btree", 4, 6, 5);
        greaterEqualAndNotGreaterThanQuery("int_not_null_btree", "idx_int_not_null_btree", 4, 6, 4, 5, 6);
        greaterThanAndNotGreaterThanQuery("int_not_null_btree", "idx_int_not_null_btree", 4, 6, 5, 6);

        notEqualQuery("int_null_btree", "none", 8, 0, 1, 2, 3, 4, 5, 6, 7, 9);
        notNotEqualQuery("int_null_btree", "none", 8, 8);
        notNotNotEqualQuery("int_null_btree", "none", 8, 0, 1, 2, 3, 4, 5, 6, 7, 9);
        notGreaterEqualQuery("int_null_btree", "none", 7, 0, 1, 2, 3, 4, 5, 6);
        notGreaterThanQuery("int_null_btree", "none", 6, 0, 1, 2, 3, 4, 5, 6);
        notLessEqualQuery("int_null_btree", "none", 4, 5, 6, 7, 8, 9);
        notLessThanQuery("int_null_btree", "none", 4, 4, 5, 6, 7, 8, 9);
        notBetweenQuery("int_null_btree", "none", 4, 6, 0, 1, 2, 3, 7, 8, 9);
        greaterEqualAndNotGreaterEqualQuery("int_null_btree", "idx_int_null_btree", 4, 6, 4, 5);
        greaterThanAndNotGreaterEqualQuery("int_null_btree", "idx_int_null_btree", 4, 6, 5);
        greaterEqualAndNotGreaterThanQuery("int_null_btree", "idx_int_null_btree", 4, 6, 4, 5, 6);
        greaterThanAndNotGreaterThanQuery("int_null_btree", "idx_int_null_btree", 4, 6, 5, 6);
    }

}
