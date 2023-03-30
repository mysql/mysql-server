/*
   Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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

import java.util.List;

import testsuite.clusterj.model.Employee;

import com.mysql.clusterj.ClusterJUserException;
import com.mysql.clusterj.Query;

import com.mysql.clusterj.query.PredicateOperand;
import com.mysql.clusterj.query.QueryBuilder;
import com.mysql.clusterj.query.QueryDomainType;

public class QueryMultipleParameterTest extends AbstractQueryTest {

    /** The persistent class
    @PersistenceCapable(table="t_basic")
    public interface Employee extends IdBase {
        
        @PrimaryKey
        int getId();
        void setId(int id);

        String getName();
        void setName(String name);

        @Index(name="idx_unique_hash_magic")
        int getMagic();
        void setMagic(int magic);

        @Index(name="idx_btree_age")
        Integer getAge();
        void setAge(Integer age);
        */

    @Override
    public Class<?> getInstanceType() {
        return Employee.class;
    }

    @Override
    void createInstances(int number) {
        createEmployeeInstances(10);
        instances.addAll(employees);
    }

    public void test() {
        // QueryBuilder is the sessionFactory for queries
        QueryBuilder builder = session.getQueryBuilder();
        // QueryDomainType is the main interface
        QueryDomainType<Employee>dobj = builder.createQueryDefinition(Employee.class);
        // parameter
        PredicateOperand param1 = dobj.param("param1");
        PredicateOperand param2 = dobj.param("param2");
        // property
        PredicateOperand propertyMagic = dobj.get("magic");
        PredicateOperand propertyId = dobj.get("id");
        // where
        // param1 is used in two different places but same type (int) in both
        dobj.where(propertyMagic.equal(param1).and(propertyId.between(param1, param2)));
        Query<Employee> query = session.createQuery(dobj);
        query.setParameter("param1", 4);
        query.setParameter("param2", 5);
        List<Employee> result = query.getResultList();
        errorIfNotEqual("Wrong size of result", 1, result.size());
        if (result.size() == 1) {
            errorIfNotEqual("Wrong result", 4, result.get(0).getId());
        }
        failOnError();
    }

    public void testNegative() {
        try {
            // QueryBuilder is the sessionFactory for queries
            QueryBuilder builder = session.getQueryBuilder();
            // QueryDomainType is the main interface
            QueryDomainType<Employee>dobj = builder.createQueryDefinition(Employee.class);
            // parameter
            PredicateOperand param1 = dobj.param("param1");
            PredicateOperand param2 = dobj.param("param2");
            // property
            PredicateOperand propertyAge = dobj.get("magic");
            PredicateOperand propertyMagic = dobj.get("name");
            // where
            // expect an exception here because param1 is used for String name and int magic
            dobj.where(propertyAge.equal(param1).and(propertyMagic.between(param1, param2)));
        } catch (ClusterJUserException e) {
            // good catch
        }
    }

}
