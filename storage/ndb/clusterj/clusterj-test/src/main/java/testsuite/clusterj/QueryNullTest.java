/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

import testsuite.clusterj.model.AllPrimitives;

import com.mysql.clusterj.ClusterJUserException;

/** Query for columns compared to null. Comparisons using greaterThan,
 * greaterEqual, lessThan, and lessEqual throw ClusterJUserException.
 * Predicates using equal null cannot use indexes, although indexes can 
 * be used for non-null comparisons.
 * This test is based on QueryExtraConditionsTest.
 */
public class QueryNullTest extends AbstractQueryTest {
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

    public void testExtraEqualNull() {
        equalAnd1ExtraQuery("int_not_null_btree", 8, "int_null_none", extraEqualPredicateProvider, null, "idx_int_not_null_btree");
        equalAnd1ExtraQuery("int_not_null_hash", 8, "int_null_none", extraEqualPredicateProvider, null, "none");
        equalAnd1ExtraQuery("int_not_null_both", 8, "int_null_none", extraEqualPredicateProvider, null, "idx_int_not_null_both");
        equalAnd1ExtraQuery("int_not_null_none", 8, "int_null_none", extraEqualPredicateProvider, null, "none");
        failOnError();        
    }

    public void testBtreeEqualNull() {
        equalQuery("int_not_null_btree", "none", null);
        equalQuery("int_null_btree", "none", null);
        failOnError();        
    }

    public void testHashEqualNull() {
        equalQuery("int_not_null_hash", "none", null);
        equalQuery("int_null_hash", "none", null);
        failOnError();        
    }

    public void testBothEqualNull() {
        equalQuery("int_not_null_both", "none", null);
        equalQuery("int_null_both", "none", null);
        failOnError();        
    }

    public void testNoneEqualNull() {
        equalQuery("int_not_null_none", "none", null);
        equalQuery("int_null_none", "none", null);
        failOnError();        
    }

    public void testExtraIsNull() {
        equalAnd1ExtraQuery("int_not_null_btree", 8, "int_null_none", extraIsNullPredicateProvider, "dummy unused value", "idx_int_not_null_btree");
        equalAnd1ExtraQuery("int_not_null_hash", 8, "int_null_none", extraIsNullPredicateProvider, "dummy unused value", "none");
        equalAnd1ExtraQuery("int_not_null_both", 8, "int_null_none", extraIsNullPredicateProvider, "dummy unused value", "idx_int_not_null_both");
        equalAnd1ExtraQuery("int_not_null_none", 8, "int_null_none", extraIsNullPredicateProvider, "dummy unused value", "none");
        failOnError();        
    }

    public void testBtreeIsNull() {
        isNullQuery("int_not_null_btree", "none");
        isNullQuery("int_null_btree", "none");
        failOnError();        
    }

    public void testHashIsNull() {
        isNullQuery("int_not_null_hash", "none");
        isNullQuery("int_null_hash", "none");
        failOnError();        
    }

    public void testBothIsNull() {
        isNullQuery("int_not_null_both", "none");
        isNullQuery("int_null_both", "none");
        failOnError();        
    }

    public void testNoneIsNull() {
        isNullQuery("int_not_null_none", "none");
        isNullQuery("int_null_none", "none");
        failOnError();        
    }

    public void testGreaterThanNull() {
        try {
            greaterThanQuery("int_not_null_btree", "none", null);
            error("Greater than query should throw ClusterJUserException with null parameter.");
        } catch (ClusterJUserException ex) {
            // good catch
        }
    }

    public void testGreaterEqualNull() {
        try {
            greaterEqualQuery("int_not_null_btree", "none", null);
            error("Greater equal query should throw ClusterJUserException with null parameter.");
        } catch (ClusterJUserException ex) {
            // good catch
        }
    }

    public void testLessThanNull() {
        try {
            lessThanQuery("int_not_null_btree", "none", null);
            error("Less than query should throw ClusterJUserException with null parameter.");
        } catch (ClusterJUserException ex) {
            // good catch
        }
    }

    public void testLessEqualNull() {
        try {
            lessEqualQuery("int_not_null_btree", "none", null);
            error("Less equal query should throw ClusterJUserException with null parameter.");
        } catch (ClusterJUserException ex) {
            // good catch
        }
    }

}
