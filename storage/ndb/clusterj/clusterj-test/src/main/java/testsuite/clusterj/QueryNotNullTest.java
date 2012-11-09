/*
   Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

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

import testsuite.clusterj.model.AllPrimitives;

/** Query for columns compared to not null.
 * Predicates using equal null cannot use indexes, although indexes can 
 * be used for non-null comparisons.
 * This test is based on QueryExtraConditionsTest.
 */
public class QueryNotNullTest extends AbstractQueryTest {
    /*
    drop table if exists allprimitives;
    create table allprimitives (
    id int not null primary key,

    int_not_null_hash int not null,
    int_not_null_btree int not null,
    int_not_null_both int not null,
    int_not_null_none int not null,
    int_null_hash int,
    int_null_btree int,
    int_null_both int,
    int_null_none int,

    byte_not_null_hash tinyint not null,
    byte_not_null_btree tinyint not null,
    byte_not_null_both tinyint not null,
    byte_not_null_none tinyint not null,
    byte_null_hash tinyint,
    byte_null_btree tinyint,
    byte_null_both tinyint,
    byte_null_none tinyint,

    short_not_null_hash smallint not null,
    short_not_null_btree smallint not null,
    short_not_null_both smallint not null,
    short_not_null_none smallint not null,
    short_null_hash smallint,
    short_null_btree smallint,
    short_null_both smallint,
    short_null_none smallint,

    long_not_null_hash bigint not null,
    long_not_null_btree bigint not null,
    long_not_null_both bigint not null,
    long_not_null_none bigint not null,
    long_null_hash bigint,
    long_null_btree bigint,
    long_null_both bigint,
    long_null_none bigint
      */

    @Override
    public Class<?> getInstanceType() {
        return AllPrimitives.class;
    }

    @Override
    void createInstances(int number) {
        createAllPrimitivesInstances(10);
    }

    public void testExtraEqualNotEqualNull() {
        equalAnd1ExtraQuery("int_not_null_btree", 8, "int_null_none", extraNotEqualPredicateProvider, null, "idx_int_not_null_btree", 8);
        equalAnd1ExtraQuery("int_not_null_hash", 8, "int_null_none", extraNotEqualPredicateProvider, null, "none", 8);
        equalAnd1ExtraQuery("int_not_null_both", 8, "int_null_none", extraNotEqualPredicateProvider, null, "idx_int_not_null_both", 8);
        equalAnd1ExtraQuery("int_not_null_none", 8, "int_null_none", extraNotEqualPredicateProvider, null, "none", 8);
        failOnError();        
    }

    public void testBtreeNotEqualNull() {
        notEqualQuery("int_not_null_btree", "none", null, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        notEqualQuery("int_null_btree", "none", null, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        failOnError();        
    }

    public void testHashNotEqualNull() {
        notEqualQuery("int_not_null_hash", "none", null, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        notEqualQuery("int_null_hash", "none", null, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        failOnError();        
    }

    public void testBothNotEqualNull() {
        notEqualQuery("int_not_null_both", "none", null, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        notEqualQuery("int_null_both", "none", null, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        failOnError();        
    }

    public void testNoneNotEqualNull() {
        notEqualQuery("int_not_null_none", "none", null, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        notEqualQuery("int_null_none", "none", null, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        failOnError();        
    }

    public void testExtraEqualIsNotNull() {
        equalAnd1ExtraQuery("int_not_null_btree", 8, "int_null_none", extraIsNotNullPredicateProvider, "dummy unused value", "idx_int_not_null_btree", 8);
        equalAnd1ExtraQuery("int_not_null_hash", 8, "int_null_none", extraIsNotNullPredicateProvider, "dummy unused value", "none", 8);
        equalAnd1ExtraQuery("int_not_null_both", 8, "int_null_none", extraIsNotNullPredicateProvider, "dummy unused value", "idx_int_not_null_both", 8);
        equalAnd1ExtraQuery("int_not_null_none", 8, "int_null_none", extraIsNotNullPredicateProvider, "dummy unused value", "none", 8);
        failOnError();        
    }

    public void testBtreeIsNotNull() {
        isNotNullQuery("int_not_null_btree", "none", 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        isNotNullQuery("int_null_btree", "none", 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        failOnError();        
    }

    public void testHashIsNotNull() {
        isNotNullQuery("int_not_null_hash", "none", 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        isNotNullQuery("int_null_hash", "none", 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        failOnError();        
    }

    public void testBothIsNotNull() {
        isNotNullQuery("int_not_null_both", "none", 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        isNotNullQuery("int_null_both", "none", 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        failOnError();        
    }

    public void testNoneIsNotNull() {
        isNotNullQuery("int_not_null_none", "none", 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        isNotNullQuery("int_null_none", "none", 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        failOnError();        
    }

}
