/*
   Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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

import java.util.Map;

import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.Query;
import com.mysql.clusterj.query.QueryBuilder;
import com.mysql.clusterj.query.QueryDomainType;

import testsuite.clusterj.model.AllPrimitives;

/**
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
 long_null_none bigint *
 */
public class QueryExplainTest extends AbstractQueryTest {

    @Override
    public Class<?> getInstanceType() {
        return AllPrimitives.class;
    }

    @Override
    void createInstances(int number) {
        createAllPrimitivesInstances(10);
    }

    public void testExplainWithNoWhereClause() {
        QueryBuilder builder = session.getQueryBuilder();
        QueryDomainType<AllPrimitives> dobj = builder.createQueryDefinition(AllPrimitives.class);
        Query<AllPrimitives> query = session.createQuery(dobj);
        Map<String, Object> result = query.explain();
        String indexUsed = result.get(Query.INDEX_USED).toString();
        String scanType = result.get(Query.SCAN_TYPE).toString();
        errorIfNotEqual("Query explain with no where clause should have index none", "none", indexUsed);
        errorIfNotEqual("Query explain with no where clause should have scan type TABLE_SCAN", "TABLE_SCAN", scanType);
        failOnError();
    }

    public void testExplainBeforeBindingParameters() {
        QueryBuilder builder = session.getQueryBuilder();
        QueryDomainType<AllPrimitives> dobj = builder.createQueryDefinition(AllPrimitives.class);
        dobj.where(dobj.get("int_null_none").equal(dobj.param("equal")));
        Query<AllPrimitives> query = session.createQuery(dobj);
        try {
            query.explain();
            fail("Explain before binding parameters should throw ClusterJUserException");
        } catch (ClusterJUserException ex) {
            // good catch; make sure message includes parameter name "equal"
            errorIfNotEqual("Message should include parameter name \"equal\"", true, ex.getMessage().contains("equal"));
        }
        failOnError();
    }

    public void testExplainAfterBindingParametersNoIndexEqual() {
        QueryBuilder builder = session.getQueryBuilder();
        QueryDomainType<AllPrimitives> dobj = builder.createQueryDefinition(AllPrimitives.class);
        dobj.where(dobj.get("int_null_none").equal(dobj.param("equal")));
        Query<AllPrimitives> query = session.createQuery(dobj);
        query.setParameter("equal", 1);
        Map<String, Object> result = query.explain();
        String indexUsed = result.get(Query.INDEX_USED).toString();
        String scanType = result.get(Query.SCAN_TYPE).toString();
        errorIfNotEqual("Query explain with no index should have index none", "none", indexUsed);
        errorIfNotEqual("Query explain with no index should have scan type TABLE_SCAN", Query.SCAN_TYPE_TABLE_SCAN, scanType);
        failOnError();
    }

    public void testExplainAfterBindingParametersUniqueEqual() {
        QueryBuilder builder = session.getQueryBuilder();
        QueryDomainType<AllPrimitives> dobj = builder.createQueryDefinition(AllPrimitives.class);
        dobj.where(dobj.get("int_not_null_hash").equal(dobj.param("equal")));
        Query<AllPrimitives> query = session.createQuery(dobj);
        query.setParameter("equal", 1);
        Map<String, Object> result = query.explain();
        String indexUsed = result.get(Query.INDEX_USED).toString();
        String scanType = result.get(Query.SCAN_TYPE).toString();
        errorIfNotEqual("Query explain with PRIMARY key equal should have index int_not_null_hash", "idx_int_not_null_hash", indexUsed);
        errorIfNotEqual("Query explain with PRIMARY key equal should have scan type UNIQUE_KEY", Query.SCAN_TYPE_UNIQUE_KEY, scanType);
        failOnError();
    }

    public void testExplainAfterBindingParametersPrimaryEqual() {
        QueryBuilder builder = session.getQueryBuilder();
        QueryDomainType<AllPrimitives> dobj = builder.createQueryDefinition(AllPrimitives.class);
        dobj.where(dobj.get("id").equal(dobj.param("equal")));
        Query<AllPrimitives> query = session.createQuery(dobj);
        query.setParameter("equal", 1);
        Map<String, Object> result = query.explain();
        String indexUsed = result.get(Query.INDEX_USED).toString();
        String scanType = result.get(Query.SCAN_TYPE).toString();
        errorIfNotEqual("Query explain with PRIMARY key equal should have index PRIMARY", "PRIMARY", indexUsed);
        errorIfNotEqual("Query explain with PRIMARY key equal should have scan type PRIMARY_KEY", Query.SCAN_TYPE_PRIMARY_KEY, scanType);
        failOnError();
    }

    public void testExplainAfterBindingParametersPrimaryLessThan() {
        QueryBuilder builder = session.getQueryBuilder();
        QueryDomainType<AllPrimitives> dobj = builder.createQueryDefinition(AllPrimitives.class);
        dobj.where(dobj.get("id").lessThan(dobj.param("lessThan")));
        Query<AllPrimitives> query = session.createQuery(dobj);
        query.setParameter("lessThan", 1);
        Map<String, Object> result = query.explain();
        String indexUsed = result.get(Query.INDEX_USED).toString();
        String scanType = result.get(Query.SCAN_TYPE).toString();
        errorIfNotEqual("Query explain with PRIMARY key lessThan should have index PRIMARY", "PRIMARY", indexUsed);
        errorIfNotEqual("Query explain with PRIMARY key lessThan should have scan type INDEX_SCAN", Query.SCAN_TYPE_INDEX_SCAN, scanType);
        failOnError();
    }

    public void testExplainAfterBindingParametersPrimaryLessThanNull() {
        QueryBuilder builder = session.getQueryBuilder();
        QueryDomainType<AllPrimitives> dobj = builder.createQueryDefinition(AllPrimitives.class);
        dobj.where(dobj.get("id").lessThan(dobj.param("lessThan")));
        Query<AllPrimitives> query = session.createQuery(dobj);
        query.setParameter("lessThan", null);
        Map<String, Object> result = query.explain();
        String indexUsed = result.get(Query.INDEX_USED).toString();
        String scanType = result.get(Query.SCAN_TYPE).toString();
        errorIfNotEqual("Query explain with PRIMARY key lessThan null should have index none", "none", indexUsed);
        errorIfNotEqual("Query explain with PRIMARY key lessThan null should have scan type TABLE_SCAN", Query.SCAN_TYPE_TABLE_SCAN, scanType);
        failOnError();
    }

}
