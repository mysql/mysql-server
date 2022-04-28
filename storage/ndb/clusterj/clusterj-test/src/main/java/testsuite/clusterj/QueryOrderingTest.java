/*
Copyright (c) 2012, 2022, Oracle and/or its affiliates.

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

import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.Query.Ordering;

import java.util.Arrays;

import testsuite.clusterj.model.LongIntStringIndex;

/** Verify queries using ordering. If a query uses ordering, there must be an index
 * containing the ordering columns already defined in the database.
 * 
 * This test is based on AbstractQueryTest.
 */
public class QueryOrderingTest extends AbstractQueryTest {

    /*
drop table if exists longintstringix;
create table longintstringix (
 id int(11) not null,
 longix bigint(20) not null,
 stringix varchar(10) not null,
 intix int(11) not null,
 stringvalue varchar(10) default null,
 PRIMARY KEY (id),
 KEY idx_long_int_string (longix, intix, stringix)
) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

      */

    @Override
    public Class<?> getInstanceType() {
        return LongIntStringIndex.class;
    }

    /** Most query tests use the same number of instances (10).
     * But this test uses 25.
     */
    @Override
    protected int getNumberOfInstances() {
        return 25;
    }

    protected int PK_MODULUS = 3;
    protected long PRETTY_BIG_NUMBER = 1000000000000000L;

    @Override
    protected boolean getCleanupAfterTest() {
        return false;
    }

    public void testNegativeOrderingFieldsTooLong() {
        try {
            setOrdering(Ordering.ASCENDING, "longix", "intix", "stringix", "id");
            greaterEqualQuery("longix", "idx_long_int_string", 0L, 9);
            fail("Ordering fields too long should fail.");
        } catch (ClusterJUserException ex) {
            // good catch
        }
        failOnError();
    }

    public void testNegativeNoIndexMatchesOrderingFields() {
        try {
            setOrdering(Ordering.ASCENDING, "longix", "intix", "id");
            greaterEqualQuery("longix", "idx_long_int_string", 0L, 9);
            fail("Ordering field not in index should fail.");
        } catch (ClusterJUserException ex) {
            // good catch
        }
        failOnError();
    }

    public void testNegativeOrderingFieldsNotInPosition() {
        try {
            setOrdering(Ordering.ASCENDING, "longix", "stringix", "intix");
            greaterEqualQuery("longix", "idx_long_int_string", 0L, 9);
            fail("Ordering field in wrong position in index should fail.");
        } catch (ClusterJUserException ex) {
            // good catch
        }
        failOnError();
    }

    public void testNegativeOrderingFieldsAreNotFields() {
        try {
            setOrdering(Ordering.ASCENDING, "poop");
            greaterEqualQuery("longix", "idx_long_int_string", 0L, 9);
            fail("Ordering field not a field should fail.");
        } catch (ClusterJUserException ex) {
            // good catch
            String message = ex.getMessage();
            errorIfNotEqual("Error message '" + message + "' does not contain the name of the failing field 'poop'.",
                    true, message.contains("poop"));
        }
        failOnError();
    }

    public void testNegativeMultipleOrderingFieldsAreNotFields() {
        try {
            setOrdering(Ordering.ASCENDING, "dupe", "poop");
            greaterEqualQuery("longix", "idx_long_int_string", 0L, 9);
            fail("Ordering field not a field should fail.");
        } catch (ClusterJUserException ex) {
            // good catch
            String message = ex.getMessage();
            errorIfNotEqual("Error message '" + message + "' does not contain the name of the failing fields 'poop' and 'dupe'.",
                    true, message.contains("poop") && message.contains("dupe"));
        }
        failOnError();
    }

    public void testNoWhereAscending() {
        setOrdering(Ordering.ASCENDING, "id");
        noWhereQuery("id", "PRIMARY", null, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24);
        failOnError();
    }

    public void testNoWhereDescending() {
        setOrdering(Ordering.DESCENDING, "id");
        noWhereQuery("id", "PRIMARY", null, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
        failOnError();
    }

    public void testPrimaryEqualAscending() {
        setOrdering(Ordering.ASCENDING, "longix", "intix", "stringix");
        equalQuery("id", "PRIMARY", 1, 1);
        failOnError();
    }

    public void testGreaterEqualAscending() {
        setOrdering(Ordering.ASCENDING, "longix", "intix", "stringix");
        greaterEqualQuery("longix", "idx_long_int_string", 2000000000000000L, 18, 19, 20, 21, 22, 23, 24);
        failOnError();
    }

    public void testGreaterEqualAscendingPartial() {
        setOrdering(Ordering.ASCENDING, "longix", "intix");
        greaterEqualQuery("longix", "idx_long_int_string", 2000000000000000L, 18, 19, 20, 21, 22, 23, 24);
        failOnError();
    }

    public void testInAndBetweenAscending() {
        setOrdering(Ordering.ASCENDING, "longix", "intix");
        inAndBetweenQuery("longix", new Object[] {1000000000000000L, 0L}, "intix", 1, 2, "idx_long_int_string", 12, 13, 14, 15, 16, 17, 3, 4, 5, 6, 7, 8);
        inAndBetweenQuery("longix", Arrays.asList(new Object[] {1000000000000000L, 0L}), "stringix", "1", "4", "idx_long_int_string", 10, 11, 13, 14, 16, 17, 1, 2, 4, 5, 7, 8);
        failOnError();        
    }

    public void testInAndBetweenDescending() {
        setOrdering(Ordering.DESCENDING, "longix", "intix", "stringix");
        inAndBetweenQuery("longix", new Object[] {1000000000000000L, 0L}, "intix", 1, 2, "idx_long_int_string", 17, 16, 15, 14, 13, 12, 8, 7, 6, 5, 4, 3);
        inAndBetweenQuery("longix", Arrays.asList(new Object[] {1000000000000000L, 0L}), "stringix", "1", "4", "idx_long_int_string", 17, 16, 14, 13, 11, 10, 8, 7, 5, 4, 2, 1);
        failOnError();        
    }

    public void testBetweenAndInAscending() {
        setOrdering(Ordering.ASCENDING, "longix", "intix");
        betweenAndInQuery("longix", 0L, 1000000000000000L, "intix", new Object[] {2, 0}, "idx_long_int_string", 0, 1, 2, 6, 7, 8, 9, 10, 11, 15, 16, 17);
        betweenAndInQuery("longix", 1000000000000000L, 2000000000000000L, "intix", Arrays.asList(new Object[] {2, 1}), "idx_long_int_string", 12, 13, 14, 15, 16, 17, 21, 22, 23, 24);
        failOnError();        
    }

    public void testBetweenAndInDescending() {
        setOrdering(Ordering.DESCENDING, "longix", "intix", "stringix");
        betweenAndInQuery("longix", 0L, 1000000000000000L, "intix", new Object[] {2, 0}, "idx_long_int_string", 17, 16, 15, 11, 10, 9, 8, 7, 6, 2, 1, 0);
        betweenAndInQuery("longix", 1000000000000000L, 2000000000000000L, "intix", Arrays.asList(new Object[] {2, 1}), "idx_long_int_string", 24, 23, 22, 21, 17, 16, 15, 14, 13, 12);
        failOnError();        
    }

    /** The strategy for instances is for the "instance number" to create 
     * the three keys, such that the value of the instance is:
     * pk1 * PK_MODULUS^2 + pk2 * PK_MODULUS + pk3
     * 
     */
    protected void createInstances(int number) {
        for (int i = 0; i < number; ++i) {
            LongIntStringIndex instance = createInstance(i);
            //System.out.println(toString(instance));
            instances.add(instance);
        }
    }

    /** Create an instance of LongIntStringPK.
     * @param index the index to use to generate data
     * @return the instance
     */
    protected LongIntStringIndex createInstance(int index) {
        LongIntStringIndex instance = session.newInstance(LongIntStringIndex.class);
        instance.setId(index);
        instance.setLongix(getPK1(index));
        instance.setIntix(getPK2(index));
        instance.setStringix(getPK3(index));
        instance.setStringvalue(getValue(index));
        return instance;
    }

    protected long getPK1(int index) {
        return PRETTY_BIG_NUMBER * ((index / PK_MODULUS / PK_MODULUS) % PK_MODULUS);
    }

    protected int getPK2(int index) {
        return ((index / PK_MODULUS) % PK_MODULUS);
    }

    protected String getPK3(int index) {
        return "" + (index % PK_MODULUS);
    }

    protected String getValue(int index) {
        return "Value " + index;
    }

    protected String toString(LongIntStringIndex instance) {
        StringBuffer result = new StringBuffer();
        result.append("LongIntStringIndex[");
        result.append(instance.getId());
        result.append("]: ");
        result.append(instance.getLongix());
        result.append(", ");
        result.append(instance.getIntix());
        result.append(", \"");
        result.append(instance.getStringix());
        result.append("\", \"");
        result.append(instance.getStringvalue());
        result.append("\".");
        return result.toString();
    }

}
