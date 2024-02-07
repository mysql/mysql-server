/*
   Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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

import testsuite.clusterj.model.CharsetUtf8;
import testsuite.clusterj.model.IdBase;

public class QueryTextIndexScanTest extends AbstractQueryTest {

    protected String constant = "0000000000";
    protected String change   = "1111111111";
    protected String variable = "abcdefghij";

    /** Save the instances acquired by query. */
    protected CharsetUtf8[] instancesToUpdate = new CharsetUtf8[getNumberOfInstances()];

    @Override
    public Class<?> getInstanceType() {
        return CharsetUtf8.class;
    }

    @Override
    void createInstances(int number) {
        for (int i = 0; i < number; ++i) {
            CharsetUtf8 b = session.newInstance(CharsetUtf8.class);
            b.setId(i);
            b.setLargeColumn(getCharacters(i));
            instances.add(b);
        }
    }

    /** Test all single-predicate queries using CharsetUtf8.id, which has a
     * btree index defined. Text columns are fetched during scan.
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
    protected String getCharacters(int number) {
        return constant + variable.charAt(number);
    }

    /** Test the ability to update the instance acquired by query.
     * @param id the id of the instance to update
     */
    protected void testUpdate(int id) {
        String updated = change + variable.charAt(id);
        CharsetUtf8 before = instancesToUpdate[id];
        before.setLargeColumn(updated);
        session.updatePersistent(before);
        CharsetUtf8 after = session.find(CharsetUtf8.class, id);
        errorIfNotEqual("Mismatch on update " + id, updated, after.getLargeColumn());
    }

    /** Test the ability to delete the instance acquired by query.
     * @param id the id of the instance to update
     */
    protected void testDelete(int id) {
        CharsetUtf8 before = instancesToUpdate[id];
        session.deletePersistent(before);
        CharsetUtf8 after = session.find(CharsetUtf8.class, id);
        if (after != null) {
            error("Failed to delete " + id + ".");
        }
    }

    /** Verify the result instance. This method is called from the query methods for each result instance.
     * @param instance the instance
     */
    protected void printResultInstance(IdBase instance) {
        CharsetUtf8 b = ((CharsetUtf8)instance);
        int id = b.getId();
        instancesToUpdate[id] = b;
        errorIfNotEqual("Mismatch reading instance " + id, constant + variable.charAt(id), b.getLargeColumn());
    }

}
