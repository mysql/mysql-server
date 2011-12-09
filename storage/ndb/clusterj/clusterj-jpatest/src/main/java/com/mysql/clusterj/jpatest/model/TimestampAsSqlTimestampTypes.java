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

import java.sql.Timestamp;

import javax.persistence.Column;
import javax.persistence.Entity;
import javax.persistence.Id;
import javax.persistence.Table;

/** Schema
 *
drop table if exists timestamptypes;
create table timestamptypes (
 id int not null primary key,

 timestamp_not_null_hash timestamp,
 timestamp_not_null_btree timestamp,
 timestamp_not_null_both timestamp,
 timestamp_not_null_none timestamp

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_timestamp_not_null_hash using hash on timestamptypes(timestamp_not_null_hash);
create index idx_timestamp_not_null_btree on timestamptypes(timestamp_not_null_btree);
create unique index idx_timestamp_not_null_both on timestamptypes(timestamp_not_null_both);

 */
@Entity
@Table(name="timestamptypes")
public class TimestampAsSqlTimestampTypes implements IdBase {

    private int id;
    private Timestamp timestamp_not_null_hash;
    private Timestamp timestamp_not_null_btree;
    private Timestamp timestamp_not_null_both;
    private Timestamp timestamp_not_null_none;

    @Id
    public int getId() {
        return id;
    }
    public void setId(int id) {
        this.id = id;
    }

    // Timestamp
    @Column(name="timestamp_not_null_hash")
    public Timestamp getTimestamp_not_null_hash() {
        return timestamp_not_null_hash;
    }
    public void setTimestamp_not_null_hash(Timestamp value) {
        this.timestamp_not_null_hash = value;
    }

    @Column(name="timestamp_not_null_btree")
    public Timestamp getTimestamp_not_null_btree() {
        return timestamp_not_null_btree;
    }
    public void setTimestamp_not_null_btree(Timestamp value) {
        this.timestamp_not_null_btree = value;
    }

    @Column(name="timestamp_not_null_both")
    public Timestamp getTimestamp_not_null_both() {
        return timestamp_not_null_both;
    }
    public void setTimestamp_not_null_both(Timestamp value) {
        this.timestamp_not_null_both = value;
    }

    @Column(name="timestamp_not_null_none")
    public Timestamp getTimestamp_not_null_none() {
        return timestamp_not_null_none;
    }
    public void setTimestamp_not_null_none(Timestamp value) {
        this.timestamp_not_null_none = value;
    }

}
