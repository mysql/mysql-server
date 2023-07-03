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
drop table if exists doubletypes;
create table doubletypes (
 id int not null primary key,

 double_null_hash double,
 double_null_btree double,
 double_null_both double,
 double_null_none double,

 double_not_null_hash double,
 double_not_null_btree double,
 double_not_null_both double,
 double_not_null_none double

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

create unique index idx_double_null_hash using hash on doubletypes(double_null_hash);
create index idx_double_null_btree on doubletypes(double_null_btree);
create unique index idx_double_null_both on doubletypes(double_null_both);

create unique index idx_double_not_null_hash using hash on doubletypes(double_not_null_hash);
create index idx_double_not_null_btree on doubletypes(double_not_null_btree);
create unique index idx_double_not_null_both on doubletypes(double_not_null_both);

 */
//@Indices({
//    @Index(name="idx_double_null_both", columns=@Column(name="double_null_both")),
//    @Index(name="idx_double_not_null_both", columns=@Column(name="double_not_null_both"))
//})
/** Double types allow hash indexes to be defined but ndb-bindings
 * do not allow an equal lookup, so they are not used.
 * If hash indexes are supported in future, uncomment the @Index annotations.
 */
@PersistenceCapable(table="doubletypes")
@PrimaryKey(column="id")
public interface DoubleTypes extends IdBase {

    int getId();
    void setId(int id);

    // Double
    @Column(name="double_null_hash")
//    @Index(name="idx_double_null_hash")
    Double getDouble_null_hash();
    void setDouble_null_hash(Double value);

    @Column(name="double_null_btree")
    @Index(name="idx_double_null_btree")
    Double getDouble_null_btree();
    void setDouble_null_btree(Double value);

    @Column(name="double_null_both")
    Double getDouble_null_both();
    void setDouble_null_both(Double value);

    @Column(name="double_null_none")
    Double getDouble_null_none();
    void setDouble_null_none(Double value);

    @Column(name="double_not_null_hash")
//    @Index(name="idx_double_not_null_hash")
    double getDouble_not_null_hash();
    void setDouble_not_null_hash(double value);

    @Column(name="double_not_null_btree")
    @Index(name="idx_double_not_null_btree")
    double getDouble_not_null_btree();
    void setDouble_not_null_btree(double value);

    @Column(name="double_not_null_both")
    double getDouble_not_null_both();
    void setDouble_not_null_both(double value);

    @Column(name="double_not_null_none")
    double getDouble_not_null_none();
    void setDouble_not_null_none(double value);

}
