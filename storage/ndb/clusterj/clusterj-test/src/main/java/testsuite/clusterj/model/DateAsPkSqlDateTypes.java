/*
   Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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
import java.sql.Date;

/** Schema
 *
drop table if exists datetypes_pk;
create table datetypes_pk (
 id int not null primary key,
 pk_key_date date not null

 date_null_hash date,
 date_null_btree date,
 date_null_both date,
 date_null_none date,

 date_not_null_hash date,
 date_not_null_btree date,
 date_not_null_both date,
 date_not_null_none date,

 PRIMARY KEY (id, pk_key_date)
) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_date_null_hash using hash on datetypes_pk(date_null_hash);
create index idx_date_null_btree on datetypes_pk(date_null_btree);
create unique index idx_date_null_both on datetypes_pk(date_null_both);

create unique index idx_date_not_null_hash using hash on datetypes_pk(date_not_null_hash);
create index idx_date_not_null_btree on datetypes_pk(date_not_null_btree);
create unique index idx_date_not_null_both on datetypes_pk(date_not_null_both);

 */
@Indices({
    @Index(name="idx_date_not_null_both", columns=@Column(name="date_not_null_both"))
})
@PersistenceCapable(table="datetypes_pk")
public interface DateAsPkSqlDateTypes extends DateIdBase {

    @PrimaryKey
    int getId();
    void setId(int id);

    // Date Primary key
    @PrimaryKey
    @Column(name = "pk_key_date")
    Date getPkKeyDate();
    void setPkKeyDate(Date date);

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
