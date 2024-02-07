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

import testsuite.clusterj.model.DateAsUtilDateTypes;

import java.util.Date;
import testsuite.clusterj.model.IdBase;

public class QueryDateAsUtilDateTypesTest extends AbstractQueryTest {

    @Override
    public Class<DateAsUtilDateTypes> getInstanceType() {
        return DateAsUtilDateTypes.class;
    }

    @Override
    void createInstances(int number) {
        createAllDateTypesInstances(number);
    }

    @Override
    protected void consistencyCheck(IdBase instance) {
        DateAsUtilDateTypes dateType = (DateAsUtilDateTypes) instance;
        Date date = getDateFor(dateType.getId());
        String errorMessage = "Wrong values retrieved from ";
        errorIfNotEqual(errorMessage + "date_not_null_both", date, dateType.getDate_not_null_both());
        errorIfNotEqual(errorMessage + "date_not_null_btree", date, dateType.getDate_not_null_btree());
        errorIfNotEqual(errorMessage + "date_not_null_hash", date, dateType.getDate_not_null_hash());
        errorIfNotEqual(errorMessage + "date_not_null_none", date, dateType.getDate_not_null_none());
    }

    /** Test all single- and double-predicate queries using DateTypes.
drop table if exists datetypes;
create table datetypes (
 id int not null primary key,

 date_null_hash date,
 date_null_btree date,
 date_null_both date,
 date_null_none date,

 date_not_null_hash date,
 date_not_null_btree date,
 date_not_null_both date,
 date_not_null_none date

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_date_null_hash using hash on datetypes(date_null_hash);
create index idx_date_null_btree on datetypes(date_null_btree);
create unique index idx_date_null_both on datetypes(date_null_both);

create unique index idx_date_not_null_hash using hash on datetypes(date_not_null_hash);
create index idx_date_not_null_btree on datetypes(date_not_null_btree);
create unique index idx_date_not_null_both on datetypes(date_not_null_both);

     */

    public void test() {
        btreeIndexScanDate();
        hashIndexScanDate();
        bothIndexScanDate();
        noneIndexScanDate();
        failOnError();
    }

    public void btreeIndexScanDate() {
        equalQuery("date_not_null_btree", "idx_date_not_null_btree", getDateFor(8), 8);
        greaterEqualQuery("date_not_null_btree", "idx_date_not_null_btree", getDateFor(7), 7, 8, 9);
        greaterThanQuery("date_not_null_btree", "idx_date_not_null_btree", getDateFor(6), 7, 8, 9);
        lessEqualQuery("date_not_null_btree", "idx_date_not_null_btree", getDateFor(4), 4, 3, 2, 1, 0);
        lessThanQuery("date_not_null_btree", "idx_date_not_null_btree", getDateFor(4), 3, 2, 1, 0);
        betweenQuery("date_not_null_btree", "idx_date_not_null_btree", getDateFor(4), getDateFor(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("date_not_null_btree", "idx_date_not_null_btree", getDateFor(4), getDateFor(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("date_not_null_btree", "idx_date_not_null_btree", getDateFor(4), getDateFor(6), 5, 6);
        greaterEqualAndLessThanQuery("date_not_null_btree", "idx_date_not_null_btree", getDateFor(4), getDateFor(6), 4, 5);
        greaterThanAndLessThanQuery("date_not_null_btree", "idx_date_not_null_btree", getDateFor(4), getDateFor(6), 5);
    }

    public void hashIndexScanDate() {
        equalQuery("date_not_null_hash", "idx_date_not_null_hash", getDateFor(8), 8);
        greaterEqualQuery("date_not_null_hash", "none", getDateFor(7), 7, 8, 9);
        greaterThanQuery("date_not_null_hash", "none", getDateFor(6), 7, 8, 9);
        lessEqualQuery("date_not_null_hash", "none", getDateFor(4), 4, 3, 2, 1, 0);
        lessThanQuery("date_not_null_hash", "none", getDateFor(4), 3, 2, 1, 0);
        betweenQuery("date_not_null_hash", "none", getDateFor(4), getDateFor(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("date_not_null_hash", "none", getDateFor(4), getDateFor(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("date_not_null_hash", "none", getDateFor(4), getDateFor(6), 5, 6);
        greaterEqualAndLessThanQuery("date_not_null_hash", "none", getDateFor(4), getDateFor(6), 4, 5);
        greaterThanAndLessThanQuery("date_not_null_hash", "none", getDateFor(4), getDateFor(6), 5);
    }

    public void bothIndexScanDate() {
        equalQuery("date_not_null_both", "idx_date_not_null_both", getDateFor(8), 8);
        greaterEqualQuery("date_not_null_both", "idx_date_not_null_both", getDateFor(7), 7, 8, 9);
        greaterThanQuery("date_not_null_both", "idx_date_not_null_both", getDateFor(6), 7, 8, 9);
        lessEqualQuery("date_not_null_both", "idx_date_not_null_both", getDateFor(4), 4, 3, 2, 1, 0);
        lessThanQuery("date_not_null_both", "idx_date_not_null_both", getDateFor(4), 3, 2, 1, 0);
        betweenQuery("date_not_null_both", "idx_date_not_null_both", getDateFor(4), getDateFor(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("date_not_null_both", "idx_date_not_null_both", getDateFor(4), getDateFor(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("date_not_null_both", "idx_date_not_null_both", getDateFor(4), getDateFor(6), 5, 6);
        greaterEqualAndLessThanQuery("date_not_null_both", "idx_date_not_null_both", getDateFor(4), getDateFor(6), 4, 5);
        greaterThanAndLessThanQuery("date_not_null_both", "idx_date_not_null_both", getDateFor(4), getDateFor(6), 5);
    }

    public void noneIndexScanDate() {
        equalQuery("date_not_null_none", "none", getDateFor(8), 8);
        greaterEqualQuery("date_not_null_none", "none", getDateFor(7), 7, 8, 9);
        greaterThanQuery("date_not_null_none", "none", getDateFor(6), 7, 8, 9);
        lessEqualQuery("date_not_null_none", "none", getDateFor(4), 4, 3, 2, 1, 0);
        lessThanQuery("date_not_null_none", "none", getDateFor(4), 3, 2, 1, 0);
        betweenQuery("date_not_null_none", "none", getDateFor(4), getDateFor(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("date_not_null_none", "none", getDateFor(4), getDateFor(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("date_not_null_none", "none", getDateFor(4), getDateFor(6), 5, 6);
        greaterEqualAndLessThanQuery("date_not_null_none", "none", getDateFor(4), getDateFor(6), 4, 5);
        greaterThanAndLessThanQuery("date_not_null_none", "none", getDateFor(4), getDateFor(6), 5);
    }

    private void createAllDateTypesInstances(int number) {
        for (int i = 0; i < number; ++i) {
            DateAsUtilDateTypes instance = session.newInstance(DateAsUtilDateTypes.class);
            instance.setId(i);
            instance.setDate_not_null_hash(getDateFor(i));
            instance.setDate_not_null_btree(getDateFor(i));
            instance.setDate_not_null_both(getDateFor(i));
            instance.setDate_not_null_none(getDateFor(i));
            instances.add(instance);
        }
    }

    protected Date getDateFor(int i) {
        return new Date(getMillisFor(1980, 0, i + 1));
    }

    public static String toString(IdBase instance) {
        DateAsUtilDateTypes datetype = (DateAsUtilDateTypes)instance;
        StringBuffer buffer = new StringBuffer("DateTypes id: ");
        buffer.append(datetype.getId());
        buffer.append("; date_not_null_both: ");
        buffer.append(datetype.getDate_not_null_both().toString());
        buffer.append("; date_not_null_btree: ");
        buffer.append(datetype.getDate_not_null_btree().toString());
        buffer.append("; date_not_null_hash: ");
        buffer.append(datetype.getDate_not_null_hash().toString());
        buffer.append("; date_not_null_none: ");
        buffer.append(datetype.getDate_not_null_none().toString());
        return buffer.toString();
    }
}
