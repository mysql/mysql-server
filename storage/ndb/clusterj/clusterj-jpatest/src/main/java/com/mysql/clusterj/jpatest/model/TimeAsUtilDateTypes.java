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

package com.mysql.clusterj.jpatest.model;

import java.util.Date;

import javax.persistence.Column;
import javax.persistence.Entity;
import javax.persistence.Id;
import javax.persistence.Table;
import javax.persistence.Temporal;
import javax.persistence.TemporalType;

/** Schema
*
drop table if exists timetypes;
create table timetypes (
id int not null primary key,

time_not_null_hash time,
time_not_null_btree time,
time_not_null_both time,
time_not_null_none time

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_time_not_null_hash using hash on timetypes(time_not_null_hash);
create index idx_time_not_null_btree on timetypes(time_not_null_btree);
create unique index idx_time_not_null_both on timetypes(time_not_null_both);

*/
@Entity
@Table(name="timetypes")
public class TimeAsUtilDateTypes implements IdBase {

    private int id;
    private Date time_not_null_hash;
    private Date time_not_null_btree;
    private Date time_not_null_both;
    private Date time_not_null_none;
    @Id
    public int getId() {
        return id;
    }
    public void setId(int id) {
        this.id = id;
    }

    // Date
    @Column(name="time_not_null_hash")
    @Temporal(TemporalType.TIME)
    public Date getTime_not_null_hash() {
        return time_not_null_hash;
    }
    public void setTime_not_null_hash(Date value) {
        this.time_not_null_hash = value;
    }

    @Column(name="time_not_null_btree")
    @Temporal(TemporalType.TIME)
    public Date getTime_not_null_btree() {
        return time_not_null_btree;
    }
    public void setTime_not_null_btree(Date value) {
        this.time_not_null_btree = value;
    }

    @Column(name="time_not_null_both")
    @Temporal(TemporalType.TIME)
    public Date getTime_not_null_both() {
        return time_not_null_both;
    }
    public void setTime_not_null_both(Date value) {
        this.time_not_null_both = value;
    }

    @Column(name="time_not_null_none")
    @Temporal(TemporalType.TIME)
    public Date getTime_not_null_none() {
        return time_not_null_none;
    }
    public void setTime_not_null_none(Date value) {
        this.time_not_null_none = value;
    }

}
