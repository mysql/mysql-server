/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.
   Use is subject to license terms.

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

package testsuite.clusterj;

import java.math.BigDecimal;
import java.math.BigInteger;

import testsuite.clusterj.model.BigIntegerTypes;
import testsuite.clusterj.model.IdBase;

public class QueryBigIntegerTypesTest extends AbstractQueryTest {

    @Override
    public Class getInstanceType() {
        return BigIntegerTypes.class;
    }

    @Override
    void createInstances(int number) {
        createAllBigIntegerTypesInstances(number);
    }

    /** Test all single- and double-predicate queries using BigIntegerTypes.
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

    /** These tests require implementation of
     * setBoundDecimal for btree index scans, and cmpDecimal for non-index
     * operations.
     */
    public void test() {
        btreeIndexScanDecimal();
        hashIndexScanDecimal();
        bothIndexScanDecimal();
        noneIndexScanDecimal();
        failOnError();
    }

    public void btreeIndexScanDecimal() {
        equalQuery("decimal_null_btree", "idx_decimal_null_btree", getBigInteger(8), 8);
        greaterEqualQuery("decimal_null_btree", "idx_decimal_null_btree", getBigInteger(7), 7, 8, 9);
        greaterThanQuery("decimal_null_btree", "idx_decimal_null_btree", getBigInteger(6), 7, 8, 9);
        lessEqualQuery("decimal_null_btree", "idx_decimal_null_btree", getBigInteger(4), 4, 3, 2, 1, 0);
        lessThanQuery("decimal_null_btree", "idx_decimal_null_btree", getBigInteger(4), 3, 2, 1, 0);
        betweenQuery("decimal_null_btree", "idx_decimal_null_btree", getBigInteger(4), getBigInteger(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("decimal_null_btree", "idx_decimal_null_btree", getBigInteger(4), getBigInteger(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("decimal_null_btree", "idx_decimal_null_btree", getBigInteger(4), getBigInteger(6), 5, 6);
        greaterEqualAndLessThanQuery("decimal_null_btree", "idx_decimal_null_btree", getBigInteger(4), getBigInteger(6), 4, 5);
        greaterThanAndLessThanQuery("decimal_null_btree", "idx_decimal_null_btree", getBigInteger(4), getBigInteger(6), 5);
    }

    public void hashIndexScanDecimal() {
        equalQuery("decimal_null_hash", "idx_decimal_null_hash", getBigInteger(8), 8);
        greaterEqualQuery("decimal_null_hash", "none", getBigInteger(7), 7, 8, 9);
        greaterThanQuery("decimal_null_hash", "none", getBigInteger(6), 7, 8, 9);
        lessEqualQuery("decimal_null_hash", "none", getBigInteger(4), 4, 3, 2, 1, 0);
        lessThanQuery("decimal_null_hash", "none", getBigInteger(4), 3, 2, 1, 0);
        betweenQuery("decimal_null_hash", "none", getBigInteger(4), getBigInteger(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("decimal_null_hash", "none", getBigInteger(4), getBigInteger(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("decimal_null_hash", "none", getBigInteger(4), getBigInteger(6), 5, 6);
        greaterEqualAndLessThanQuery("decimal_null_hash", "none", getBigInteger(4), getBigInteger(6), 4, 5);
        greaterThanAndLessThanQuery("decimal_null_hash", "none", getBigInteger(4), getBigInteger(6), 5);
    }

    public void bothIndexScanDecimal() {
        equalQuery("decimal_null_both", "idx_decimal_null_both", getBigInteger(8), 8);
        greaterEqualQuery("decimal_null_both", "idx_decimal_null_both", getBigInteger(7), 7, 8, 9);
        greaterThanQuery("decimal_null_both", "idx_decimal_null_both", getBigInteger(6), 7, 8, 9);
        lessEqualQuery("decimal_null_both", "idx_decimal_null_both", getBigInteger(4), 4, 3, 2, 1, 0);
        lessThanQuery("decimal_null_both", "idx_decimal_null_both", getBigInteger(4), 3, 2, 1, 0);
        betweenQuery("decimal_null_both", "idx_decimal_null_both", getBigInteger(4), getBigInteger(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("decimal_null_both", "idx_decimal_null_both", getBigInteger(4), getBigInteger(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("decimal_null_both", "idx_decimal_null_both", getBigInteger(4), getBigInteger(6), 5, 6);
        greaterEqualAndLessThanQuery("decimal_null_both", "idx_decimal_null_both", getBigInteger(4), getBigInteger(6), 4, 5);
        greaterThanAndLessThanQuery("decimal_null_both", "idx_decimal_null_both", getBigInteger(4), getBigInteger(6), 5);
    }

    public void noneIndexScanDecimal() {
        equalQuery("decimal_null_none", "none", getBigInteger(8), 8);
        greaterEqualQuery("decimal_null_none", "none", getBigInteger(7), 7, 8, 9);
        greaterThanQuery("decimal_null_none", "none", getBigInteger(6), 7, 8, 9);
        lessEqualQuery("decimal_null_none", "none", getBigInteger(4), 4, 3, 2, 1, 0);
        lessThanQuery("decimal_null_none", "none", getBigInteger(4), 3, 2, 1, 0);
        betweenQuery("decimal_null_none", "none", getBigInteger(4), getBigInteger(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("decimal_null_none", "none", getBigInteger(4), getBigInteger(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("decimal_null_none", "none", getBigInteger(4), getBigInteger(6), 5, 6);
        greaterEqualAndLessThanQuery("decimal_null_none", "none", getBigInteger(4), getBigInteger(6), 4, 5);
        greaterThanAndLessThanQuery("decimal_null_none", "none", getBigInteger(4), getBigInteger(6), 5);
    }


    private void createAllBigIntegerTypesInstances(int number) {
        for (int i = 0; i < number; ++i) {
            BigIntegerTypes instance = session.newInstance(BigIntegerTypes.class);
            instance.setId(i);
            instance.setDecimal_null_hash(getBigInteger(i));
            instance.setDecimal_null_btree(getBigInteger(i));
            instance.setDecimal_null_both(getBigInteger(i));
            instance.setDecimal_null_none(getBigInteger(i));
            instances.add(instance);
        }
    }

    /** Create a BigInteger value from an int value by multiplying by 10000.
     *
     * @param number the sequence number
     * @return the value corresponding to the number
     */
    protected BigInteger getBigInteger(int number) {
        return BigInteger.valueOf(number * 10000);
    }

    /** Print the results of a query for debugging.
     *
     * @param instance the instance to print
     */
    @Override
    protected void printResultInstance(IdBase instance) {
        if (instance instanceof BigIntegerTypes) {
            BigIntegerTypes bigIntegerTypes = (BigIntegerTypes)instance;
//            System.out.println(toString(bigIntegerTypes));
        }
    }

    public static String toString(IdBase idBase) {
        BigIntegerTypes instance = (BigIntegerTypes)idBase;
        StringBuffer buffer = new StringBuffer("BigIntegerTypes id: ");
        buffer.append(instance.getId());
        buffer.append("; decimal_null_both: ");
        buffer.append(instance.getDecimal_null_both().toString());
        buffer.append("; decimal_null_btree: ");
        buffer.append(instance.getDecimal_null_btree().toString());
        buffer.append("; decimal_null_hash: ");
        buffer.append(instance.getDecimal_null_hash().toString());
        buffer.append("; decimal_null_none: ");
        buffer.append(instance.getDecimal_null_none().toString());
        return buffer.toString();
    }
}
