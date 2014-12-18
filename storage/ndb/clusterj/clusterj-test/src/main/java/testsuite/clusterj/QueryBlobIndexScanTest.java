/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

import testsuite.clusterj.model.BlobTypes;
import testsuite.clusterj.model.IdBase;

public class QueryBlobIndexScanTest extends AbstractQueryTest {

    /** Save the instances acquired by query. */
    protected BlobTypes[] instancesToUpdate = new BlobTypes[getNumberOfInstances()];

    @Override
    public Class<?> getInstanceType() {
        return BlobTypes.class;
    }

    @Override
    void createInstances(int number) {
        for (int i = 0; i < number; ++i) {
            BlobTypes b = session.newInstance(BlobTypes.class);
            b.setId(i);
            b.setBlobbytes(getByteArray(i));
            instances.add(b);
        }
    }

    /** Test all single-predicate queries using BlobTypes.id, which has a
     * btree index defined. Blob columns are fetched during scan.
     */
    public void test() {
        equalQuery("id", "PRIMARY", 5, 5);
        testUpdate(5);
        greaterEqualQuery("id", "PRIMARY", 7, 7, 8, 9);
        greaterThanQuery("id", "PRIMARY", 6, 7, 8, 9);
        testUpdate(7);
        lessEqualQuery("id", "PRIMARY", 4, 4, 3, 2, 1, 0);
        lessThanQuery("id", "PRIMARY", 4, 3, 2, 1, 0);
        betweenQuery("id", "PRIMARY", 2, 4, 2, 3, 4);
        greaterEqualAndLessEqualQuery("id", "PRIMARY", 2, 4, 2, 3, 4);
        greaterThanAndLessEqualQuery("id", "PRIMARY", 2, 4, 3, 4);
        greaterEqualAndLessThanQuery("id", "PRIMARY", 2, 4, 2, 3);
        greaterThanAndLessThanQuery("id", "PRIMARY", 2, 4, 3);
        testDelete(5);
        testDelete(8);
        failOnError();
    }

    protected byte[] getByteArray(int number) {
        return new byte[]{0, 0, 0, 0, 0, 0, 0, (byte)number};
    }

    /** Test the ability to update the instance acquired by query.
     * @param id the id of the instance to update
     */
    protected void testUpdate(int id) {
        try {
        BlobTypes before = instancesToUpdate[id];
        byte[] updateBytes = before.getBlobbytes();
        updateBytes[0] = (byte)128;
        before.setBlobbytes(updateBytes);
        session.updatePersistent(before);
        BlobTypes after = session.find(BlobTypes.class, id);
        String message = compareBytes(updateBytes, after.getBlobbytes());
        if (message != null) {
            error("Mismatch on update " + id + ": " + message);
        }
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }

    /** Test the ability to delete the instance acquired by query.
     * @param id the id of the instance to update
     */
    protected void testDelete(int id) {
        BlobTypes before = instancesToUpdate[id];
        BlobTypes after = null;
        session.deletePersistent(before);
        after = session.find(BlobTypes.class, id);
        if (after != null) {
            error("Failed to delete " + id + ".");
        }
    }

    /** Verify the result instance. This method is called from the query methods for each result instance.
     * @param instance the instance
     */
    protected void printResultInstance(IdBase instance) {
        BlobTypes b = ((BlobTypes)instance);
        int id = b.getId();
        instancesToUpdate[id] = b;
        String message = compareBytes(getByteArray(id), b.getBlobbytes());
        if (message != null) {
            error("Mismatch reading instance " + id + ": " + message);
        }
    }

}
