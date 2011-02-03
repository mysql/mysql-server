/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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
import com.mysql.clusterj.jpatest.model.LazyEmployee;


/** Test lazy loading support. Currently only remove and insert are tested.
 * 
 * Schema
 * 
drop table if exists t_basic;
create table t_basic (
  id int not null,
  name varchar(32), // lazy with load fetch group (name, age)
  age int,          // lazy with no load fetch group
  magic int not null,
  primary key(id)) 
  engine=ndbcluster;
create unique index idx_unique_hash_magic using hash on t_basic(magic);
create index idx_btree_age on t_basic(age);

*/
@Ignore
public class LazyTest extends AbstractJPABaseTest {

    private int NUMBER_OF_INSTANCES = 4;

    /** Subclasses must override this method to provide the model class for the test */
    protected Class<? extends IdBase> getModelClass() {
        return LazyEmployee.class;
    }

    public void test() {
        removeAll(LazyEmployee.class);
        em.getTransaction().begin();
        for (int i = 0; i < NUMBER_OF_INSTANCES ; ++i) {
            LazyEmployee e = createLazyEmployee(i);
            em.persist(e);
        }
        em.getTransaction().commit();
        em.clear();
        em.getTransaction().begin();
        Query query = em.createQuery("select e from LazyEmployee e where e.id = :id");
        for (int i = 0; i < NUMBER_OF_INSTANCES ; ++i) {
            query.setParameter("id", i);
            LazyEmployee e = (LazyEmployee)query.getSingleResult();
            int id = e.getId();
            int magic = e.getMagic();
            // name and age are lazily loaded
            final int age;
            final String name;
            if (0 == i%2) {
                // age and name are loaded separately because age has no load fetch group
                age = e.getAge();
                name = e.getName();
            } else {
                // age and name are loaded together because name's load fetch group includes age
                name = e.getName();
                age = e.getAge();
            }
            String result = new String("Lazy Employee " + id + " magic: " + magic + " age: " + age + " name: " + name);
//            System.out.println(result);
        }
        em.getTransaction().commit();
    }

    private LazyEmployee createLazyEmployee(int i) {
        LazyEmployee lazy = new LazyEmployee();
        lazy.setId(i);
        lazy.setAge(i);
        lazy.setName("LazyEmployee " + i);
        lazy.setMagic(i);
        return lazy;
    }

}
