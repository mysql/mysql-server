/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package testsuite.clusterj.model;

import com.mysql.clusterj.annotation.Column;
import com.mysql.clusterj.annotation.Index;
import com.mysql.clusterj.annotation.Indices;
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.PrimaryKey;

import java.util.Date;

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
@Indices({
    @Index(name="idx_datetime_not_null_both", columns=@Column(name="datetime_not_null_both"))
})
@PersistenceCapable(table="datetimetypes")
@PrimaryKey(column="id")
public interface DatetimeAsUtilDateTypes extends IdBase {

    int getId();
    void setId(int id);

    // java.util.Date
    @Column(name="datetime_not_null_hash")
    @Index(name="idx_datetime_not_null_hash")
    Date getDatetime_not_null_hash();
    void setDatetime_not_null_hash(Date value);

    @Column(name="datetime_not_null_btree")
    @Index(name="idx_datetime_not_null_btree")
    Date getDatetime_not_null_btree();
    void setDatetime_not_null_btree(Date value);

    @Column(name="datetime_not_null_both")
    Date getDatetime_not_null_both();
    void setDatetime_not_null_both(Date value);

    @Column(name="datetime_not_null_none")
    Date getDatetime_not_null_none();
    void setDatetime_not_null_none(Date value);

}
