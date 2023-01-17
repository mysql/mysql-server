/*
   Copyright (c) 2010, 2023, Oracle and/or its affiliates.
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
import testsuite.clusterj.model.IdBase;

public class QueryAllPrimitivesTest extends AbstractQueryTest {

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

    public void test() {
        btreeIndexScanInt();
        hashIndexScanInt();
        bothIndexScanInt();
        noneIndexScanInt();

        btreeIndexScanByte();
        hashIndexScanByte();
        bothIndexScanByte();
        noneIndexScanByte();

        btreeIndexScanShort();
        hashIndexScanShort();
        bothIndexScanShort();
        noneIndexScanShort();

        btreeIndexScanLong();
        hashIndexScanLong();
        bothIndexScanLong();
        noneIndexScanLong();
        failOnError();
    }

    public void btreeIndexScanInt() {
        equalQuery("int_not_null_btree", "idx_int_not_null_btree", 8, 8);
        greaterEqualQuery("int_not_null_btree", "idx_int_not_null_btree", 7, 7, 8, 9);
        greaterThanQuery("int_not_null_btree", "idx_int_not_null_btree", 6, 7, 8, 9);
        lessEqualQuery("int_not_null_btree", "idx_int_not_null_btree", 4, 4, 3, 2, 1, 0);
        lessThanQuery("int_not_null_btree", "idx_int_not_null_btree", 4, 3, 2, 1, 0);
        betweenQuery("int_not_null_btree", "idx_int_not_null_btree", 4, 6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("int_not_null_btree", "idx_int_not_null_btree", 4, 6, 4, 5, 6);
        greaterThanAndLessEqualQuery("int_not_null_btree", "idx_int_not_null_btree", 4, 6, 5, 6);
        greaterEqualAndLessThanQuery("int_not_null_btree", "idx_int_not_null_btree", 4, 6, 4, 5);
        greaterThanAndLessThanQuery("int_not_null_btree", "idx_int_not_null_btree", 4, 6, 5);

        equalQuery("int_null_btree", "idx_int_null_btree", 8, 8);
        greaterEqualQuery("int_null_btree", "idx_int_null_btree", 7, 7, 8, 9);
        greaterThanQuery("int_null_btree", "idx_int_null_btree", 6, 7, 8, 9);
        lessEqualQuery("int_null_btree", "idx_int_null_btree", 4, 4, 3, 2, 1, 0);
        lessThanQuery("int_null_btree", "idx_int_null_btree", 4, 3, 2, 1, 0);
        betweenQuery("int_null_btree", "idx_int_null_btree", 4, 6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("int_null_btree", "idx_int_null_btree", 4, 6, 4, 5, 6);
        greaterThanAndLessEqualQuery("int_null_btree", "idx_int_null_btree", 4, 6, 5, 6);
        greaterEqualAndLessThanQuery("int_null_btree", "idx_int_null_btree", 4, 6, 4, 5);
        greaterThanAndLessThanQuery("int_null_btree", "idx_int_null_btree", 4, 6, 5);
    }

    public void hashIndexScanInt() {
        equalQuery("int_not_null_hash", "idx_int_not_null_hash", 8, 8);
        greaterEqualQuery("int_not_null_hash", "none", 7, 7, 8, 9);
        greaterThanQuery("int_not_null_hash", "none", 6, 7, 8, 9);
        lessEqualQuery("int_not_null_hash", "none", 4, 4, 3, 2, 1, 0);
        lessThanQuery("int_not_null_hash", "none", 4, 3, 2, 1, 0);
        betweenQuery("int_not_null_hash", "none", 4, 6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("int_not_null_hash", "none", 4, 6, 4, 5, 6);
        greaterThanAndLessEqualQuery("int_not_null_hash", "none", 4, 6, 5, 6);
        greaterEqualAndLessThanQuery("int_not_null_hash", "none", 4, 6, 4, 5);
        greaterThanAndLessThanQuery("int_not_null_hash", "none", 4, 6, 5);

        equalQuery("int_null_hash", "idx_int_null_hash", 8, 8);
        greaterEqualQuery("int_null_hash", "none", 7, 7, 8, 9);
        greaterThanQuery("int_null_hash", "none", 6, 7, 8, 9);
        lessEqualQuery("int_null_hash", "none", 4, 4, 3, 2, 1, 0);
        lessThanQuery("int_null_hash", "none", 4, 3, 2, 1, 0);
        betweenQuery("int_null_hash", "none", 4, 6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("int_null_hash", "none", 4, 6, 4, 5, 6);
        greaterThanAndLessEqualQuery("int_null_hash", "none", 4, 6, 5, 6);
        greaterEqualAndLessThanQuery("int_null_hash", "none", 4, 6, 4, 5);
        greaterThanAndLessThanQuery("int_null_hash", "none", 4, 6, 5);
    }

    public void bothIndexScanInt() {
        equalQuery("int_not_null_both", "idx_int_not_null_both", 8, 8);
        greaterEqualQuery("int_not_null_both", "idx_int_not_null_both", 7, 7, 8, 9);
        greaterThanQuery("int_not_null_both", "idx_int_not_null_both", 6, 7, 8, 9);
        lessEqualQuery("int_not_null_both", "idx_int_not_null_both", 4, 4, 3, 2, 1, 0);
        lessThanQuery("int_not_null_both", "idx_int_not_null_both", 4, 3, 2, 1, 0);
        betweenQuery("int_not_null_both", "idx_int_not_null_both", 4, 6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("int_not_null_both", "idx_int_not_null_both", 4, 6, 4, 5, 6);
        greaterThanAndLessEqualQuery("int_not_null_both", "idx_int_not_null_both", 4, 6, 5, 6);
        greaterEqualAndLessThanQuery("int_not_null_both", "idx_int_not_null_both", 4, 6, 4, 5);
        greaterThanAndLessThanQuery("int_not_null_both", "idx_int_not_null_both", 4, 6, 5);

        equalQuery("int_null_both", "idx_int_null_both", 8, 8);
        greaterEqualQuery("int_null_both", "idx_int_null_both", 7, 7, 8, 9);
        greaterThanQuery("int_null_both", "idx_int_null_both", 6, 7, 8, 9);
        lessEqualQuery("int_null_both", "idx_int_null_both", 4, 4, 3, 2, 1, 0);
        lessThanQuery("int_null_both", "idx_int_null_both", 4, 3, 2, 1, 0);
        betweenQuery("int_null_both", "idx_int_null_both", 4, 6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("int_null_both", "idx_int_null_both", 4, 6, 4, 5, 6);
        greaterThanAndLessEqualQuery("int_null_both", "idx_int_null_both", 4, 6, 5, 6);
        greaterEqualAndLessThanQuery("int_null_both", "idx_int_null_both", 4, 6, 4, 5);
        greaterThanAndLessThanQuery("int_null_both", "idx_int_null_both", 4, 6, 5);
    }

    public void noneIndexScanInt() {
        equalQuery("int_not_null_none", "none", 8, 8);
        greaterEqualQuery("int_not_null_none", "none", 7, 7, 8, 9);
        greaterThanQuery("int_not_null_none", "none", 6, 7, 8, 9);
        lessEqualQuery("int_not_null_none", "none", 4, 4, 3, 2, 1, 0);
        lessThanQuery("int_not_null_none", "none", 4, 3, 2, 1, 0);
        betweenQuery("int_not_null_none", "none", 4, 6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("int_not_null_none", "none", 4, 6, 4, 5, 6);
        greaterThanAndLessEqualQuery("int_not_null_none", "none", 4, 6, 5, 6);
        greaterEqualAndLessThanQuery("int_not_null_none", "none", 4, 6, 4, 5);
        greaterThanAndLessThanQuery("int_not_null_none", "none", 4, 6, 5);

        equalQuery("int_not_null_none", "none", 8, 8);
        greaterEqualQuery("int_not_null_none", "none", 7, 7, 8, 9);
        greaterThanQuery("int_not_null_none", "none", 6, 7, 8, 9);
        lessEqualQuery("int_not_null_none", "none", 4, 4, 3, 2, 1, 0);
        lessThanQuery("int_not_null_none", "none", 4, 3, 2, 1, 0);
        betweenQuery("int_not_null_none", "none", 4, 6, 4, 5, 6);

        greaterEqualAndLessEqualQuery("int_not_null_none", "none", 4, 6, 4, 5, 6);
        greaterThanAndLessEqualQuery("int_not_null_none", "none", 4, 6, 5, 6);
        greaterEqualAndLessThanQuery("int_not_null_none", "none", 4, 6, 4, 5);
        greaterThanAndLessThanQuery("int_not_null_none", "none", 4, 6, 5);
    }

    public void btreeIndexScanByte() {
        equalQuery("byte_not_null_btree", "idx_byte_not_null_btree", (byte)8, 8);
        greaterEqualQuery("byte_not_null_btree", "idx_byte_not_null_btree", (byte)7, 7, 8, 9);
        greaterThanQuery("byte_not_null_btree", "idx_byte_not_null_btree", (byte)6, 7, 8, 9);
        lessEqualQuery("byte_not_null_btree", "idx_byte_not_null_btree", (byte)4, 4, 3, 2, 1, 0);
        lessThanQuery("byte_not_null_btree", "idx_byte_not_null_btree", (byte)4, 3, 2, 1, 0);
        betweenQuery("byte_not_null_btree", "idx_byte_not_null_btree", (byte)4, (byte)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("byte_not_null_btree", "idx_byte_not_null_btree", (byte)4, (byte)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("byte_not_null_btree", "idx_byte_not_null_btree", (byte)4, (byte)6, 5, 6);
        greaterEqualAndLessThanQuery("byte_not_null_btree", "idx_byte_not_null_btree", (byte)4, (byte)6, 4, 5);
        greaterThanAndLessThanQuery("byte_not_null_btree", "idx_byte_not_null_btree", (byte)4, (byte)6, 5);

        equalQuery("byte_null_btree", "idx_byte_null_btree", (byte)8, 8);
        greaterEqualQuery("byte_null_btree", "idx_byte_null_btree", (byte)7, 7, 8, 9);
        greaterThanQuery("byte_null_btree", "idx_byte_null_btree", (byte)6, 7, 8, 9);
        lessEqualQuery("byte_null_btree", "idx_byte_null_btree", (byte)4, 4, 3, 2, 1, 0);
        lessThanQuery("byte_null_btree", "idx_byte_null_btree", (byte)4, 3, 2, 1, 0);
        betweenQuery("byte_null_btree", "idx_byte_null_btree", (byte)4, (byte)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("byte_null_btree", "idx_byte_null_btree", (byte)4, (byte)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("byte_null_btree", "idx_byte_null_btree", (byte)4, (byte)6, 5, 6);
        greaterEqualAndLessThanQuery("byte_null_btree", "idx_byte_null_btree", (byte)4, (byte)6, 4, 5);
        greaterThanAndLessThanQuery("byte_null_btree", "idx_byte_null_btree", (byte)4, (byte)6, 5);
    }

    public void hashIndexScanByte() {
        equalQuery("byte_not_null_hash", "idx_byte_not_null_hash", (byte)8, 8);
        greaterEqualQuery("byte_not_null_hash", "none", (byte)7, 7, 8, 9);
        greaterThanQuery("byte_not_null_hash", "none", (byte)6, 7, 8, 9);
        lessEqualQuery("byte_not_null_hash", "none", (byte)4, 4, 3, 2, 1, 0);
        lessThanQuery("byte_not_null_hash", "none", (byte)4, 3, 2, 1, 0);
        betweenQuery("byte_not_null_hash", "none", (byte)4, (byte)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("byte_not_null_hash", "none", (byte)4, (byte)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("byte_not_null_hash", "none", (byte)4, (byte)6, 5, 6);
        greaterEqualAndLessThanQuery("byte_not_null_hash", "none", (byte)4, (byte)6, 4, 5);
        greaterThanAndLessThanQuery("byte_not_null_hash", "none", (byte)4, (byte)6, 5);

        equalQuery("byte_null_hash", "idx_byte_null_hash", (byte)8, 8);
        greaterEqualQuery("byte_null_hash", "none", (byte)7, 7, 8, 9);
        greaterThanQuery("byte_null_hash", "none", (byte)6, 7, 8, 9);
        lessEqualQuery("byte_null_hash", "none", (byte)4, 4, 3, 2, 1, 0);
        lessThanQuery("byte_null_hash", "none", (byte)4, 3, 2, 1, 0);
        betweenQuery("byte_null_hash", "none", (byte)4, (byte)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("byte_null_hash", "none", (byte)4, (byte)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("byte_null_hash", "none", (byte)4, (byte)6, 5, 6);
        greaterEqualAndLessThanQuery("byte_null_hash", "none", (byte)4, (byte)6, 4, 5);
        greaterThanAndLessThanQuery("byte_null_hash", "none", (byte)4, (byte)6, 5);
    }

    public void bothIndexScanByte() {
        equalQuery("byte_not_null_both", "idx_byte_not_null_both", (byte)8, 8);
        greaterEqualQuery("byte_not_null_both", "idx_byte_not_null_both", (byte)7, 7, 8, 9);
        greaterThanQuery("byte_not_null_both", "idx_byte_not_null_both", (byte)6, 7, 8, 9);
        lessEqualQuery("byte_not_null_both", "idx_byte_not_null_both", (byte)4, 4, 3, 2, 1, 0);
        lessThanQuery("byte_not_null_both", "idx_byte_not_null_both", (byte)4, 3, 2, 1, 0);
        betweenQuery("byte_not_null_both", "idx_byte_not_null_both", (byte)4, (byte)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("byte_not_null_both", "idx_byte_not_null_both", (byte)4, (byte)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("byte_not_null_both", "idx_byte_not_null_both", (byte)4, (byte)6, 5, 6);
        greaterEqualAndLessThanQuery("byte_not_null_both", "idx_byte_not_null_both", (byte)4, (byte)6, 4, 5);
        greaterThanAndLessThanQuery("byte_not_null_both", "idx_byte_not_null_both", (byte)4, (byte)6, 5);

        equalQuery("byte_null_both", "idx_byte_null_both", (byte)8, 8);
        greaterEqualQuery("byte_null_both", "idx_byte_null_both", (byte)7, 7, 8, 9);
        greaterThanQuery("byte_null_both", "idx_byte_null_both", (byte)6, 7, 8, 9);
        lessEqualQuery("byte_null_both", "idx_byte_null_both", (byte)4, 4, 3, 2, 1, 0);
        lessThanQuery("byte_null_both", "idx_byte_null_both", (byte)4, 3, 2, 1, 0);
        betweenQuery("byte_null_both", "idx_byte_null_both", (byte)4, (byte)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("byte_null_both", "idx_byte_null_both", (byte)4, (byte)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("byte_null_both", "idx_byte_null_both", (byte)4, (byte)6, 5, 6);
        greaterEqualAndLessThanQuery("byte_null_both", "idx_byte_null_both", (byte)4, (byte)6, 4, 5);
        greaterThanAndLessThanQuery("byte_null_both", "idx_byte_null_both", (byte)4, (byte)6, 5);
    }

    public void noneIndexScanByte() {
        equalQuery("byte_not_null_none", "none", (byte)8, 8);
        greaterEqualQuery("byte_not_null_none", "none", (byte)7, 7, 8, 9);
        greaterThanQuery("byte_not_null_none", "none", (byte)6, 7, 8, 9);
        lessEqualQuery("byte_not_null_none", "none", (byte)4, 4, 3, 2, 1, 0);
        lessThanQuery("byte_not_null_none", "none", (byte)4, 3, 2, 1, 0);
        betweenQuery("byte_not_null_none", "none", (byte)4, (byte)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("byte_not_null_none", "none", (byte)4, (byte)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("byte_not_null_none", "none", (byte)4, (byte)6, 5, 6);
        greaterEqualAndLessThanQuery("byte_not_null_none", "none", (byte)4, (byte)6, 4, 5);
        greaterThanAndLessThanQuery("byte_not_null_none", "none", (byte)4, (byte)6, 5);

        equalQuery("byte_null_none", "none", (byte)8, 8);
        greaterEqualQuery("byte_null_none", "none", (byte)7, 7, 8, 9);
        greaterThanQuery("byte_null_none", "none", (byte)6, 7, 8, 9);
        lessEqualQuery("byte_null_none", "none", (byte)4, 4, 3, 2, 1, 0);
        lessThanQuery("byte_null_none", "none", (byte)4, 3, 2, 1, 0);
        betweenQuery("byte_null_none", "none", (byte)4, (byte)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("byte_null_none", "none", (byte)4, (byte)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("byte_null_none", "none", (byte)4, (byte)6, 5, 6);
        greaterEqualAndLessThanQuery("byte_null_none", "none", (byte)4, (byte)6, 4, 5);
        greaterThanAndLessThanQuery("byte_null_none", "none", (byte)4, (byte)6, 5);
    }

    public void btreeIndexScanShort() {
        equalQuery("short_not_null_btree", "idx_short_not_null_btree", (short)8, 8);
        greaterEqualQuery("short_not_null_btree", "idx_short_not_null_btree", (short)7, 7, 8, 9);
        greaterThanQuery("short_not_null_btree", "idx_short_not_null_btree", (short)6, 7, 8, 9);
        lessEqualQuery("short_not_null_btree", "idx_short_not_null_btree", (short)4, 4, 3, 2, 1, 0);
        lessThanQuery("short_not_null_btree", "idx_short_not_null_btree", (short)4, 3, 2, 1, 0);
        betweenQuery("short_not_null_btree", "idx_short_not_null_btree", (short)4, (short)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("short_not_null_btree", "idx_short_not_null_btree", (short)4, (short)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("short_not_null_btree", "idx_short_not_null_btree", (short)4, (short)6, 5, 6);
        greaterEqualAndLessThanQuery("short_not_null_btree", "idx_short_not_null_btree", (short)4, (short)6, 4, 5);
        greaterThanAndLessThanQuery("short_not_null_btree", "idx_short_not_null_btree", (short)4, (short)6, 5);

        equalQuery("short_null_btree", "idx_short_null_btree", (short)8, 8);
        greaterEqualQuery("short_null_btree", "idx_short_null_btree", (short)7, 7, 8, 9);
        greaterThanQuery("short_null_btree", "idx_short_null_btree", (short)6, 7, 8, 9);
        lessEqualQuery("short_null_btree", "idx_short_null_btree", (short)4, 4, 3, 2, 1, 0);
        lessThanQuery("short_null_btree", "idx_short_null_btree", (short)4, 3, 2, 1, 0);
        betweenQuery("short_null_btree", "idx_short_null_btree", (short)4, (short)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("short_null_btree", "idx_short_null_btree", (short)4, (short)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("short_null_btree", "idx_short_null_btree", (short)4, (short)6, 5, 6);
        greaterEqualAndLessThanQuery("short_null_btree", "idx_short_null_btree", (short)4, (short)6, 4, 5);
        greaterThanAndLessThanQuery("short_null_btree", "idx_short_null_btree", (short)4, (short)6, 5);
    }

    public void hashIndexScanShort() {
        equalQuery("short_not_null_hash", "idx_short_not_null_hash", (short)8, 8);
        greaterEqualQuery("short_not_null_hash", "none", (short)7, 7, 8, 9);
        greaterThanQuery("short_not_null_hash", "none", (short)6, 7, 8, 9);
        lessEqualQuery("short_not_null_hash", "none", (short)4, 4, 3, 2, 1, 0);
        lessThanQuery("short_not_null_hash", "none", (short)4, 3, 2, 1, 0);
        betweenQuery("short_not_null_hash", "none", (short)4, (short)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("short_not_null_hash", "none", (short)4, (short)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("short_not_null_hash", "none", (short)4, (short)6, 5, 6);
        greaterEqualAndLessThanQuery("short_not_null_hash", "none", (short)4, (short)6, 4, 5);
        greaterThanAndLessThanQuery("short_not_null_hash", "none", (short)4, (short)6, 5);

        equalQuery("short_null_hash", "idx_short_null_hash", (short)8, 8);
        greaterEqualQuery("short_null_hash", "none", (short)7, 7, 8, 9);
        greaterThanQuery("short_null_hash", "none", (short)6, 7, 8, 9);
        lessEqualQuery("short_null_hash", "none", (short)4, 4, 3, 2, 1, 0);
        lessThanQuery("short_null_hash", "none", (short)4, 3, 2, 1, 0);
        betweenQuery("short_null_hash", "none", (short)4, (short)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("short_null_hash", "none", (short)4, (short)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("short_null_hash", "none", (short)4, (short)6, 5, 6);
        greaterEqualAndLessThanQuery("short_null_hash", "none", (short)4, (short)6, 4, 5);
        greaterThanAndLessThanQuery("short_null_hash", "none", (short)4, (short)6, 5);
    }

    public void bothIndexScanShort() {
        equalQuery("short_not_null_both", "idx_short_not_null_both", (short)8, 8);
        greaterEqualQuery("short_not_null_both", "idx_short_not_null_both", (short)7, 7, 8, 9);
        greaterThanQuery("short_not_null_both", "idx_short_not_null_both", (short)6, 7, 8, 9);
        lessEqualQuery("short_not_null_both", "idx_short_not_null_both", (short)4, 4, 3, 2, 1, 0);
        lessThanQuery("short_not_null_both", "idx_short_not_null_both", (short)4, 3, 2, 1, 0);
        betweenQuery("short_not_null_both", "idx_short_not_null_both", (short)4, (short)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("short_not_null_both", "idx_short_not_null_both", (short)4, (short)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("short_not_null_both", "idx_short_not_null_both", (short)4, (short)6, 5, 6);
        greaterEqualAndLessThanQuery("short_not_null_both", "idx_short_not_null_both", (short)4, (short)6, 4, 5);
        greaterThanAndLessThanQuery("short_not_null_both", "idx_short_not_null_both", (short)4, (short)6, 5);

        equalQuery("short_null_both", "idx_short_null_both", (short)8, 8);
        greaterEqualQuery("short_null_both", "idx_short_null_both", (short)7, 7, 8, 9);
        greaterThanQuery("short_null_both", "idx_short_null_both", (short)6, 7, 8, 9);
        lessEqualQuery("short_null_both", "idx_short_null_both", (short)4, 4, 3, 2, 1, 0);
        lessThanQuery("short_null_both", "idx_short_null_both", (short)4, 3, 2, 1, 0);
        betweenQuery("short_null_both", "idx_short_null_both", (short)4, (short)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("short_null_both", "idx_short_null_both", (short)4, (short)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("short_null_both", "idx_short_null_both", (short)4, (short)6, 5, 6);
        greaterEqualAndLessThanQuery("short_null_both", "idx_short_null_both", (short)4, (short)6, 4, 5);
        greaterThanAndLessThanQuery("short_null_both", "idx_short_null_both", (short)4, (short)6, 5);
    }

    public void noneIndexScanShort() {
        equalQuery("short_not_null_none", "none", (short)8, 8);
        greaterEqualQuery("short_not_null_none", "none", (short)7, 7, 8, 9);
        greaterThanQuery("short_not_null_none", "none", (short)6, 7, 8, 9);
        lessEqualQuery("short_not_null_none", "none", (short)4, 4, 3, 2, 1, 0);
        lessThanQuery("short_not_null_none", "none", (short)4, 3, 2, 1, 0);
        betweenQuery("short_not_null_none", "none", (short)4, (short)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("short_not_null_none", "none", (short)4, (short)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("short_not_null_none", "none", (short)4, (short)6, 5, 6);
        greaterEqualAndLessThanQuery("short_not_null_none", "none", (short)4, (short)6, 4, 5);
        greaterThanAndLessThanQuery("short_not_null_none", "none", (short)4, (short)6, 5);

        equalQuery("short_null_none", "none", (short)8, 8);
        greaterEqualQuery("short_null_none", "none", (short)7, 7, 8, 9);
        greaterThanQuery("short_null_none", "none", (short)6, 7, 8, 9);
        lessEqualQuery("short_null_none", "none", (short)4, 4, 3, 2, 1, 0);
        lessThanQuery("short_null_none", "none", (short)4, 3, 2, 1, 0);
        betweenQuery("short_null_none", "none", (short)4, (short)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("short_null_none", "none", (short)4, (short)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("short_null_none", "none", (short)4, (short)6, 5, 6);
        greaterEqualAndLessThanQuery("short_null_none", "none", (short)4, (short)6, 4, 5);
        greaterThanAndLessThanQuery("short_null_none", "none", (short)4, (short)6, 5);
    }

    public void btreeIndexScanLong() {
        equalQuery("long_not_null_btree", "idx_long_not_null_btree", (long)8, 8);
        greaterEqualQuery("long_not_null_btree", "idx_long_not_null_btree", (long)7, 7, 8, 9);
        greaterThanQuery("long_not_null_btree", "idx_long_not_null_btree", (long)6, 7, 8, 9);
        lessEqualQuery("long_not_null_btree", "idx_long_not_null_btree", (long)4, 4, 3, 2, 1, 0);
        lessThanQuery("long_not_null_btree", "idx_long_not_null_btree", (long)4, 3, 2, 1, 0);
        betweenQuery("long_not_null_btree", "idx_long_not_null_btree", (long)4, (long)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("long_not_null_btree", "idx_long_not_null_btree", (long)4, (long)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("long_not_null_btree", "idx_long_not_null_btree", (long)4, (long)6, 5, 6);
        greaterEqualAndLessThanQuery("long_not_null_btree", "idx_long_not_null_btree", (long)4, (long)6, 4, 5);
        greaterThanAndLessThanQuery("long_not_null_btree", "idx_long_not_null_btree", (long)4, (long)6, 5);

        equalQuery("long_null_btree", "idx_long_null_btree", (long)8, 8);
        greaterEqualQuery("long_null_btree", "idx_long_null_btree", (long)7, 7, 8, 9);
        greaterThanQuery("long_null_btree", "idx_long_null_btree", (long)6, 7, 8, 9);
        lessEqualQuery("long_null_btree", "idx_long_null_btree", (long)4, 4, 3, 2, 1, 0);
        lessThanQuery("long_null_btree", "idx_long_null_btree", (long)4, 3, 2, 1, 0);
        betweenQuery("long_null_btree", "idx_long_null_btree", (long)4, (long)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("long_null_btree", "idx_long_null_btree", (long)4, (long)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("long_null_btree", "idx_long_null_btree", (long)4, (long)6, 5, 6);
        greaterEqualAndLessThanQuery("long_null_btree", "idx_long_null_btree", (long)4, (long)6, 4, 5);
        greaterThanAndLessThanQuery("long_null_btree", "idx_long_null_btree", (long)4, (long)6, 5);
    }

    public void hashIndexScanLong() {
        equalQuery("long_not_null_hash", "idx_long_not_null_hash", (long)8, 8);
        greaterEqualQuery("long_not_null_hash", "none", (long)7, 7, 8, 9);
        greaterThanQuery("long_not_null_hash", "none", (long)6, 7, 8, 9);
        lessEqualQuery("long_not_null_hash", "none", (long)4, 4, 3, 2, 1, 0);
        lessThanQuery("long_not_null_hash", "none", (long)4, 3, 2, 1, 0);
        betweenQuery("long_not_null_hash", "none", (long)4, (long)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("long_not_null_hash", "none", (long)4, (long)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("long_not_null_hash", "none", (long)4, (long)6, 5, 6);
        greaterEqualAndLessThanQuery("long_not_null_hash", "none", (long)4, (long)6, 4, 5);
        greaterThanAndLessThanQuery("long_not_null_hash", "none", (long)4, (long)6, 5);

        equalQuery("long_null_hash", "idx_long_null_hash", (long)8, 8);
        greaterEqualQuery("long_null_hash", "none", (long)7, 7, 8, 9);
        greaterThanQuery("long_null_hash", "none", (long)6, 7, 8, 9);
        lessEqualQuery("long_null_hash", "none", (long)4, 4, 3, 2, 1, 0);
        lessThanQuery("long_null_hash", "none", (long)4, 3, 2, 1, 0);
        betweenQuery("long_null_hash", "none", (long)4, (long)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("long_null_hash", "none", (long)4, (long)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("long_null_hash", "none", (long)4, (long)6, 5, 6);
        greaterEqualAndLessThanQuery("long_null_hash", "none", (long)4, (long)6, 4, 5);
        greaterThanAndLessThanQuery("long_null_hash", "none", (long)4, (long)6, 5);
    }

    public void bothIndexScanLong() {
        equalQuery("long_not_null_both", "idx_long_not_null_both", (long)8, 8);
        greaterEqualQuery("long_not_null_both", "idx_long_not_null_both", (long)7, 7, 8, 9);
        greaterThanQuery("long_not_null_both", "idx_long_not_null_both", (long)6, 7, 8, 9);
        lessEqualQuery("long_not_null_both", "idx_long_not_null_both", (long)4, 4, 3, 2, 1, 0);
        lessThanQuery("long_not_null_both", "idx_long_not_null_both", (long)4, 3, 2, 1, 0);
        betweenQuery("long_not_null_both", "idx_long_not_null_both", (long)4, (long)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("long_not_null_both", "idx_long_not_null_both", (long)4, (long)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("long_not_null_both", "idx_long_not_null_both", (long)4, (long)6, 5, 6);
        greaterEqualAndLessThanQuery("long_not_null_both", "idx_long_not_null_both", (long)4, (long)6, 4, 5);
        greaterThanAndLessThanQuery("long_not_null_both", "idx_long_not_null_both", (long)4, (long)6, 5);

        equalQuery("long_null_both", "idx_long_null_both", (long)8, 8);
        greaterEqualQuery("long_null_both", "idx_long_null_both", (long)7, 7, 8, 9);
        greaterThanQuery("long_null_both", "idx_long_null_both", (long)6, 7, 8, 9);
        lessEqualQuery("long_null_both", "idx_long_null_both", (long)4, 4, 3, 2, 1, 0);
        lessThanQuery("long_null_both", "idx_long_null_both", (long)4, 3, 2, 1, 0);
        betweenQuery("long_null_both", "idx_long_null_both", (long)4, (long)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("long_null_both", "idx_long_null_both", (long)4, (long)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("long_null_both", "idx_long_null_both", (long)4, (long)6, 5, 6);
        greaterEqualAndLessThanQuery("long_null_both", "idx_long_null_both", (long)4, (long)6, 4, 5);
        greaterThanAndLessThanQuery("long_null_both", "idx_long_null_both", (long)4, (long)6, 5);
    }

    public void noneIndexScanLong() {
        equalQuery("long_not_null_none", "none", (long)8, 8);
        greaterEqualQuery("long_not_null_none", "none", (long)7, 7, 8, 9);
        greaterThanQuery("long_not_null_none", "none", (long)6, 7, 8, 9);
        lessEqualQuery("long_not_null_none", "none", (long)4, 4, 3, 2, 1, 0);
        lessThanQuery("long_not_null_none", "none", (long)4, 3, 2, 1, 0);
        betweenQuery("long_not_null_none", "none", (long)4, (long)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("long_not_null_none", "none", (long)4, (long)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("long_not_null_none", "none", (long)4, (long)6, 5, 6);
        greaterEqualAndLessThanQuery("long_not_null_none", "none", (long)4, (long)6, 4, 5);
        greaterThanAndLessThanQuery("long_not_null_none", "none", (long)4, (long)6, 5);

        equalQuery("long_null_none", "none", (long)8, 8);
        greaterEqualQuery("long_null_none", "none", (long)7, 7, 8, 9);
        greaterThanQuery("long_null_none", "none", (long)6, 7, 8, 9);
        lessEqualQuery("long_null_none", "none", (long)4, 4, 3, 2, 1, 0);
        lessThanQuery("long_null_none", "none", (long)4, 3, 2, 1, 0);
        betweenQuery("long_null_none", "none", (long)4, (long)6, 4, 5, 6);
        greaterEqualAndLessEqualQuery("long_null_none", "none", (long)4, (long)6, 4, 5, 6);
        greaterThanAndLessEqualQuery("long_null_none", "none", (long)4, (long)6, 5, 6);
        greaterEqualAndLessThanQuery("long_null_none", "none", (long)4, (long)6, 4, 5);
        greaterThanAndLessThanQuery("long_null_none", "none", (long)4, (long)6, 5);
    }

}
