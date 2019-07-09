/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj.jpatest;

import com.mysql.clusterj.jpatest.model.Embedded;
import com.mysql.clusterj.jpatest.model.Embedding;
import com.mysql.clusterj.jpatest.model.IdBase;


/** Test Embedded class support. Currently only remove and insert are tested.
 * 
 * Schema
 * 
drop table if exists t_basic;
create table t_basic (
  id int not null,
  name varchar(32), // embedded
  age int,          // embedded
  magic int not null,
  primary key(id)) 
  engine=ndbcluster;
create unique index idx_unique_hash_magic using hash on t_basic(magic);
create index idx_btree_age on t_basic(age);

*/
@Ignore
public class EmbeddedTest extends AbstractJPABaseTest {

    private int NUMBER_OF_INSTANCES = 10;

    @Override
    protected boolean getDebug() {
        return false;
    }

    /** Subclasses must override this method to provide the model class for the test */
    protected Class<? extends IdBase> getModelClass() {
        return Embedding.class;
    }

    /**
     * The name of the persistence unit that this test class should use
     * by default. This defaults to "ndb".
     */
    @Override
    protected String getPersistenceUnitName() {
        return "ndb";
    }

    public void test() {
        removeAll(Embedding.class);
        em.getTransaction().begin();
        for (int i = 0; i < NUMBER_OF_INSTANCES ; ++i) {
            Embedding e = createEmbedding(i);
            em.persist(e);
        }
        em.getTransaction().commit();
    }

    private Embedding createEmbedding(int i) {
        Embedded embedded = new Embedded();
        embedded.setAge(i);
        embedded.setName("Embedded " + i);
        Embedding embedding = new Embedding();
        embedding.setId(i);
        embedding.setMagic(i);
        embedding.setEmbedded(embedded);
        return embedding;
    }

}
