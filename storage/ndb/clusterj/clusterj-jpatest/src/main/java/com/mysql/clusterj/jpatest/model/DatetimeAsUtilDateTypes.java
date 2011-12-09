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

import java.util.Date;

import javax.persistence.Column;
import javax.persistence.Entity;
import javax.persistence.Id;
import javax.persistence.Table;

/** Schema
 *
drop table if exists datetimetypes;
create table datetimetypes (
 id int not null primary key,

 datetime_null_hash datetime,
 datetime_null_btree datetime,
 datetime_null_both datetime,
 datetime_null_none datetime,

 datetime_not_null_hash datetime,
 datetime_not_null_btree datetime,
 datetime_not_null_both datetime,
 datetime_not_null_none datetime

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_datetime_null_hash using hash on datetimetypes(datetime_null_hash);
create index idx_datetime_null_btree on datetimetypes(datetime_null_btree);
create unique index idx_datetime_null_both on datetimetypes(datetime_null_both);

create unique index idx_datetime_not_null_hash using hash on datetimetypes(datetime_not_null_hash);
create index idx_datetime_not_null_btree on datetimetypes(datetime_not_null_btree);
create unique index idx_datetime_not_null_both on datetimetypes(datetime_not_null_both);

 */
@Entity
@Table(name="datetimetypes")
public class DatetimeAsUtilDateTypes implements IdBase {

    private int id;
    private Date datetime_not_null_hash;
    private Date datetime_not_null_btree;
    private Date datetime_not_null_both;
    private Date datetime_not_null_none;

    @Id
    public int getId() {
        return id;
    }
    public void setId(int id) {
        this.id = id;
    }

    // Timestamp
    @Column(name="datetime_not_null_hash")
    public Date getDatetime_not_null_hash() {
        return datetime_not_null_hash;
    }
    public void setDatetime_not_null_hash(Date value) {
        this.datetime_not_null_hash = value;
    }

    @Column(name="datetime_not_null_btree")
    public Date getDatetime_not_null_btree() {
        return datetime_not_null_btree;
    }
    public void setDatetime_not_null_btree(Date value) {
        this.datetime_not_null_btree = value;
    }

    @Column(name="datetime_not_null_both")
    public Date getDatetime_not_null_both() {
        return datetime_not_null_both;
    }
    public void setDatetime_not_null_both(Date value) {
        this.datetime_not_null_both = value;
    }

    @Column(name="datetime_not_null_none")
    public Date getDatetime_not_null_none() {
        return datetime_not_null_none;
    }
    public void setDatetime_not_null_none(Date value) {
        this.datetime_not_null_none = value;
    }

}
