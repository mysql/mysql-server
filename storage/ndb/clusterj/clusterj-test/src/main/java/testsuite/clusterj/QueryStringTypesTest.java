/*
   Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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

import testsuite.clusterj.model.StringTypes;
import testsuite.clusterj.model.IdBase;

public class QueryStringTypesTest extends AbstractQueryTest {

    @Override
    public Class getInstanceType() {
        return StringTypes.class;
    }

    @Override
    void createInstances(int number) {
        createAllStringTypesInstances(number);
    }

    static String[] strings = new String[] {
        "Alabama",
        "Arkansas",
        "Delaware",
        "New Jersey",
        "New York",
        "Pennsylvania",
        "Rhode Island",
        "Texax",
        "Virginia",
        "Wyoming"
    };

    /** Schema
    *
   drop table if exists stringtypes;
   create table stringtypes (
    id int not null primary key,

    string_null_hash varchar(20),
    string_null_btree varchar(300),
    string_null_both varchar(20),
    string_null_none varchar(300),

    string_not_null_hash varchar(300),
    string_not_null_btree varchar(20),
    string_not_null_both varchar(300),
    string_not_null_none varchar(20),
    unique key idx_string_null_hash (string_null_hash) using hash,
    key idx_string_null_btree (string_null_btree),
    unique key idx_string_null_both (string_null_both),

    unique key idx_string_not_null_hash (string_not_null_hash) using hash,
    key idx_string_not_null_btree (string_not_null_btree),
    unique key idx_string_not_null_both (string_not_null_both)

   ) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

    */
    public void test() {
        btreeIndexScanString();
        hashIndexScanString();
        bothIndexScanString();
        noneIndexScanString();
        failOnError();
    }

    public void btreeIndexScanString() {
        equalQuery("string_not_null_btree", "idx_string_not_null_btree", getString(8), 8);
        greaterEqualQuery("string_not_null_btree", "idx_string_not_null_btree", getString(7), 7, 8, 9);
        greaterThanQuery("string_not_null_btree", "idx_string_not_null_btree", getString(6), 7, 8, 9);
        lessEqualQuery("string_not_null_btree", "idx_string_not_null_btree", getString(4), 4, 3, 2, 1, 0);
        lessThanQuery("string_not_null_btree", "idx_string_not_null_btree", getString(4), 3, 2, 1, 0);
        betweenQuery("string_not_null_btree", "idx_string_not_null_btree", getString(4), getString(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("string_not_null_btree", "idx_string_not_null_btree", getString(4), getString(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("string_not_null_btree", "idx_string_not_null_btree", getString(4), getString(6), 5, 6);
        greaterEqualAndLessThanQuery("string_not_null_btree", "idx_string_not_null_btree", getString(4), getString(6), 4, 5);
        greaterThanAndLessThanQuery("string_not_null_btree", "idx_string_not_null_btree", getString(4), getString(6), 5);

        equalQuery("string_null_btree", "idx_string_null_btree", getString(8), 8);
        greaterEqualQuery("string_null_btree", "idx_string_null_btree", getString(7), 7, 8, 9);
        greaterThanQuery("string_null_btree", "idx_string_null_btree", getString(6), 7, 8, 9);
        lessEqualQuery("string_null_btree", "idx_string_null_btree", getString(4), 4, 3, 2, 1, 0);
        lessThanQuery("string_null_btree", "idx_string_null_btree", getString(4), 3, 2, 1, 0);
        betweenQuery("string_null_btree", "idx_string_null_btree", getString(4), getString(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("string_null_btree", "idx_string_null_btree", getString(4), getString(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("string_null_btree", "idx_string_null_btree", getString(4), getString(6), 5, 6);
        greaterEqualAndLessThanQuery("string_null_btree", "idx_string_null_btree", getString(4), getString(6), 4, 5);
        greaterThanAndLessThanQuery("string_null_btree", "idx_string_null_btree", getString(4), getString(6), 5);
}

    public void hashIndexScanString() {
        equalQuery("string_not_null_hash", "idx_string_not_null_hash", getString(8), 8);
        greaterEqualQuery("string_not_null_hash", "none", getString(7), 7, 8, 9);
        greaterThanQuery("string_not_null_hash", "none", getString(6), 7, 8, 9);
        lessEqualQuery("string_not_null_hash", "none", getString(4), 4, 3, 2, 1, 0);
        lessThanQuery("string_not_null_hash", "none", getString(4), 3, 2, 1, 0);
        betweenQuery("string_not_null_hash", "none", getString(4), getString(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("string_not_null_hash", "none", getString(4), getString(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("string_not_null_hash", "none", getString(4), getString(6), 5, 6);
        greaterEqualAndLessThanQuery("string_not_null_hash", "none", getString(4), getString(6), 4, 5);
        greaterThanAndLessThanQuery("string_not_null_hash", "none", getString(4), getString(6), 5);

        equalQuery("string_null_hash", "idx_string_null_hash", getString(8), 8);
        greaterEqualQuery("string_null_hash", "none", getString(7), 7, 8, 9);
        greaterThanQuery("string_null_hash", "none", getString(6), 7, 8, 9);
        lessEqualQuery("string_null_hash", "none", getString(4), 4, 3, 2, 1, 0);
        lessThanQuery("string_null_hash", "none", getString(4), 3, 2, 1, 0);
        betweenQuery("string_null_hash", "none", getString(4), getString(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("string_null_hash", "none", getString(4), getString(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("string_null_hash", "none", getString(4), getString(6), 5, 6);
        greaterEqualAndLessThanQuery("string_null_hash", "none", getString(4), getString(6), 4, 5);
        greaterThanAndLessThanQuery("string_null_hash", "none", getString(4), getString(6), 5);

    }

    public void bothIndexScanString() {
        equalQuery("string_not_null_both", "idx_string_not_null_both", getString(8), 8);
        greaterEqualQuery("string_not_null_both", "idx_string_not_null_both", getString(7), 7, 8, 9);
        greaterThanQuery("string_not_null_both", "idx_string_not_null_both", getString(6), 7, 8, 9);
        lessEqualQuery("string_not_null_both", "idx_string_not_null_both", getString(4), 4, 3, 2, 1, 0);
        lessThanQuery("string_not_null_both", "idx_string_not_null_both", getString(4), 3, 2, 1, 0);
        betweenQuery("string_not_null_both", "idx_string_not_null_both", getString(4), getString(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("string_not_null_both", "idx_string_not_null_both", getString(4), getString(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("string_not_null_both", "idx_string_not_null_both", getString(4), getString(6), 5, 6);
        greaterEqualAndLessThanQuery("string_not_null_both", "idx_string_not_null_both", getString(4), getString(6), 4, 5);
        greaterThanAndLessThanQuery("string_not_null_both", "idx_string_not_null_both", getString(4), getString(6), 5);

        equalQuery("string_null_both", "idx_string_null_both", getString(8), 8);
        greaterEqualQuery("string_null_both", "idx_string_null_both", getString(7), 7, 8, 9);
        greaterThanQuery("string_null_both", "idx_string_null_both", getString(6), 7, 8, 9);
        lessEqualQuery("string_null_both", "idx_string_null_both", getString(4), 4, 3, 2, 1, 0);
        lessThanQuery("string_null_both", "idx_string_null_both", getString(4), 3, 2, 1, 0);
        betweenQuery("string_null_both", "idx_string_null_both", getString(4), getString(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("string_null_both", "idx_string_null_both", getString(4), getString(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("string_null_both", "idx_string_null_both", getString(4), getString(6), 5, 6);
        greaterEqualAndLessThanQuery("string_null_both", "idx_string_null_both", getString(4), getString(6), 4, 5);
        greaterThanAndLessThanQuery("string_null_both", "idx_string_null_both", getString(4), getString(6), 5);

    }

    public void noneIndexScanString() {
        equalQuery("string_not_null_none", "none", getString(8), 8);
        greaterEqualQuery("string_not_null_none", "none", getString(7), 7, 8, 9);
        greaterThanQuery("string_not_null_none", "none", getString(6), 7, 8, 9);
        lessEqualQuery("string_not_null_none", "none", getString(4), 4, 3, 2, 1, 0);
        lessThanQuery("string_not_null_none", "none", getString(4), 3, 2, 1, 0);
        betweenQuery("string_not_null_none", "none", getString(4), getString(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("string_not_null_none", "none", getString(4), getString(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("string_not_null_none", "none", getString(4), getString(6), 5, 6);
        greaterEqualAndLessThanQuery("string_not_null_none", "none", getString(4), getString(6), 4, 5);
        greaterThanAndLessThanQuery("string_not_null_none", "none", getString(4), getString(6), 5);

        equalQuery("string_null_none", "none", getString(8), 8);
        greaterEqualQuery("string_null_none", "none", getString(7), 7, 8, 9);
        greaterThanQuery("string_null_none", "none", getString(6), 7, 8, 9);
        lessEqualQuery("string_null_none", "none", getString(4), 4, 3, 2, 1, 0);
        lessThanQuery("string_null_none", "none", getString(4), 3, 2, 1, 0);
        betweenQuery("string_null_none", "none", getString(4), getString(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("string_null_none", "none", getString(4), getString(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("string_null_none", "none", getString(4), getString(6), 5, 6);
        greaterEqualAndLessThanQuery("string_null_none", "none", getString(4), getString(6), 4, 5);
        greaterThanAndLessThanQuery("string_null_none", "none", getString(4), getString(6), 5);

    }

    private void createAllStringTypesInstances(int number) {
        for (int i = 0; i < number; ++i) {
            StringTypes instance = session.newInstance(StringTypes.class);
            instance.setId(i);
            instance.setString_not_null_hash(getString(i));
            instance.setString_not_null_btree(getString(i));
            instance.setString_not_null_both(getString(i));
            instance.setString_not_null_none(getString(i));
            instance.setString_null_hash(getString(i));
            instance.setString_null_btree(getString(i));
            instance.setString_null_both(getString(i));
            instance.setString_null_none(getString(i));
            instances.add(instance);
        }
    }

    protected String getString(int number) {
        return strings[number];
    }

    /** Print the results of a query for debugging.
     *
     * @param instance the instance to print
     */
    @Override
    protected void printResultInstance(IdBase instance) {
        if (instance instanceof StringTypes) {
            StringTypes stringType = (StringTypes)instance;
//            System.out.println(toString(stringType));
        }
    }

    public static String toString(IdBase idBase) {
        StringTypes instance = (StringTypes)idBase;
        StringBuffer buffer = new StringBuffer("StringTypes id: ");
        buffer.append(instance.getId());
        buffer.append("; string_not_null_both: ");
        buffer.append(instance.getString_not_null_both());
        buffer.append("; string_not_null_btree: ");
        buffer.append(instance.getString_not_null_btree());
        buffer.append("; string_not_null_hash: ");
        buffer.append(instance.getString_not_null_hash());
        buffer.append("; string_not_null_none: ");
        buffer.append(instance.getString_not_null_none());
        buffer.append("; string_null_both: ");
        buffer.append(instance.getString_null_both().toString());
        buffer.append("; string_null_btree: ");
        buffer.append(instance.getString_null_btree().toString());
        buffer.append("; string_null_hash: ");
        buffer.append(instance.getString_null_hash().toString());
        buffer.append("; string_null_none: ");
        buffer.append(instance.getString_null_none().toString());
        return buffer.toString();
    }
}
