/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package testsuite.clusterj;

import testsuite.clusterj.model.ByteArrayTypes;
import testsuite.clusterj.model.IdBase;

/** Derived from QueryStringTypesTest.
 */
public class QueryLikeByteArrayTypesTest extends AbstractQueryTest {

    @Override
    public Class<?> getInstanceType() {
        return ByteArrayTypes.class;
    }

    @Override
    void createInstances(int number) {
        createAllByteArrayTypesInstances(number);
    }

    static byte[][] bytes = new byte[][] {
        new byte[] {'a', 'a', 'a', 'a'},
        new byte[] {'a', 'a', 'a', 'b'},
        new byte[] {'a', 'a', 'b', 'a'},
        new byte[] {'a', 'a', 'b', 'b'},
        new byte[] {'a', 'b', 'a', 'a'},
        new byte[] {'a', 'b', 'a', 'b'},
        new byte[] {'a', 'b', 'b', 'a'},
        new byte[] {'a', 'b', 'b', 'b'},
        new byte[] {'b', 'a', 'a', 'a'},
        new byte[] {'b', 'a', 'a', 'b'},
    };

    /** Schema
    *
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
        btreeIndexScanString();
        hashIndexScanString();
        bothIndexScanString();
        noneIndexScanString();
        failOnError();
    }

    public void btreeIndexScanString() {
        likeQuery("bytes_null_btree", "none", new byte[] {(byte)'%'}, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        likeQuery("bytes_null_btree", "none", new byte[] {(byte)'_'});
        likeQuery("bytes_null_btree", "none", new byte[] {(byte)'_', (byte)'_', (byte)'_', (byte)'_'}, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        likeQuery("bytes_null_btree", "none", new byte[] {(byte)'_', (byte)'a', (byte)'a', (byte)'a'}, 0, 8);
        likeQuery("bytes_null_btree", "none", new byte[] {(byte)'a', (byte)'b', (byte)'a', (byte)'a'}, 4);
        likeQuery("bytes_null_btree", "none", new byte[] {(byte)'a', (byte)'b', (byte)'a', (byte)'%'}, 4, 5);
        likeQuery("bytes_null_btree", "none", new byte[] {(byte)'a', (byte)'b', (byte)'%'}, 4, 5, 6, 7);
        greaterEqualAndLikeQuery("bytes_null_btree", "idx_bytes_null_btree", getBytes(4), new byte[] {(byte)'%'}, 4, 5, 6, 7, 8, 9);
        greaterEqualAndLikeQuery("bytes_null_btree", "idx_bytes_null_btree", getBytes(4), new byte[] {(byte)'a', (byte)'%', (byte)'b'}, 5, 7);
        greaterThanAndLikeQuery("bytes_null_btree", "idx_bytes_null_btree", getBytes(4), new byte[] {(byte)'%', (byte)'b', (byte)'b', (byte)'%'}, 6, 7);
    }

    public void hashIndexScanString() {
        greaterEqualAndLikeQuery("bytes_null_hash", "none", getBytes(3), new byte[] {(byte)'%', (byte)'b', (byte)'b', (byte)'%'}, 3, 6, 7);
        greaterThanAndLikeQuery("bytes_null_hash", "none", getBytes(3), new byte[] {(byte)'%', (byte)'b', (byte)'b', (byte)'%'}, 6, 7);
    }

    public void bothIndexScanString() {
        greaterEqualAndLikeQuery("bytes_null_both", "idx_bytes_null_both", getBytes(3), new byte[] {(byte)'%', (byte)'b', (byte)'b', (byte)'%'}, 3, 6, 7);
        greaterThanAndLikeQuery("bytes_null_both", "idx_bytes_null_both", getBytes(3), new byte[] {(byte)'%', (byte)'b', (byte)'b', (byte)'%'}, 6, 7);
    }

    public void noneIndexScanString() {
        greaterEqualAndLikeQuery("bytes_null_none", "none", getBytes(3), new byte[] {(byte)'%', (byte)'b', (byte)'b', (byte)'%'}, 3, 6, 7);
        greaterThanAndLikeQuery("bytes_null_none", "none", getBytes(3), new byte[] {(byte)'%', (byte)'b', (byte)'b', (byte)'%'}, 6, 7);
    }

    private void createAllByteArrayTypesInstances(int number) {
        for (int i = 0; i < number; ++i) {
            ByteArrayTypes instance = session.newInstance(ByteArrayTypes.class);
            instance.setId(i);
            instance.setBytes_null_hash(getBytes(i));
            instance.setBytes_null_btree(getBytes(i));
            instance.setBytes_null_both(getBytes(i));
            instance.setBytes_null_none(getBytes(i));
            instances.add(instance);
        }
    }

    protected byte[] getBytes(int number) {
        return bytes[number];
    }

    /** Print the results of a query for debugging.
     *
     * @param instance the instance to print
     */
    @Override
    protected void printResultInstance(IdBase instance) {
        if (instance instanceof ByteArrayTypes) {
            @SuppressWarnings("unused")
            ByteArrayTypes stringType = (ByteArrayTypes)instance;
//            System.out.println(toString(stringType));
        }
    }

}
