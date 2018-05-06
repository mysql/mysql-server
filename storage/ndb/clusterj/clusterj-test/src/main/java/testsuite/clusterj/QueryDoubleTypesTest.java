/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

import testsuite.clusterj.model.DoubleTypes;
import testsuite.clusterj.model.IdBase;

public class QueryDoubleTypesTest extends AbstractQueryTest {

    @Override
    public Class getInstanceType() {
        return DoubleTypes.class;
    }

    @Override
    void createInstances(int number) {
        createAllDoubleTypesInstances(number);
    }

    /** Test all single- and double-predicate queries using DoubleTypes.
drop table if exists doubletypes;
create table doubletypes (
 id int not null primary key,

 double_null_hash double(10,5),
 double_null_btree double(10,5),
 double_null_both double(10,5),
 double_null_none double(10,5)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_double_null_hash using hash on doubletypes(double_null_hash);
create index idx_double_null_btree on doubletypes(double_null_btree);
create unique index idx_double_null_both on doubletypes(double_null_both);

     */

/** Double types allow hash indexes to be defined but ndb-bindings
 * do not allow an equal lookup, so they are not used.
 * If hash indexes are supported in future, uncomment the test case methods.
 */
    public void test() {
        btreeIndexScanDouble();
        hashIndexScanDouble();
        bothIndexScanDouble();
        noneIndexScanDouble();
        failOnError();
    }

    public void btreeIndexScanDouble() {
        equalQuery("double_not_null_btree", "idx_double_not_null_btree", getDouble(8), 8);
        greaterEqualQuery("double_not_null_btree", "idx_double_not_null_btree", getDouble(7), 7, 8, 9);
        greaterThanQuery("double_not_null_btree", "idx_double_not_null_btree", getDouble(6), 7, 8, 9);
        lessEqualQuery("double_not_null_btree", "idx_double_not_null_btree", getDouble(4), 4, 3, 2, 1, 0);
        lessThanQuery("double_not_null_btree", "idx_double_not_null_btree", getDouble(4), 3, 2, 1, 0);
        betweenQuery("double_not_null_btree", "idx_double_not_null_btree", getDouble(4), getDouble(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("double_not_null_btree", "idx_double_not_null_btree", getDouble(4), getDouble(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("double_not_null_btree", "idx_double_not_null_btree", getDouble(4), getDouble(6), 5, 6);
        greaterEqualAndLessThanQuery("double_not_null_btree", "idx_double_not_null_btree", getDouble(4), getDouble(6), 4, 5);
        greaterThanAndLessThanQuery("double_not_null_btree", "idx_double_not_null_btree", getDouble(4), getDouble(6), 5);

        equalQuery("double_null_btree", "idx_double_null_btree", getDouble(8), 8);
        greaterEqualQuery("double_null_btree", "idx_double_null_btree", getDouble(7), 7, 8, 9);
        greaterThanQuery("double_null_btree", "idx_double_null_btree", getDouble(6), 7, 8, 9);
        lessEqualQuery("double_null_btree", "idx_double_null_btree", getDouble(4), 4, 3, 2, 1, 0);
        lessThanQuery("double_null_btree", "idx_double_null_btree", getDouble(4), 3, 2, 1, 0);
        betweenQuery("double_null_btree", "idx_double_null_btree", getDouble(4), getDouble(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("double_null_btree", "idx_double_null_btree", getDouble(4), getDouble(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("double_null_btree", "idx_double_null_btree", getDouble(4), getDouble(6), 5, 6);
        greaterEqualAndLessThanQuery("double_null_btree", "idx_double_null_btree", getDouble(4), getDouble(6), 4, 5);
        greaterThanAndLessThanQuery("double_null_btree", "idx_double_null_btree", getDouble(4), getDouble(6), 5);
}

    public void hashIndexScanDouble() {
        equalQuery("double_not_null_hash", "idx_double_not_null_hash", getDouble(8), 8);
        greaterEqualQuery("double_not_null_hash", "none", getDouble(7), 7, 8, 9);
        greaterThanQuery("double_not_null_hash", "none", getDouble(6), 7, 8, 9);
        lessEqualQuery("double_not_null_hash", "none", getDouble(4), 4, 3, 2, 1, 0);
        lessThanQuery("double_not_null_hash", "none", getDouble(4), 3, 2, 1, 0);
        betweenQuery("double_not_null_hash", "none", getDouble(4), getDouble(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("double_not_null_hash", "none", getDouble(4), getDouble(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("double_not_null_hash", "none", getDouble(4), getDouble(6), 5, 6);
        greaterEqualAndLessThanQuery("double_not_null_hash", "none", getDouble(4), getDouble(6), 4, 5);
        greaterThanAndLessThanQuery("double_not_null_hash", "none", getDouble(4), getDouble(6), 5);

        equalQuery("double_null_hash", "idx_double_null_hash", getDouble(8), 8);
        greaterEqualQuery("double_null_hash", "none", getDouble(7), 7, 8, 9);
        greaterThanQuery("double_null_hash", "none", getDouble(6), 7, 8, 9);
        lessEqualQuery("double_null_hash", "none", getDouble(4), 4, 3, 2, 1, 0);
        lessThanQuery("double_null_hash", "none", getDouble(4), 3, 2, 1, 0);
        betweenQuery("double_null_hash", "none", getDouble(4), getDouble(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("double_null_hash", "none", getDouble(4), getDouble(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("double_null_hash", "none", getDouble(4), getDouble(6), 5, 6);
        greaterEqualAndLessThanQuery("double_null_hash", "none", getDouble(4), getDouble(6), 4, 5);
        greaterThanAndLessThanQuery("double_null_hash", "none", getDouble(4), getDouble(6), 5);

    }

    public void bothIndexScanDouble() {
        equalQuery("double_not_null_both", "idx_double_not_null_both", getDouble(8), 8);
        greaterEqualQuery("double_not_null_both", "idx_double_not_null_both", getDouble(7), 7, 8, 9);
        greaterThanQuery("double_not_null_both", "idx_double_not_null_both", getDouble(6), 7, 8, 9);
        lessEqualQuery("double_not_null_both", "idx_double_not_null_both", getDouble(4), 4, 3, 2, 1, 0);
        lessThanQuery("double_not_null_both", "idx_double_not_null_both", getDouble(4), 3, 2, 1, 0);
        betweenQuery("double_not_null_both", "idx_double_not_null_both", getDouble(4), getDouble(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("double_not_null_both", "idx_double_not_null_both", getDouble(4), getDouble(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("double_not_null_both", "idx_double_not_null_both", getDouble(4), getDouble(6), 5, 6);
        greaterEqualAndLessThanQuery("double_not_null_both", "idx_double_not_null_both", getDouble(4), getDouble(6), 4, 5);
        greaterThanAndLessThanQuery("double_not_null_both", "idx_double_not_null_both", getDouble(4), getDouble(6), 5);

        equalQuery("double_null_both", "idx_double_null_both", getDouble(8), 8);
        greaterEqualQuery("double_null_both", "idx_double_null_both", getDouble(7), 7, 8, 9);
        greaterThanQuery("double_null_both", "idx_double_null_both", getDouble(6), 7, 8, 9);
        lessEqualQuery("double_null_both", "idx_double_null_both", getDouble(4), 4, 3, 2, 1, 0);
        lessThanQuery("double_null_both", "idx_double_null_both", getDouble(4), 3, 2, 1, 0);
        betweenQuery("double_null_both", "idx_double_null_both", getDouble(4), getDouble(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("double_null_both", "idx_double_null_both", getDouble(4), getDouble(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("double_null_both", "idx_double_null_both", getDouble(4), getDouble(6), 5, 6);
        greaterEqualAndLessThanQuery("double_null_both", "idx_double_null_both", getDouble(4), getDouble(6), 4, 5);
        greaterThanAndLessThanQuery("double_null_both", "idx_double_null_both", getDouble(4), getDouble(6), 5);

    }

    public void noneIndexScanDouble() {
        equalQuery("double_not_null_none", "none", getDouble(8), 8);
        greaterEqualQuery("double_not_null_none", "none", getDouble(7), 7, 8, 9);
        greaterThanQuery("double_not_null_none", "none", getDouble(6), 7, 8, 9);
        lessEqualQuery("double_not_null_none", "none", getDouble(4), 4, 3, 2, 1, 0);
        lessThanQuery("double_not_null_none", "none", getDouble(4), 3, 2, 1, 0);
        betweenQuery("double_not_null_none", "none", getDouble(4), getDouble(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("double_not_null_none", "none", getDouble(4), getDouble(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("double_not_null_none", "none", getDouble(4), getDouble(6), 5, 6);
        greaterEqualAndLessThanQuery("double_not_null_none", "none", getDouble(4), getDouble(6), 4, 5);
        greaterThanAndLessThanQuery("double_not_null_none", "none", getDouble(4), getDouble(6), 5);

        equalQuery("double_null_none", "none", getDouble(8), 8);
        greaterEqualQuery("double_null_none", "none", getDouble(7), 7, 8, 9);
        greaterThanQuery("double_null_none", "none", getDouble(6), 7, 8, 9);
        lessEqualQuery("double_null_none", "none", getDouble(4), 4, 3, 2, 1, 0);
        lessThanQuery("double_null_none", "none", getDouble(4), 3, 2, 1, 0);
        betweenQuery("double_null_none", "none", getDouble(4), getDouble(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("double_null_none", "none", getDouble(4), getDouble(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("double_null_none", "none", getDouble(4), getDouble(6), 5, 6);
        greaterEqualAndLessThanQuery("double_null_none", "none", getDouble(4), getDouble(6), 4, 5);
        greaterThanAndLessThanQuery("double_null_none", "none", getDouble(4), getDouble(6), 5);

    }

    private void createAllDoubleTypesInstances(int number) {
        for (int i = 0; i < number; ++i) {
            DoubleTypes instance = session.newInstance(DoubleTypes.class);
            instance.setId(i);
            instance.setDouble_not_null_hash(getDouble(i));
            instance.setDouble_not_null_btree(getDouble(i));
            instance.setDouble_not_null_both(getDouble(i));
            instance.setDouble_not_null_none(getDouble(i));
            instance.setDouble_null_hash(getDouble(i));
            instance.setDouble_null_btree(getDouble(i));
            instance.setDouble_null_both(getDouble(i));
            instance.setDouble_null_none(getDouble(i));
            instances.add(instance);
        }
    }

    protected Double getDouble(int number) {
        return Double.valueOf(0.00001D * number);
    }

    /** Print the results of a query for debugging.
     *
     * @param instance the instance to print
     */
    @Override
    protected void printResultInstance(IdBase instance) {
        if (instance instanceof DoubleTypes) {
            DoubleTypes doubleType = (DoubleTypes)instance;
//            System.out.println(toString(doubleType));
        }
    }

    public static String toString(IdBase idBase) {
        DoubleTypes instance = (DoubleTypes)idBase;
        StringBuffer buffer = new StringBuffer("DoubleTypes id: ");
        buffer.append(instance.getId());
        buffer.append("; double_not_null_both: ");
        buffer.append(instance.getDouble_not_null_both());
        buffer.append("; double_not_null_btree: ");
        buffer.append(instance.getDouble_not_null_btree());
        buffer.append("; double_not_null_hash: ");
        buffer.append(instance.getDouble_not_null_hash());
        buffer.append("; double_not_null_none: ");
        buffer.append(instance.getDouble_not_null_none());
        buffer.append("; double_null_both: ");
        buffer.append(instance.getDouble_null_both().toString());
        buffer.append("; double_null_btree: ");
        buffer.append(instance.getDouble_null_btree().toString());
        buffer.append("; double_null_hash: ");
        buffer.append(instance.getDouble_null_hash().toString());
        buffer.append("; double_null_none: ");
        buffer.append(instance.getDouble_null_none().toString());
        return buffer.toString();
    }
}
