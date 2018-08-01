/*
   Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

public class QueryBlobTypesTest extends AbstractQueryTest {

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
            b.setId_null_none(i);
            b.setId_null_hash(i);
            b.setBlobbytes(getByteArray(i));
            instances.add(b);
        }
    }

    /** Test all single-predicate queries using BlobTypes.id, which has a
     * btree index defined. Blob columns are fetched during scan and verified.
     * Also, update/delete of the blob values in the rows acquired by
     * the queries is tested.
     */
    public void testIndexScan() {
        testSinglePredicateQueries("id", "PRIMARY");
        testUpdate(5);
        testUpdate(7);
        testDelete(5);
        testDelete(8);
        failOnError();
    }

    /** Test the 'equal' query using BlobTypes.id_null_hash, which has a
     * unique index defined. Blob columns are fetched during scan and
     * verified.
     */
    public void testUniqueIndexScan() {
        // query for existing rows
        equalQuery("id_null_hash", "idx_id_null_hash", 5, 5);
        // query for non-existing rows
        equalQuery("id_null_hash", "idx_id_null_hash", 20);
        failOnError();
    }

    /** Test all single-predicate queries using BlobTypes.id_no_idx, which
     * has no index on it and thus triggering a table scan. Blob columns
     * are fetched during scan and verified.
     */
    public void testTableScan() {
        testSinglePredicateQueries("id_null_none", "none");
        failOnError();
    }

    protected byte[] getByteArray(int number) {
        return new byte[]{0, 0, 0, 0, 0, 0, 0, (byte)number};
    }

    /** Test all single-predicate queries using the given propertyName.
     * @param propertyName the column name to run the query on
     * @param expectedIndex the index that is expected to be used for the query
     */
    protected void testSinglePredicateQueries(String propertyName, String expectedIndex) {
        equalQuery(propertyName, expectedIndex, 5, 5);
        greaterEqualQuery(propertyName, expectedIndex, 7, 7, 8, 9);
        greaterThanQuery(propertyName, expectedIndex, 6, 7, 8, 9);
        lessEqualQuery(propertyName, expectedIndex, 4, 4, 3, 2, 1, 0);
        lessThanQuery(propertyName, expectedIndex, 4, 3, 2, 1, 0);
        betweenQuery(propertyName, expectedIndex, 2, 4, 2, 3, 4);
        greaterEqualAndLessEqualQuery(propertyName, expectedIndex, 2, 4, 2, 3, 4);
        greaterThanAndLessEqualQuery(propertyName, expectedIndex, 2, 4, 3, 4);
        greaterEqualAndLessThanQuery(propertyName, expectedIndex, 2, 4, 2, 3);
        greaterThanAndLessThanQuery(propertyName, expectedIndex, 2, 4, 3);

        // Run queries for records that doesn't exist
        equalQuery(propertyName, expectedIndex, 20);
        betweenQuery(propertyName, expectedIndex, 21, 40);
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
