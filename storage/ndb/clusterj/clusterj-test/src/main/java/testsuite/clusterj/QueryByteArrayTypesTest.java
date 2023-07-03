/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.
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

import testsuite.clusterj.model.ByteArrayTypes;
import testsuite.clusterj.model.IdBase;

public class QueryByteArrayTypesTest extends AbstractQueryTest {

    @Override
    public Class getInstanceType() {
        return ByteArrayTypes.class;
    }

    @Override
    void createInstances(int number) {
        createAllByteArrayTypesInstances(number);
    }

    /** Test all single- and double-predicate queries using ByteArrayTypes.

drop table if exists bytestype;

create table bytestype (
 id int not null primary key,

 bytes_null_hash varbinary(8),
 bytes_null_btree varbinary(8),
 bytes_null_both varbinary(8),
 bytes_null_none varbinary(8),

key idx_bytes_null_btree (bytes_null_btree),
unique key idx_bytes_null_both (bytes_null_both),
unique key idx_bytes_null_hash (bytes_null_hash) using hash

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;
     */

    public void test() {
        btreeIndexScanByteArray();
        hashIndexScanByteArray();
        bothIndexScanByteArray();
        noneIndexScanByteArray();
        failOnError();
    }

    public void btreeIndexScanByteArray() {
        equalQuery("bytes_null_btree", "idx_bytes_null_btree", getByteArray(8), 8);
        greaterEqualQuery("bytes_null_btree", "idx_bytes_null_btree", getByteArray(7), 7, 8, 9);
        greaterThanQuery("bytes_null_btree", "idx_bytes_null_btree", getByteArray(6), 7, 8, 9);
        lessEqualQuery("bytes_null_btree", "idx_bytes_null_btree", getByteArray(4), 4, 3, 2, 1, 0);
        lessThanQuery("bytes_null_btree", "idx_bytes_null_btree", getByteArray(4), 3, 2, 1, 0);
        betweenQuery("bytes_null_btree", "idx_bytes_null_btree", getByteArray(4), getByteArray(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("bytes_null_btree", "idx_bytes_null_btree", getByteArray(4), getByteArray(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("bytes_null_btree", "idx_bytes_null_btree", getByteArray(4), getByteArray(6), 5, 6);
        greaterEqualAndLessThanQuery("bytes_null_btree", "idx_bytes_null_btree", getByteArray(4), getByteArray(6), 4, 5);
        greaterThanAndLessThanQuery("bytes_null_btree", "idx_bytes_null_btree", getByteArray(4), getByteArray(6), 5);
    }

    public void hashIndexScanByteArray() {
        equalQuery("bytes_null_hash", "idx_bytes_null_hash", getByteArray(8), 8);
        greaterEqualQuery("bytes_null_hash", "none", getByteArray(7), 7, 8, 9);
        greaterThanQuery("bytes_null_hash", "none", getByteArray(6), 7, 8, 9);
        lessEqualQuery("bytes_null_hash", "none", getByteArray(4), 4, 3, 2, 1, 0);
        lessThanQuery("bytes_null_hash", "none", getByteArray(4), 3, 2, 1, 0);
        betweenQuery("bytes_null_hash", "none", getByteArray(4), getByteArray(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("bytes_null_hash", "none", getByteArray(4), getByteArray(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("bytes_null_hash", "none", getByteArray(4), getByteArray(6), 5, 6);
        greaterEqualAndLessThanQuery("bytes_null_hash", "none", getByteArray(4), getByteArray(6), 4, 5);
        greaterThanAndLessThanQuery("bytes_null_hash", "none", getByteArray(4), getByteArray(6), 5);
    }

    public void bothIndexScanByteArray() {
        equalQuery("bytes_null_both", "idx_bytes_null_both", getByteArray(8), 8);
        greaterEqualQuery("bytes_null_both", "idx_bytes_null_both", getByteArray(7), 7, 8, 9);
        greaterThanQuery("bytes_null_both", "idx_bytes_null_both", getByteArray(6), 7, 8, 9);
        lessEqualQuery("bytes_null_both", "idx_bytes_null_both", getByteArray(4), 4, 3, 2, 1, 0);
        lessThanQuery("bytes_null_both", "idx_bytes_null_both", getByteArray(4), 3, 2, 1, 0);
        betweenQuery("bytes_null_both", "idx_bytes_null_both", getByteArray(4), getByteArray(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("bytes_null_both", "idx_bytes_null_both", getByteArray(4), getByteArray(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("bytes_null_both", "idx_bytes_null_both", getByteArray(4), getByteArray(6), 5, 6);
        greaterEqualAndLessThanQuery("bytes_null_both", "idx_bytes_null_both", getByteArray(4), getByteArray(6), 4, 5);
        greaterThanAndLessThanQuery("bytes_null_both", "idx_bytes_null_both", getByteArray(4), getByteArray(6), 5);
    }

    public void noneIndexScanByteArray() {
        equalQuery("bytes_null_none", "none", getByteArray(8), 8);
        greaterEqualQuery("bytes_null_none", "none", getByteArray(7), 7, 8, 9);
        greaterThanQuery("bytes_null_none", "none", getByteArray(6), 7, 8, 9);
        lessEqualQuery("bytes_null_none", "none", getByteArray(4), 4, 3, 2, 1, 0);
        lessThanQuery("bytes_null_none", "none", getByteArray(4), 3, 2, 1, 0);
        betweenQuery("bytes_null_none", "none", getByteArray(4), getByteArray(6), 4, 5, 6);
        greaterEqualAndLessEqualQuery("bytes_null_none", "none", getByteArray(4), getByteArray(6), 4, 5, 6);
        greaterThanAndLessEqualQuery("bytes_null_none", "none", getByteArray(4), getByteArray(6), 5, 6);
        greaterEqualAndLessThanQuery("bytes_null_none", "none", getByteArray(4), getByteArray(6), 4, 5);
        greaterThanAndLessThanQuery("bytes_null_none", "none", getByteArray(4), getByteArray(6), 5);
    }


    private void createAllByteArrayTypesInstances(int number) {
        for (int i = 0; i < number; ++i) {
            ByteArrayTypes instance = session.newInstance(ByteArrayTypes.class);
            instance.setId(i);
            instance.setBytes_null_hash(getByteArray(i));
            instance.setBytes_null_btree(getByteArray(i));
            instance.setBytes_null_both(getByteArray(i));
            instance.setBytes_null_none(getByteArray(i));
            instances.add(instance);
        }
    }

    protected byte[] getByteArray(int number) {
        return new byte[]{0, 0, 0, 0, 0, 0, 0, (byte)number};
    }

    public static String toString(IdBase idBase) {
        ByteArrayTypes instance = (ByteArrayTypes)idBase;
        StringBuffer buffer = new StringBuffer("ByteArrayTypes id: ");
        buffer.append(instance.getId());
        buffer.append("; bytes_null_both: ");
        buffer.append(instance.getBytes_null_both().toString());
        buffer.append("; bytes_null_btree: ");
        buffer.append(instance.getBytes_null_btree().toString());
        buffer.append("; bytes_null_hash: ");
        buffer.append(instance.getBytes_null_hash().toString());
        buffer.append("; bytes_null_none: ");
        buffer.append(instance.getBytes_null_none().toString());
        return buffer.toString();
    }
}
