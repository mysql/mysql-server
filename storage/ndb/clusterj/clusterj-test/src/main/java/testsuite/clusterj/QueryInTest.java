/*
Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

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

import java.util.Arrays;
import java.util.HashSet;

import com.mysql.clusterj.ClusterJUserException;

import testsuite.clusterj.model.AllPrimitives;

/** Query for columns compared via IN.
 * Predicates using IN cannot use indexes, although indexes can 
 * be used for AND predicates where some of the predicates are IN
 * predicates.
 * This test is based on AbstractQueryTest.
 */
public class QueryInTest extends AbstractQueryTest {
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

    public void testBtreeEqualOrIn() {
        equalOrInQuery("int_not_null_btree", 4, "int_null_none", new Object[] {}, "none", 4);
        equalOrInQuery("int_not_null_btree", 4, "int_null_none", new Object[] {6, 9}, "none", 4, 6, 9);
        equalOrInQuery("int_null_btree", 4, "int_null_none", new Object[] {4, 6, 9}, "none", 4, 6, 9);
        equalOrInQuery("int_null_btree", 4, "int_null_none", new Object[] {6, 6, 6, 9}, "none", 4, 6, 9);

        equalOrInQuery("int_not_null_btree", 4, "int_null_none", Arrays.asList(new Object[] {}), "none", 4);
        equalOrInQuery("int_not_null_btree", 4, "int_null_none", Arrays.asList(new Integer[] {6, 9}), "none", 4, 6, 9);
        equalOrInQuery("int_null_btree", 4, "int_null_none", new HashSet<Integer>(Arrays.asList(new Integer[] {4, 6, 9})), "none", 4, 6, 9);
        equalOrInQuery("int_null_btree", 4, "int_null_none", new HashSet<Integer>(Arrays.asList(new Integer[] {6, 6, 6, 9})), "none", 4, 6, 9);
        failOnError();        
    }

    public void testIn() {
        inQuery("int_not_null_none", new Object[] {4, 6, 9}, "none", 4, 6, 9);
        inQuery("int_not_null_hash", Arrays.asList(new Object[] {4, 6, 9}), "none", 4, 6, 9);
        inQuery("int_not_null_both", new Object[] {4, 6, 9}, "idx_int_not_null_both", 4, 6, 9);
        inQuery("int_not_null_btree", new Object[] {4, 6, 9}, "idx_int_not_null_btree", 4, 6, 9);
        failOnError();        
    }

    public void testInAndIn() {
        inAndInQuery("int_not_null_none", new Object[] {4, 6, 9}, "id", new Object[] {4, 9}, "PRIMARY", 4, 9);
        inAndInQuery("int_not_null_hash", new Object[] {4, 9}, "int_not_null_both", new Object[] {6, 9}, "idx_int_not_null_both", 9);
        inAndInQuery("int_not_null_hash", new Object[] {4, 9}, "int_not_null_btree", new Object[] {6, 9}, "idx_int_not_null_btree", 9);
        inAndInQuery("int_not_null_both", new Object[] {4, 9}, "int_not_null_hash", new Object[] {6, 9}, "idx_int_not_null_both", 9);
        inAndInQuery("int_not_null_btree", new Object[] {4, 9}, "int_not_null_hash", new Object[] {6, 9}, "idx_int_not_null_btree", 9);
        failOnError();        
    }

    public void testHashEqualOrIn() {
        equalOrInQuery("int_not_null_hash", 4, "int_null_both", new Object[] {6, 9}, "none", 4, 6, 9);
        equalOrInQuery("int_null_hash", 4, "int_null_both", new Object[] {6, 9}, "none", 4, 6, 9);
        failOnError();        
    }

    public void testBothEqualOrIn() {
        equalOrInQuery("int_not_null_both", 4, "int_null_hash", new Object[] {6, 9}, "none", 4, 6, 9);
        equalOrInQuery("int_null_both", 4, "int_null_hash", new Object[] {6, 9}, "none", 4, 6, 9);
        failOnError();        
    }

    public void testNoneEqualOrIn() {
        equalOrInQuery("int_not_null_none", 4, "int_null_btree", new Object[] {6, 9}, "none", 4, 6, 9);
        equalOrInQuery("int_null_none", 4, "int_null_btree", new Object[] {6, 9}, "none", 4, 6, 9);
        failOnError();        
    }

    public void testNullParameterForIn() {
        try {
            equalOrInQuery("int_not_null_btree", 4, "int_null_none", null, "none", 4);
            fail("testNullParameterForIn should throw ClusterJUserException.");
        } catch (ClusterJUserException ex) {
            // good catch
        }
    }
}
