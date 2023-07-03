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

import testsuite.clusterj.model.TimestampAsUtilDateTypes;
import java.util.Date;

import testsuite.clusterj.model.IdBase;

public class QueryTimestampAsUtilDateTypesTest extends AbstractQueryTest {

    @Override
    public Class<TimestampAsUtilDateTypes> getInstanceType() {
        return TimestampAsUtilDateTypes.class;
    }

    @Override
    void createInstances(int number) {
        createAllTimestampAsUtilDateTypesInstances(number);
    }

    @Override
    protected void consistencyCheck(IdBase instance) {
        TimestampAsUtilDateTypes dateType = (TimestampAsUtilDateTypes) instance;
        Date date = getUtilDateFor(dateType.getId());
        String errorMessage = "Wrong values retrieved from ";
        errorIfNotEqual(errorMessage + "timestamp_not_null_both", date, dateType.getTimestamp_not_null_both());
        errorIfNotEqual(errorMessage + "timestamp_not_null_btree", date, dateType.getTimestamp_not_null_btree());
        errorIfNotEqual(errorMessage + "timestamp_not_null_hash", date, dateType.getTimestamp_not_null_hash());
        errorIfNotEqual(errorMessage + "timestamp_not_null_none", date, dateType.getTimestamp_not_null_none());
    }

    /** Test all single- and double-predicate queries using TimestampTypes.
drop table if exists timestamptypes;
create table timestamptypes (
 id int not null primary key,

 timestamp_not_null_hash timestamp,
 timestamp_not_null_btree timestamp,
 timestamp_not_null_both timestamp,
 timestamp_not_null_none timestamp

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_timestamp_not_null_hash using hash on timestamptypes(timestamp_not_null_hash);
create index idx_timestamp_not_null_btree on timestamptypes(timestamp_not_null_btree);
create unique index idx_timestamp_not_null_both on timestamptypes(timestamp_not_null_both);

     */

    public void test() {
        btreeIndexScanTimestamp();
        hashIndexScanTimestamp();
        bothIndexScanTimestamp();
        noneIndexScanTimestamp();
        failOnError();
    }

    public void btreeIndexScanTimestamp() {
        equalQuery("timestamp_not_null_btree", "idx_timestamp_not_null_btree", getUtilDateFor(8), 8);
        greaterEqualQuery("timestamp_not_null_btree", "idx_timestamp_not_null_btree", getUtilDateFor(7), 7, 8, 9);
        greaterThanQuery("timestamp_not_null_btree", "idx_timestamp_not_null_btree", getUtilDateFor(6), 7, 8, 9);
        lessEqualQuery("timestamp_not_null_btree", "idx_timestamp_not_null_btree", getUtilDateFor(4), 4, 3, 2, 1, 0);
        lessThanQuery("timestamp_not_null_btree", "idx_timestamp_not_null_btree", getUtilDateFor(4), 3, 2, 1, 0);
        betweenQuery("timestamp_not_null_btree", "idx_timestamp_not_null_btree", getUtilDateFor(4), getUtilDateFor(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("timestamp_not_null_btree", "idx_timestamp_not_null_btree", getUtilDateFor(4), getUtilDateFor(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("timestamp_not_null_btree", "idx_timestamp_not_null_btree", getUtilDateFor(4), getUtilDateFor(6), 5, 6);
        greaterEqualAndLessThanQuery("timestamp_not_null_btree", "idx_timestamp_not_null_btree", getUtilDateFor(4), getUtilDateFor(6), 4, 5);
        greaterThanAndLessThanQuery("timestamp_not_null_btree", "idx_timestamp_not_null_btree", getUtilDateFor(4), getUtilDateFor(6), 5);
    }

    public void hashIndexScanTimestamp() {
        equalQuery("timestamp_not_null_hash", "idx_timestamp_not_null_hash", getUtilDateFor(8), 8);
        greaterEqualQuery("timestamp_not_null_hash", "none", getUtilDateFor(7), 7, 8, 9);
        greaterThanQuery("timestamp_not_null_hash", "none", getUtilDateFor(6), 7, 8, 9);
        lessEqualQuery("timestamp_not_null_hash", "none", getUtilDateFor(4), 4, 3, 2, 1, 0);
        lessThanQuery("timestamp_not_null_hash", "none", getUtilDateFor(4), 3, 2, 1, 0);
        betweenQuery("timestamp_not_null_hash", "none", getUtilDateFor(4), getUtilDateFor(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("timestamp_not_null_hash", "none", getUtilDateFor(4), getUtilDateFor(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("timestamp_not_null_hash", "none", getUtilDateFor(4), getUtilDateFor(6), 5, 6);
        greaterEqualAndLessThanQuery("timestamp_not_null_hash", "none", getUtilDateFor(4), getUtilDateFor(6), 4, 5);
        greaterThanAndLessThanQuery("timestamp_not_null_hash", "none", getUtilDateFor(4), getUtilDateFor(6), 5);
    }

    public void bothIndexScanTimestamp() {
        equalQuery("timestamp_not_null_both", "idx_timestamp_not_null_both", getUtilDateFor(8), 8);
        greaterEqualQuery("timestamp_not_null_both", "idx_timestamp_not_null_both", getUtilDateFor(7), 7, 8, 9);
        greaterThanQuery("timestamp_not_null_both", "idx_timestamp_not_null_both", getUtilDateFor(6), 7, 8, 9);
        lessEqualQuery("timestamp_not_null_both", "idx_timestamp_not_null_both", getUtilDateFor(4), 4, 3, 2, 1, 0);
        lessThanQuery("timestamp_not_null_both", "idx_timestamp_not_null_both", getUtilDateFor(4), 3, 2, 1, 0);
        betweenQuery("timestamp_not_null_both", "idx_timestamp_not_null_both", getUtilDateFor(4), getUtilDateFor(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("timestamp_not_null_both", "idx_timestamp_not_null_both", getUtilDateFor(4), getUtilDateFor(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("timestamp_not_null_both", "idx_timestamp_not_null_both", getUtilDateFor(4), getUtilDateFor(6), 5, 6);
        greaterEqualAndLessThanQuery("timestamp_not_null_both", "idx_timestamp_not_null_both", getUtilDateFor(4), getUtilDateFor(6), 4, 5);
        greaterThanAndLessThanQuery("timestamp_not_null_both", "idx_timestamp_not_null_both", getUtilDateFor(4), getUtilDateFor(6), 5);
    }

    public void noneIndexScanTimestamp() {
        equalQuery("timestamp_not_null_none", "none", getUtilDateFor(8), 8);
        greaterEqualQuery("timestamp_not_null_none", "none", getUtilDateFor(7), 7, 8, 9);
        greaterThanQuery("timestamp_not_null_none", "none", getUtilDateFor(6), 7, 8, 9);
        lessEqualQuery("timestamp_not_null_none", "none", getUtilDateFor(4), 4, 3, 2, 1, 0);
        lessThanQuery("timestamp_not_null_none", "none", getUtilDateFor(4), 3, 2, 1, 0);
        betweenQuery("timestamp_not_null_none", "none", getUtilDateFor(4), getUtilDateFor(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("timestamp_not_null_none", "none", getUtilDateFor(4), getUtilDateFor(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("timestamp_not_null_none", "none", getUtilDateFor(4), getUtilDateFor(6), 5, 6);
        greaterEqualAndLessThanQuery("timestamp_not_null_none", "none", getUtilDateFor(4), getUtilDateFor(6), 4, 5);
        greaterThanAndLessThanQuery("timestamp_not_null_none", "none", getUtilDateFor(4), getUtilDateFor(6), 5);
    }

    private void createAllTimestampAsUtilDateTypesInstances(int number) {
        for (int i = 0; i < number; ++i) {
            TimestampAsUtilDateTypes instance = session.newInstance(TimestampAsUtilDateTypes.class);
            instance.setId(i);
            instance.setTimestamp_not_null_hash(getUtilDateFor(i));
            instance.setTimestamp_not_null_btree(getUtilDateFor(i));
            instance.setTimestamp_not_null_both(getUtilDateFor(i));
            instance.setTimestamp_not_null_none(getUtilDateFor(i));
            instances.add(instance);
//            if (i%3 == 0) System.out.println(toString(instance));
        }
    }

    private Date getUtilDateFor(int i) {
        return new Date(getMillisFor(1980, 0, 1, 0, 0, i));
    }

    public static String toString(IdBase instance) {
        TimestampAsUtilDateTypes timetype = (TimestampAsUtilDateTypes)instance;
        StringBuffer buffer = new StringBuffer("TimestampTypes id: ");
        buffer.append(timetype.getId());
        buffer.append("; timestamp_not_null_both: ");
        buffer.append(timetype.getTimestamp_not_null_both().toString());
        buffer.append("; timestamp_not_null_btree: ");
        buffer.append(timetype.getTimestamp_not_null_btree().toString());
        buffer.append("; timestamp_not_null_hash: ");
        buffer.append(timetype.getTimestamp_not_null_hash().toString());
        buffer.append("; timestamp_not_null_none: ");
        buffer.append(timetype.getTimestamp_not_null_none().toString());
        return buffer.toString();
    }
}
