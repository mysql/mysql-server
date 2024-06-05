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

import testsuite.clusterj.model.YearTypes;
import testsuite.clusterj.model.IdBase;

public class QueryYearTypesTest extends AbstractQueryTest {

    @Override
    public Class getInstanceType() {
        return YearTypes.class;
    }

    @Override
    void createInstances(int number) {
        createAllYearTypesInstances(number);
    }

    /** Test all single- and double-predicate queries using YearTypes.
drop table if exists yeartypes;
create table yeartypes (
 id int not null primary key,

 year_null_hash year,
 year_null_btree year,
 year_null_both year,
 year_null_none year,

 year_not_null_hash year,
 year_not_null_btree year,
 year_not_null_both year,
 year_not_null_none year

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_year_null_hash using hash on yeartypes(year_null_hash);
create index idx_year_null_btree on yeartypes(year_null_btree);
create unique index idx_year_null_both on yeartypes(year_null_both);

create unique index idx_year_not_null_hash using hash on yeartypes(year_not_null_hash);
create index idx_year_not_null_btree on yeartypes(year_not_null_btree);
create unique index idx_year_not_null_both on yeartypes(year_not_null_both);

     */

    public void test() {
        btreeIndexScanYear();
        hashIndexScanYear();
        bothIndexScanYear();
        noneIndexScanYear();
        failOnError();
    }

    public void btreeIndexScanYear() {
        equalQuery("year_not_null_btree", "idx_year_not_null_btree", getYear(8), 8);
        greaterEqualQuery("year_not_null_btree", "idx_year_not_null_btree", getYear(7), 7, 8, 9);
        greaterThanQuery("year_not_null_btree", "idx_year_not_null_btree", getYear(6), 7, 8, 9);
        lessEqualQuery("year_not_null_btree", "idx_year_not_null_btree", getYear(4), 4, 3, 2, 1, 0);
        lessThanQuery("year_not_null_btree", "idx_year_not_null_btree", getYear(4), 3, 2, 1, 0);
        betweenQuery("year_not_null_btree", "idx_year_not_null_btree", getYear(4), getYear(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("year_not_null_btree", "idx_year_not_null_btree", getYear(4), getYear(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("year_not_null_btree", "idx_year_not_null_btree", getYear(4), getYear(6), 5, 6);
        greaterEqualAndLessThanQuery("year_not_null_btree", "idx_year_not_null_btree", getYear(4), getYear(6), 4, 5);
        greaterThanAndLessThanQuery("year_not_null_btree", "idx_year_not_null_btree", getYear(4), getYear(6), 5);

        equalQuery("year_null_btree", "idx_year_null_btree", getYear(8), 8);
        greaterEqualQuery("year_null_btree", "idx_year_null_btree", getYear(7), 7, 8, 9);
        greaterThanQuery("year_null_btree", "idx_year_null_btree", getYear(6), 7, 8, 9);
        lessEqualQuery("year_null_btree", "idx_year_null_btree", getYear(4), 4, 3, 2, 1, 0);
        lessThanQuery("year_null_btree", "idx_year_null_btree", getYear(4), 3, 2, 1, 0);
        betweenQuery("year_null_btree", "idx_year_null_btree", getYear(4), getYear(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("year_null_btree", "idx_year_null_btree", getYear(4), getYear(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("year_null_btree", "idx_year_null_btree", getYear(4), getYear(6), 5, 6);
        greaterEqualAndLessThanQuery("year_null_btree", "idx_year_null_btree", getYear(4), getYear(6), 4, 5);
        greaterThanAndLessThanQuery("year_null_btree", "idx_year_null_btree", getYear(4), getYear(6), 5);
}

    public void hashIndexScanYear() {
        equalQuery("year_not_null_hash", "idx_year_not_null_hash", getYear(8), 8);
        greaterEqualQuery("year_not_null_hash", "none", getYear(7), 7, 8, 9);
        greaterThanQuery("year_not_null_hash", "none", getYear(6), 7, 8, 9);
        lessEqualQuery("year_not_null_hash", "none", getYear(4), 4, 3, 2, 1, 0);
        lessThanQuery("year_not_null_hash", "none", getYear(4), 3, 2, 1, 0);
        betweenQuery("year_not_null_hash", "none", getYear(4), getYear(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("year_not_null_hash", "none", getYear(4), getYear(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("year_not_null_hash", "none", getYear(4), getYear(6), 5, 6);
        greaterEqualAndLessThanQuery("year_not_null_hash", "none", getYear(4), getYear(6), 4, 5);
        greaterThanAndLessThanQuery("year_not_null_hash", "none", getYear(4), getYear(6), 5);

        equalQuery("year_null_hash", "idx_year_null_hash", getYear(8), 8);
        greaterEqualQuery("year_null_hash", "none", getYear(7), 7, 8, 9);
        greaterThanQuery("year_null_hash", "none", getYear(6), 7, 8, 9);
        lessEqualQuery("year_null_hash", "none", getYear(4), 4, 3, 2, 1, 0);
        lessThanQuery("year_null_hash", "none", getYear(4), 3, 2, 1, 0);
        betweenQuery("year_null_hash", "none", getYear(4), getYear(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("year_null_hash", "none", getYear(4), getYear(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("year_null_hash", "none", getYear(4), getYear(6), 5, 6);
        greaterEqualAndLessThanQuery("year_null_hash", "none", getYear(4), getYear(6), 4, 5);
        greaterThanAndLessThanQuery("year_null_hash", "none", getYear(4), getYear(6), 5);

    }

    public void bothIndexScanYear() {
        equalQuery("year_not_null_both", "idx_year_not_null_both", getYear(8), 8);
        greaterEqualQuery("year_not_null_both", "idx_year_not_null_both", getYear(7), 7, 8, 9);
        greaterThanQuery("year_not_null_both", "idx_year_not_null_both", getYear(6), 7, 8, 9);
        lessEqualQuery("year_not_null_both", "idx_year_not_null_both", getYear(4), 4, 3, 2, 1, 0);
        lessThanQuery("year_not_null_both", "idx_year_not_null_both", getYear(4), 3, 2, 1, 0);
        betweenQuery("year_not_null_both", "idx_year_not_null_both", getYear(4), getYear(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("year_not_null_both", "idx_year_not_null_both", getYear(4), getYear(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("year_not_null_both", "idx_year_not_null_both", getYear(4), getYear(6), 5, 6);
        greaterEqualAndLessThanQuery("year_not_null_both", "idx_year_not_null_both", getYear(4), getYear(6), 4, 5);
        greaterThanAndLessThanQuery("year_not_null_both", "idx_year_not_null_both", getYear(4), getYear(6), 5);

        equalQuery("year_null_both", "idx_year_null_both", getYear(8), 8);
        greaterEqualQuery("year_null_both", "idx_year_null_both", getYear(7), 7, 8, 9);
        greaterThanQuery("year_null_both", "idx_year_null_both", getYear(6), 7, 8, 9);
        lessEqualQuery("year_null_both", "idx_year_null_both", getYear(4), 4, 3, 2, 1, 0);
        lessThanQuery("year_null_both", "idx_year_null_both", getYear(4), 3, 2, 1, 0);
        betweenQuery("year_null_both", "idx_year_null_both", getYear(4), getYear(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("year_null_both", "idx_year_null_both", getYear(4), getYear(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("year_null_both", "idx_year_null_both", getYear(4), getYear(6), 5, 6);
        greaterEqualAndLessThanQuery("year_null_both", "idx_year_null_both", getYear(4), getYear(6), 4, 5);
        greaterThanAndLessThanQuery("year_null_both", "idx_year_null_both", getYear(4), getYear(6), 5);

    }

    public void noneIndexScanYear() {
        equalQuery("year_not_null_none", "none", getYear(8), 8);
        greaterEqualQuery("year_not_null_none", "none", getYear(7), 7, 8, 9);
        greaterThanQuery("year_not_null_none", "none", getYear(6), 7, 8, 9);
        lessEqualQuery("year_not_null_none", "none", getYear(4), 4, 3, 2, 1, 0);
        lessThanQuery("year_not_null_none", "none", getYear(4), 3, 2, 1, 0);
        betweenQuery("year_not_null_none", "none", getYear(4), getYear(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("year_not_null_none", "none", getYear(4), getYear(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("year_not_null_none", "none", getYear(4), getYear(6), 5, 6);
        greaterEqualAndLessThanQuery("year_not_null_none", "none", getYear(4), getYear(6), 4, 5);
        greaterThanAndLessThanQuery("year_not_null_none", "none", getYear(4), getYear(6), 5);

        equalQuery("year_null_none", "none", getYear(8), 8);
        greaterEqualQuery("year_null_none", "none", getYear(7), 7, 8, 9);
        greaterThanQuery("year_null_none", "none", getYear(6), 7, 8, 9);
        lessEqualQuery("year_null_none", "none", getYear(4), 4, 3, 2, 1, 0);
        lessThanQuery("year_null_none", "none", getYear(4), 3, 2, 1, 0);
        betweenQuery("year_null_none", "none", getYear(4), getYear(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("year_null_none", "none", getYear(4), getYear(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("year_null_none", "none", getYear(4), getYear(6), 5, 6);
        greaterEqualAndLessThanQuery("year_null_none", "none", getYear(4), getYear(6), 4, 5);
        greaterThanAndLessThanQuery("year_null_none", "none", getYear(4), getYear(6), 5);

    }

    private void createAllYearTypesInstances(int number) {
        for (int i = 0; i < number; ++i) {
            YearTypes instance = session.newInstance(YearTypes.class);
            instance.setId(i);
            instance.setYear_not_null_hash(getYear(i));
            instance.setYear_not_null_btree(getYear(i));
            instance.setYear_not_null_both(getYear(i));
            instance.setYear_not_null_none(getYear(i));
            instance.setYear_null_hash(getYear(i));
            instance.setYear_null_btree(getYear(i));
            instance.setYear_null_both(getYear(i));
            instance.setYear_null_none(getYear(i));
            instances.add(instance);
        }
    }

    protected Short getYear(int number) {
        return Short.valueOf((short)(2000 + number));
    }

    /** Print the results of a query for debugging.
     *
     * @param instance the instance to print
     */
    @Override
    protected void printResultInstance(IdBase instance) {
        if (instance instanceof YearTypes) {
            YearTypes yearType = (YearTypes)instance;
//            System.out.println(toString(yearType));
        }
    }

    public static String toString(IdBase idBase) {
        YearTypes instance = (YearTypes)idBase;
        StringBuffer buffer = new StringBuffer("YearTypes id: ");
        buffer.append(instance.getId());
        buffer.append("; year_not_null_both: ");
        buffer.append(instance.getYear_not_null_both());
        buffer.append("; year_not_null_btree: ");
        buffer.append(instance.getYear_not_null_btree());
        buffer.append("; year_not_null_hash: ");
        buffer.append(instance.getYear_not_null_hash());
        buffer.append("; year_not_null_none: ");
        buffer.append(instance.getYear_not_null_none());
        buffer.append("; year_null_both: ");
        buffer.append(instance.getYear_null_both().toString());
        buffer.append("; year_null_btree: ");
        buffer.append(instance.getYear_null_btree().toString());
        buffer.append("; year_null_hash: ");
        buffer.append(instance.getYear_null_hash().toString());
        buffer.append("; year_null_none: ");
        buffer.append(instance.getYear_null_none().toString());
        return buffer.toString();
    }
}
