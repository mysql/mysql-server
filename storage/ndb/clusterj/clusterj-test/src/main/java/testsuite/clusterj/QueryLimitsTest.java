/*
Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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

import testsuite.clusterj.model.AllPrimitives;

/** Query for limits used to skip query results and limit the number
 * of results returned.
 */
public class QueryLimitsTest extends AbstractQueryTest {
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

    public void test() {
        setLimits(1, 2);
        inQuery("int_not_null_both", new Object[] {4, 6, 9}, "idx_int_not_null_both", 6, 9);
        setLimits(1, 2);
        inQuery("int_not_null_btree", new Object[] {4, 6, 9}, "idx_int_not_null_btree", 6, 9);
        setLimits(1, 0);
        equalQuery("int_not_null_btree", "idx_int_not_null_btree", 8);
        setLimits(1, Long.MAX_VALUE);
        greaterEqualQuery("int_not_null_btree", "idx_int_not_null_btree", 7, 8, 9);
        setLimits(1, 2);
        greaterThanQuery("int_not_null_btree", "idx_int_not_null_btree", 6, 8, 9);
        setLimits(2, 2);
        lessEqualQuery("int_not_null_btree", "idx_int_not_null_btree", 4, 2, 3);
        setLimits(1, 2);
        lessThanQuery("int_not_null_btree", "idx_int_not_null_btree", 4, 1, 2);
        setLimits(1, 2);
        betweenQuery("int_not_null_btree", "idx_int_not_null_btree", 4, 6, 5, 6);
        setLimits(0, 3);
        equalQuery("int_not_null_hash", "idx_int_not_null_hash", 8, 8);
        setLimits(1, 2);
        equalQuery("int_not_null_both", "idx_int_not_null_both", 8);
        setLimits(1, 2);
        greaterEqualQuery("int_not_null_both", "idx_int_not_null_both", 7, 8, 9);
        setLimits(1, 2);
        greaterThanQuery("int_not_null_both", "idx_int_not_null_both", 6, 8, 9);
        setLimits(1, 2);
        lessEqualQuery("int_not_null_both", "idx_int_not_null_both", 4, 1, 2);
        setLimits(1, 2);
        lessThanQuery("int_not_null_both", "idx_int_not_null_both", 4, 1, 2);
        setLimits(1, 2);
        betweenQuery("int_not_null_both", "idx_int_not_null_both", 4, 6, 5, 6);
        setLimits(1, 2);
        equalQuery("int_not_null_none", "none", 8);
        setLimits(0, 0);
        equalQuery("int_not_null_none", "none", 8);
        setLimits(1, 0);
        equalQuery("int_not_null_none", "none", 8);
        failOnError();        
    }

    public void testNegative() {
        if (session.currentTransaction().isActive()) {
            session.currentTransaction().rollback();
        }
        try {
            // bad limit; first parameter must be greater than or equal to zero
            setLimits(-1, 1);
            equalQuery("int_not_null_none", "none", 8);
            error("Bad limit (-1, 1) should fail.");
        } catch (ClusterJUserException ex) {
            // good catch
        }
        if (session.currentTransaction().isActive()) {
            session.currentTransaction().rollback();
        }
        try {
            // bad limit; second parameter must be greater than or equal to zero
            setLimits(1, -1);
            equalQuery("int_not_null_none", "none", 8);
            error("Bad limit (1, -1) should fail.");
        } catch (ClusterJUserException ex) {
            // good catch
        }
        if (session.currentTransaction().isActive()) {
            session.currentTransaction().rollback();
        }
        try {
            // bad limit; cannot use limits for delete operations
            setLimits(0, 1);
            deleteEqualQuery("int_not_null_none", "none", 8, 1);
            error("Bad limit for delete should fail.");
        } catch (ClusterJUserException ex) {
            // good catch
        }
        if (session.currentTransaction().isActive()) {
            session.currentTransaction().rollback();
        }
        try {
            // bad limit; cannot use limits for delete operations
            setLimits(1, 1);
            deleteEqualQuery("int_not_null_none", "none", 8, 1);
            error("Bad limit for delete should fail.");
        } catch (ClusterJUserException ex) {
            // good catch
        }
        failOnError();        
    }

}
