/*
   Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj.jpatest;

import javax.persistence.Query;

import com.mysql.clusterj.jpatest.model.IdBase;
import com.mysql.clusterj.jpatest.model.Employee;


/** Test query support.
 * 
 * Schema
 * 
drop table if exists t_basic;
create table t_basic (
  id int not null,
  name varchar(32),
  age int,
  magic int not null,
  primary key(id)) 
  engine=ndbcluster;
create unique index idx_unique_hash_magic using hash on t_basic(magic);
create index idx_btree_age on t_basic(age);

*/
public class QueryCacheTest extends AbstractJPABaseTest {

    private int NUMBER_OF_INSTANCES = 4;

    @Override
    protected int getNumberOfEmployees() {
        return NUMBER_OF_INSTANCES;
    }

    /** Subclasses must override this method to provide the model class for the test */
    protected Class<? extends IdBase> getModelClass() {
        return Employee.class;
    }

    public void test() {
        deleteAll();
        createAll();
        em = emf.createEntityManager();
        em.getTransaction().begin();
        for (int i = 0; i < NUMBER_OF_INSTANCES ; ++i) {
            // deliberately use a different query instance each time to test query caching
            // the query string is cached with its associated prepared statement
            Query query = em.createQuery("select e from Employee e where e.id = :id");
            query.setParameter("id", i);
            Employee e = (Employee)query.getSingleResult();
            int id = e.getId();
            errorIfNotEqual("Fail to verify id", i, id);
        }
        em.getTransaction().commit();
        failOnError();
    }

}
