/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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

import testsuite.clusterj.model.FloatTypes;
import testsuite.clusterj.model.IdBase;

public class QueryFloatTypesTest extends AbstractQueryTest {

    @Override
    public Class getInstanceType() {
        return FloatTypes.class;
    }

    @Override
    void createInstances(int number) {
        createAllFloatTypesInstances(number);
    }

    /** Test all single- and double-predicate queries using FloatTypes.
drop table if exists floattypes;
create table floattypes (
 id int not null primary key,

 float_null_hash float,
 float_null_btree float,
 float_null_both float,
 float_null_none float,

 float_not_null_hash float,
 float_not_null_btree float,
 float_not_null_both float,
 float_not_null_none float

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_float_null_hash using hash on floattypes(float_null_hash);
create index idx_float_null_btree on floattypes(float_null_btree);
create unique index idx_float_null_both on floattypes(float_null_both);

create unique index idx_float_not_null_hash using hash on floattypes(float_not_null_hash);
create index idx_float_not_null_btree on floattypes(float_not_null_btree);
create unique index idx_float_not_null_both on floattypes(float_not_null_both);

     */

/** Float types allow hash indexes to be defined but ndb-bindings
 * do not allow an equal lookup, so they are not used.
 * If hash indexes are supported in future, uncomment the test case methods.
 */
    public void test() {
        btreeIndexScanFloat();
        hashIndexScanFloat();
        bothIndexScanFloat();
        noneIndexScanFloat();
        failOnError();
    }

    public void btreeIndexScanFloat() {
        equalQuery("float_not_null_btree", "idx_float_not_null_btree", getFloat(8), 8);
        greaterEqualQuery("float_not_null_btree", "idx_float_not_null_btree", getFloat(7), 7, 8, 9);
        greaterThanQuery("float_not_null_btree", "idx_float_not_null_btree", getFloat(6), 7, 8, 9);
        lessEqualQuery("float_not_null_btree", "idx_float_not_null_btree", getFloat(4), 4, 3, 2, 1, 0);
        lessThanQuery("float_not_null_btree", "idx_float_not_null_btree", getFloat(4), 3, 2, 1, 0);
        betweenQuery("float_not_null_btree", "idx_float_not_null_btree", getFloat(4), getFloat(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("float_not_null_btree", "idx_float_not_null_btree", getFloat(4), getFloat(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("float_not_null_btree", "idx_float_not_null_btree", getFloat(4), getFloat(6), 5, 6);
        greaterEqualAndLessThanQuery("float_not_null_btree", "idx_float_not_null_btree", getFloat(4), getFloat(6), 4, 5);
        greaterThanAndLessThanQuery("float_not_null_btree", "idx_float_not_null_btree", getFloat(4), getFloat(6), 5);

        equalQuery("float_null_btree", "idx_float_null_btree", getFloat(8), 8);
        greaterEqualQuery("float_null_btree", "idx_float_null_btree", getFloat(7), 7, 8, 9);
        greaterThanQuery("float_null_btree", "idx_float_null_btree", getFloat(6), 7, 8, 9);
        lessEqualQuery("float_null_btree", "idx_float_null_btree", getFloat(4), 4, 3, 2, 1, 0);
        lessThanQuery("float_null_btree", "idx_float_null_btree", getFloat(4), 3, 2, 1, 0);
        betweenQuery("float_null_btree", "idx_float_null_btree", getFloat(4), getFloat(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("float_null_btree", "idx_float_null_btree", getFloat(4), getFloat(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("float_null_btree", "idx_float_null_btree", getFloat(4), getFloat(6), 5, 6);
        greaterEqualAndLessThanQuery("float_null_btree", "idx_float_null_btree", getFloat(4), getFloat(6), 4, 5);
        greaterThanAndLessThanQuery("float_null_btree", "idx_float_null_btree", getFloat(4), getFloat(6), 5);
}

    public void hashIndexScanFloat() {
        equalQuery("float_not_null_hash", "idx_float_not_null_hash", getFloat(8), 8);
        greaterEqualQuery("float_not_null_hash", "none", getFloat(7), 7, 8, 9);
        greaterThanQuery("float_not_null_hash", "none", getFloat(6), 7, 8, 9);
        lessEqualQuery("float_not_null_hash", "none", getFloat(4), 4, 3, 2, 1, 0);
        lessThanQuery("float_not_null_hash", "none", getFloat(4), 3, 2, 1, 0);
        betweenQuery("float_not_null_hash", "none", getFloat(4), getFloat(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("float_not_null_hash", "none", getFloat(4), getFloat(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("float_not_null_hash", "none", getFloat(4), getFloat(6), 5, 6);
        greaterEqualAndLessThanQuery("float_not_null_hash", "none", getFloat(4), getFloat(6), 4, 5);
        greaterThanAndLessThanQuery("float_not_null_hash", "none", getFloat(4), getFloat(6), 5);

        equalQuery("float_null_hash", "idx_float_null_hash", getFloat(8), 8);
        greaterEqualQuery("float_null_hash", "none", getFloat(7), 7, 8, 9);
        greaterThanQuery("float_null_hash", "none", getFloat(6), 7, 8, 9);
        lessEqualQuery("float_null_hash", "none", getFloat(4), 4, 3, 2, 1, 0);
        lessThanQuery("float_null_hash", "none", getFloat(4), 3, 2, 1, 0);
        betweenQuery("float_null_hash", "none", getFloat(4), getFloat(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("float_null_hash", "none", getFloat(4), getFloat(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("float_null_hash", "none", getFloat(4), getFloat(6), 5, 6);
        greaterEqualAndLessThanQuery("float_null_hash", "none", getFloat(4), getFloat(6), 4, 5);
        greaterThanAndLessThanQuery("float_null_hash", "none", getFloat(4), getFloat(6), 5);

    }

    public void bothIndexScanFloat() {
        equalQuery("float_not_null_both", "idx_float_not_null_both", getFloat(8), 8);
        greaterEqualQuery("float_not_null_both", "idx_float_not_null_both", getFloat(7), 7, 8, 9);
        greaterThanQuery("float_not_null_both", "idx_float_not_null_both", getFloat(6), 7, 8, 9);
        lessEqualQuery("float_not_null_both", "idx_float_not_null_both", getFloat(4), 4, 3, 2, 1, 0);
        lessThanQuery("float_not_null_both", "idx_float_not_null_both", getFloat(4), 3, 2, 1, 0);
        betweenQuery("float_not_null_both", "idx_float_not_null_both", getFloat(4), getFloat(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("float_not_null_both", "idx_float_not_null_both", getFloat(4), getFloat(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("float_not_null_both", "idx_float_not_null_both", getFloat(4), getFloat(6), 5, 6);
        greaterEqualAndLessThanQuery("float_not_null_both", "idx_float_not_null_both", getFloat(4), getFloat(6), 4, 5);
        greaterThanAndLessThanQuery("float_not_null_both", "idx_float_not_null_both", getFloat(4), getFloat(6), 5);

        equalQuery("float_null_both", "idx_float_null_both", getFloat(8), 8);
        greaterEqualQuery("float_null_both", "idx_float_null_both", getFloat(7), 7, 8, 9);
        greaterThanQuery("float_null_both", "idx_float_null_both", getFloat(6), 7, 8, 9);
        lessEqualQuery("float_null_both", "idx_float_null_both", getFloat(4), 4, 3, 2, 1, 0);
        lessThanQuery("float_null_both", "idx_float_null_both", getFloat(4), 3, 2, 1, 0);
        betweenQuery("float_null_both", "idx_float_null_both", getFloat(4), getFloat(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("float_null_both", "idx_float_null_both", getFloat(4), getFloat(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("float_null_both", "idx_float_null_both", getFloat(4), getFloat(6), 5, 6);
        greaterEqualAndLessThanQuery("float_null_both", "idx_float_null_both", getFloat(4), getFloat(6), 4, 5);
        greaterThanAndLessThanQuery("float_null_both", "idx_float_null_both", getFloat(4), getFloat(6), 5);

    }

    public void noneIndexScanFloat() {
        equalQuery("float_not_null_none", "none", getFloat(8), 8);
        greaterEqualQuery("float_not_null_none", "none", getFloat(7), 7, 8, 9);
        greaterThanQuery("float_not_null_none", "none", getFloat(6), 7, 8, 9);
        lessEqualQuery("float_not_null_none", "none", getFloat(4), 4, 3, 2, 1, 0);
        lessThanQuery("float_not_null_none", "none", getFloat(4), 3, 2, 1, 0);
        betweenQuery("float_not_null_none", "none", getFloat(4), getFloat(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("float_not_null_none", "none", getFloat(4), getFloat(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("float_not_null_none", "none", getFloat(4), getFloat(6), 5, 6);
        greaterEqualAndLessThanQuery("float_not_null_none", "none", getFloat(4), getFloat(6), 4, 5);
        greaterThanAndLessThanQuery("float_not_null_none", "none", getFloat(4), getFloat(6), 5);

        equalQuery("float_null_none", "none", getFloat(8), 8);
        greaterEqualQuery("float_null_none", "none", getFloat(7), 7, 8, 9);
        greaterThanQuery("float_null_none", "none", getFloat(6), 7, 8, 9);
        lessEqualQuery("float_null_none", "none", getFloat(4), 4, 3, 2, 1, 0);
        lessThanQuery("float_null_none", "none", getFloat(4), 3, 2, 1, 0);
        betweenQuery("float_null_none", "none", getFloat(4), getFloat(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("float_null_none", "none", getFloat(4), getFloat(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("float_null_none", "none", getFloat(4), getFloat(6), 5, 6);
        greaterEqualAndLessThanQuery("float_null_none", "none", getFloat(4), getFloat(6), 4, 5);
        greaterThanAndLessThanQuery("float_null_none", "none", getFloat(4), getFloat(6), 5);

    }

    private void createAllFloatTypesInstances(int number) {
        for (int i = 0; i < number; ++i) {
            FloatTypes instance = session.newInstance(FloatTypes.class);
            instance.setId(i);
            instance.setFloat_not_null_hash(getFloat(i));
            instance.setFloat_not_null_btree(getFloat(i));
            instance.setFloat_not_null_both(getFloat(i));
            instance.setFloat_not_null_none(getFloat(i));
            instance.setFloat_null_hash(getFloat(i));
            instance.setFloat_null_btree(getFloat(i));
            instance.setFloat_null_both(getFloat(i));
            instance.setFloat_null_none(getFloat(i));
            instances.add(instance);
        }
    }

    protected Float getFloat(int number) {
        return Float.valueOf(0.00001F * number);
    }

    /** Print the results of a query for debugging.
     *
     * @param instance the instance to print
     */
    @Override
    protected void printResultInstance(IdBase instance) {
        if (instance instanceof FloatTypes) {
            FloatTypes floatType = (FloatTypes)instance;
//            System.out.println(toString(floatType));
        }
    }

    public static String toString(IdBase idBase) {
        FloatTypes instance = (FloatTypes)idBase;
        StringBuffer buffer = new StringBuffer("FloatTypes id: ");
        buffer.append(instance.getId());
        buffer.append("; float_not_null_both: ");
        buffer.append(instance.getFloat_not_null_both());
        buffer.append("; float_not_null_btree: ");
        buffer.append(instance.getFloat_not_null_btree());
        buffer.append("; float_not_null_hash: ");
        buffer.append(instance.getFloat_not_null_hash());
        buffer.append("; float_not_null_none: ");
        buffer.append(instance.getFloat_not_null_none());
        buffer.append("; float_null_both: ");
        buffer.append(instance.getFloat_null_both().toString());
        buffer.append("; float_null_btree: ");
        buffer.append(instance.getFloat_null_btree().toString());
        buffer.append("; float_null_hash: ");
        buffer.append(instance.getFloat_null_hash().toString());
        buffer.append("; float_null_none: ");
        buffer.append(instance.getFloat_null_none().toString());
        return buffer.toString();
    }
}
