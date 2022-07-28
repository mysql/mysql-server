/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.
   Use is subject to license terms.

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

package testsuite.clusterj.model;

import com.mysql.clusterj.annotation.Column;
import com.mysql.clusterj.annotation.Index;
import com.mysql.clusterj.annotation.Indices;
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.PrimaryKey;
import java.util.Date;

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
@Indices({
    @Index(name="idx_date_not_null_both", columns=@Column(name="date_not_null_both"))
})
@PersistenceCapable(table="datetypes")
@PrimaryKey(column="id")
public interface DateAsUtilDateTypes extends IdBase {

    int getId();
    void setId(int id);

    // Date
    @Column(name="date_not_null_hash")
    @Index(name="idx_date_not_null_hash")
    Date getDate_not_null_hash();
    void setDate_not_null_hash(Date value);

    @Column(name="date_not_null_btree")
    @Index(name="idx_date_not_null_btree")
    Date getDate_not_null_btree();
    void setDate_not_null_btree(Date value);

    @Column(name="date_not_null_both")
    Date getDate_not_null_both();
    void setDate_not_null_both(Date value);

    @Column(name="date_not_null_none")
    Date getDate_not_null_none();
    void setDate_not_null_none(Date value);

}
