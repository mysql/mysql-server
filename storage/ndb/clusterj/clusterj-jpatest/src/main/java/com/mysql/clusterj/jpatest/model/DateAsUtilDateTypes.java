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
drop table if exists datetypes;
create table datetypes (
 id int not null primary key,

 date_null_hash date,
 date_null_btree date,
 date_null_both date,
 date_null_none date,

 date_not_null_hash date,
 date_not_null_btree date,
 date_not_null_both date,
 date_not_null_none date

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_date_null_hash using hash on datetypes(date_null_hash);
create index idx_date_null_btree on datetypes(date_null_btree);
create unique index idx_date_null_both on datetypes(date_null_both);

create unique index idx_date_not_null_hash using hash on datetypes(date_not_null_hash);
create index idx_date_not_null_btree on datetypes(date_not_null_btree);
create unique index idx_date_not_null_both on datetypes(date_not_null_both);

 */
@Entity
@Table(name="datetypes")
public class DateAsUtilDateTypes implements IdBase {

    private int id;
    private Date date_not_null_hash;
    private Date date_not_null_btree;
    private Date date_not_null_both;
    private Date date_not_null_none;
    @Id
    public int getId() {
        return id;
    }
    public void setId(int id) {
        this.id = id;
    }

    // Date
    @Column(name="date_not_null_hash")
    public Date getDate_not_null_hash() {
        return date_not_null_hash;
    }
    public void setDate_not_null_hash(Date value) {
        this.date_not_null_hash = value;
    }

    @Column(name="date_not_null_btree")
    public Date getDate_not_null_btree() {
        return date_not_null_btree;
    }
    public void setDate_not_null_btree(Date value) {
        this.date_not_null_btree = value;
    }

    @Column(name="date_not_null_both")
    public Date getDate_not_null_both() {
        return date_not_null_both;
    }
    public void setDate_not_null_both(Date value) {
        this.date_not_null_both = value;
    }

    @Column(name="date_not_null_none")
    public Date getDate_not_null_none() {
        return date_not_null_none;
    }
    public void setDate_not_null_none(Date value) {
        this.date_not_null_none = value;
    }

}
