/*
   Copyright 2010 Sun Microsystems, Inc.
   All rights reserved. Use is subject to license terms.

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

package com.mysql.clusterj.jpatest.model;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;

import javax.persistence.Basic;

/**
 * An Entity test class that has an embedded class.

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
@javax.persistence.Entity
@javax.persistence.Table(name="t_basic")
public class Embedding implements IdBase, Serializable {

    private static final long serialVersionUID = -5449615148226881972L;

    @javax.persistence.Id
    private int id;

    @Basic
    private int magic;

    // defaults to @Embedded
    private Embedded embedded;

    public Embedding() {
    }

    public int getId() {
        return id;
    }
    
    public void setId(int id) {
        this.id = id;
    }
    
    public int getMagic() {
        return magic;
    }

    public void setMagic(int magic) {
        this.magic = magic;
    }

    public Embedded getEmbedded() {
        return embedded;
    }

    public void setEmbedded(Embedded embedded) {
        this.embedded = embedded;
    }

    private void writeObject(ObjectOutputStream out) throws IOException {
        out.defaultWriteObject();
    }
    
    private void readObject(ObjectInputStream in) throws IOException, ClassNotFoundException {
        in.defaultReadObject();
    }
    
    @Override
    public String toString() {
        StringBuffer buffer = new StringBuffer();
        buffer.append("Embedding id: ");
        buffer.append(id);
        buffer.append("; magic: "); 
        buffer.append(magic);
        buffer.append("; embedded: ");
        buffer.append(embedded.toString());
        return buffer.toString();
    }

}

