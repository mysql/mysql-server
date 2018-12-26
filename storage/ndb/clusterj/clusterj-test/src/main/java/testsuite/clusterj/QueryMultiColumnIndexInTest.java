/*
Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

import java.util.Arrays;

import testsuite.clusterj.model.LongIntStringIndex;

/** Query for columns compared via IN.
 * Predicates using IN cannot use indexes, although indexes can 
 * be used for AND predicates where some of the predicates are IN
 * predicates.
 * This test is based on AbstractQueryTest.
 */
public class QueryMultiColumnIndexInTest extends AbstractQueryTest {

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

    /** The number of iterations of the multi-range IN test */
    private static final int MULTI_RANGE_IN_ITERATIONS = 1;

    protected int PK_MODULUS = 3;
    protected long PRETTY_BIG_NUMBER = 1000000000000000L;

    @Override
    protected boolean getCleanupAfterTest() {
        return true;
    }

    public void testInAndBetween() {
        inAndBetweenQuery("longix", new Object[] {1000000000000000L, 0L}, "intix", 1, 2, "idx_long_int_string", 3, 4, 5, 6, 7, 8);
        inAndBetweenQuery("longix", Arrays.asList(new Object[] {1000000000000000L, 0L}), "stringix", "1", "4", "idx_long_int_string", 1, 2, 4, 5, 7, 8);
        failOnError();        
    }

    public void testBetweenAndIn() {
        betweenAndInQuery("longix", 0L, 3000000000000000L, "intix", new Object[] {2, 0}, "idx_long_int_string", 0, 1, 2, 6, 7, 8, 9);
        betweenAndInQuery("longix", 0L, 1000000000000000L, "intix", Arrays.asList(new Object[] {2, 1}), "idx_long_int_string", 3, 4, 5, 6, 7, 8);
        failOnError();        
    }

    public void testPartialBoundsAndEqual() {
        greaterThanAnd1ExtraQuery("longix", 0, "intix", extraEqualPredicateProvider, 0, "idx_long_int_string", 9);
        greaterEqualAnd1ExtraQuery("longix", 0, "intix", extraEqualPredicateProvider, 0, "idx_long_int_string", 0, 1, 2, 9);
        lessThanAnd1ExtraQuery("longix", 1000000000000000L, "intix", extraEqualPredicateProvider, 0, "idx_long_int_string", 0, 1, 2);
        lessEqualAnd1ExtraQuery("longix", 1000000000000000L, "intix", extraEqualPredicateProvider, 0, "idx_long_int_string", 0, 1, 2, 9);
        failOnError();        
    }

    public void testGapBoundsAndEqual() {
        greaterThanAnd1ExtraQuery("longix", 0, "stringix", extraEqualPredicateProvider, "0", "idx_long_int_string", 9);
        greaterEqualAnd1ExtraQuery("longix", 0, "stringix", extraEqualPredicateProvider, "0", "idx_long_int_string", 0, 3, 6, 9);
        lessThanAnd1ExtraQuery("longix", 1000000000000000L, "stringix", extraEqualPredicateProvider, "0", "idx_long_int_string", 0, 3, 6);
        lessEqualAnd1ExtraQuery("longix", 1000000000000000L, "stringix", extraEqualPredicateProvider, "0", "idx_long_int_string", 0, 3, 6, 9);
        failOnError();        
    }

    public void testNegativeInParameter() {
        int keys[] = new int[0];
        try {
            inQuery("id", keys, "PRIMARY", keys);
            error("Query with ''in'' parameter of int[] type should fail.");
        } catch (ClusterJUserException e) {
            if (getDebug()) {
                e.printStackTrace();
            }
            String message = e.getMessage();
            if (!message.contains("id")) {
                error("Query with ''in'' parameter of int[] type should fail.");
            }
        }
        failOnError();        
    }

    public void testPrettyBigIn() {
        int arraySize = 4096;
        Integer[] keys = new Integer[arraySize];
        for (int i = 0; i < arraySize; ++i) {
            keys[i] = i;
        }
        int[] expectedKeys = new int[] {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        for (int i = 0; i < MULTI_RANGE_IN_ITERATIONS; ++i) {
            inQuery("iteration " + Integer.toString(i) + " ", "id", keys, "PRIMARY", expectedKeys);
        }
        failOnError();        
    }

    public void testNegativeInTooBig() {
        int arraySize = 4097;
        Integer[] keys = new Integer[arraySize];
        for (int i = 0; i < arraySize; ++i) {
            keys[i] = i;
        }
        int[] expectedKeys = new int[] {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        try {
            inQuery("id", keys, "PRIMARY", expectedKeys);
            error("Query with more than 4096 elements should fail.");
        } catch (ClusterJUserException e) {
            if (getDebug()) {
                e.printStackTrace();
            }
            String message = e.getMessage();
            if (!message.contains("4097")) {
                error("Query with more than 4096 elements should fail.");
            }
        }
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
