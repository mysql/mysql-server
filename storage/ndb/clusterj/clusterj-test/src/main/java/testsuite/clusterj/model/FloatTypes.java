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
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.PrimaryKey;

/** Schema
 *
drop table if exists floattypes;
create table floattypes (
 id int not null primary key,

 float_null_hash float,
 float_null_btree float,
 float_null_both float,
 float_null_none float,

 float_not_null_hash float,
 float_not_null_btree float,
 float_not_null_both float,
 float_not_null_none float

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_float_null_hash using hash on floattypes(float_null_hash);
create index idx_float_null_btree on floattypes(float_null_btree);
create unique index idx_float_null_both on floattypes(float_null_both);

create unique index idx_float_not_null_hash using hash on floattypes(float_not_null_hash);
create index idx_float_not_null_btree on floattypes(float_not_null_btree);
create unique index idx_float_not_null_both on floattypes(float_not_null_both);

 */
//@Indices({
//    @Index(name="idx_float_null_both", columns=@Column(name="float_null_both")),
//    @Index(name="idx_float_not_null_both", columns=@Column(name="float_not_null_both"))
//})
/** Float types allow hash indexes to be defined but ndb-bindings
 * do not allow an equal lookup, so they are not used.
 * If hash indexes are supported in future, uncomment the @Index annotations.
 */
@PersistenceCapable(table="floattypes")
@PrimaryKey(column="id")
public interface FloatTypes extends IdBase {

    int getId();
    void setId(int id);

    // Float
    @Column(name="float_null_hash")
///    @Index(name="idx_float_null_hash")
    Float getFloat_null_hash();
    void setFloat_null_hash(Float value);

    @Column(name="float_null_btree")
    @Index(name="idx_float_null_btree")
    Float getFloat_null_btree();
    void setFloat_null_btree(Float value);

    @Column(name="float_null_both")
    Float getFloat_null_both();
    void setFloat_null_both(Float value);

    @Column(name="float_null_none")
    Float getFloat_null_none();
    void setFloat_null_none(Float value);

    @Column(name="float_not_null_hash")
//    @Index(name="idx_float_not_null_hash")
    float getFloat_not_null_hash();
    void setFloat_not_null_hash(float value);

    @Column(name="float_not_null_btree")
    @Index(name="idx_float_not_null_btree")
    float getFloat_not_null_btree();
    void setFloat_not_null_btree(float value);

    @Column(name="float_not_null_both")
    float getFloat_not_null_both();
    void setFloat_not_null_both(float value);

    @Column(name="float_not_null_none")
    float getFloat_not_null_none();
    void setFloat_not_null_none(float value);

}
