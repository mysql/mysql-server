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
drop table if exists shortpk;
create table shortpk (
 id smallint not null primary key,
 short_null_none smallint,
 short_null_btree smallint,
 short_null_hash smallint,
 short_null_both smallint,
 ) ENGINE=ndbcluster DEFAULT CHARSET=latin1;
*/

@Entity
@Table(name="shortpk")
public class ShortPK implements Serializable {

    private static final long serialVersionUID = 1L;
    @Id
    private short id;
    private short short_null_none;
    private short short_null_btree;
    private short short_null_hash;
    private short short_null_both;

    public ShortPK() {}

    public short getId() {
        return id;
    }

    public void setId(short id) {
        this.id = id;
    }

    public short getShort_null_none() {
        return short_null_none;
    }

    public void setShort_null_none(short value) {
        this.short_null_none = value;
    }

    public short getShort_null_btree() {
        return short_null_btree;
    }

    public void setShort_null_btree(short value) {
        this.short_null_btree = value;
    }

    public short getShort_null_hash() {
        return short_null_hash;
    }

    public void setShort_null_hash(short value) {
        this.short_null_hash = value;
    }

    public short getShort_null_both() {
        return short_null_both;
    }

    public void setShort_null_both(short value) {
        this.short_null_both = value;
    }

}
