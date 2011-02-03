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

package testsuite.clusterj.model;

import com.mysql.clusterj.annotation.Column;
import com.mysql.clusterj.annotation.Index;
import com.mysql.clusterj.annotation.Indices;
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.PrimaryKey;

/** Schema
 *
drop table if exists yeartypes;
create table yeartypes (
 id int not null primary key,

 year_null_hash year,
 year_null_btree year,
 year_null_both year,
 year_null_none year,

 year_not_null_hash year,
 year_not_null_btree year,
 year_not_null_both year,
 year_not_null_none year

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_year_null_hash using hash on yeartypes(year_null_hash);
create index idx_year_null_btree on yeartypes(year_null_btree);
create unique index idx_year_null_both on yeartypes(year_null_both);

create unique index idx_year_not_null_hash using hash on yeartypes(year_not_null_hash);
create index idx_year_not_null_btree on yeartypes(year_not_null_btree);
create unique index idx_year_not_null_both on yeartypes(year_not_null_both);

 */
@Indices({
    @Index(name="idx_year_null_both", columns=@Column(name="year_null_both")),
    @Index(name="idx_year_not_null_both", columns=@Column(name="year_not_null_both"))
})
@PersistenceCapable(table="yeartypes")
@PrimaryKey(column="id")
public interface YearTypes extends IdBase {

    int getId();
    void setId(int id);

    // Year
    @Column(name="year_null_hash")
    @Index(name="idx_year_null_hash")
    Short getYear_null_hash();
    void setYear_null_hash(Short value);

    @Column(name="year_null_btree")
    @Index(name="idx_year_null_btree")
    Short getYear_null_btree();
    void setYear_null_btree(Short value);

    @Column(name="year_null_both")
    Short getYear_null_both();
    void setYear_null_both(Short value);

    @Column(name="year_null_none")
    Short getYear_null_none();
    void setYear_null_none(Short value);

    @Column(name="year_not_null_hash")
    @Index(name="idx_year_not_null_hash")
    short getYear_not_null_hash();
    void setYear_not_null_hash(short value);

    @Column(name="year_not_null_btree")
    @Index(name="idx_year_not_null_btree")
    short getYear_not_null_btree();
    void setYear_not_null_btree(short value);

    @Column(name="year_not_null_both")
    short getYear_not_null_both();
    void setYear_not_null_both(short value);

    @Column(name="year_not_null_none")
    short getYear_not_null_none();
    void setYear_not_null_none(short value);

}
