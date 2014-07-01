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

package com.mysql.clusterj.jpatest.model;

import java.io.Serializable;
import javax.persistence.Entity;
import javax.persistence.Id;
import javax.persistence.Table;
/** Schema
 *
drop table if exists bytepk;
create table bytepk (
 id tinyint not null primary key,
 byte_null_none tinyint,
 byte_null_btree tinyint,
 byte_null_hash tinyint,
 byte_null_both tinyint,
 ) ENGINE=ndbcluster DEFAULT CHARSET=latin1;
*/

@Entity
@Table(name="bytepk")
public class BytePK implements Serializable {

    private static final long serialVersionUID = 1L;
    @Id
    private byte id;
    private byte byte_null_none;
    private byte byte_null_btree;
    private byte byte_null_hash;
    private byte byte_null_both;

    public BytePK() {}

    public byte getId() {
        return id;
    }

    public void setId(byte id) {
        this.id = id;
    }

    public byte getByte_null_none() {
        return byte_null_none;
    }

    public void setByte_null_none(byte value) {
        this.byte_null_none = value;
    }

    public byte getByte_null_btree() {
        return byte_null_btree;
    }

    public void setByte_null_btree(byte value) {
        this.byte_null_btree = value;
    }

    public byte getByte_null_hash() {
        return byte_null_hash;
    }

    public void setByte_null_hash(byte value) {
        this.byte_null_hash = value;
    }

    public byte getByte_null_both() {
        return byte_null_both;
    }

    public void setByte_null_both(byte value) {
        this.byte_null_both = value;
    }

}
