/*
   Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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
drop table if exists stringtypes;
create table stringtypes (
 id int not null primary key,

 string_null_hash varchar(20),
 string_null_btree varchar(300),
 string_null_both varchar(20),
 string_null_none varchar(300),

 string_not_null_hash varchar(300),
 string_not_null_btree varchar(20),
 string_not_null_both varchar(300),
 string_not_null_none varchar(20),
 unique key idx_string_null_hash (string_null_hash) using hash,
 key idx_string_null_btree (string_null_btree),
 unique key idx_string_null_both (string_null_both),

 unique key idx_string_not_null_hash (string_not_null_hash) using hash,
 key idx_string_not_null_btree (string_not_null_btree),
 unique key idx_string_not_null_both (string_not_null_both)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

 */
//@Indices({
//    @Index(name="idx_string_null_both", columns=@Column(name="string_null_both")),
//    @Index(name="idx_string_not_null_both", columns=@Column(name="string_not_null_both")),
//    @Index(name="idx_string_null_btree", columns=@Column(name="string_null_btree")),
//    @Index(name="idx_string_not_null_btree", columns=@Column(name="string_not_null_btree")),
//    @Index(name="idx_string_null_hash", columns=@Column(name="string_null_hash")),
//    @Index(name="idx_string_not_null_hash", columns=@Column(name="string_not_null_hash"))
//})
@PersistenceCapable(table="stringtypes")
@PrimaryKey(column="id")
public interface StringTypes extends IdBase {

    int getId();
    void setId(int id);

    // String
    @Column(name="string_null_hash")
    @Index(name="idx_string_null_hash")
    String getString_null_hash();
    void setString_null_hash(String value);

    @Column(name="string_null_btree")
    @Index(name="idx_string_null_btree")
    String getString_null_btree();
    void setString_null_btree(String value);

    @Column(name="string_null_both")
    @Index(name="idx_string_null_both")
    String getString_null_both();
    void setString_null_both(String value);

    @Column(name="string_null_none")
    String getString_null_none();
    void setString_null_none(String value);

    @Column(name="string_not_null_hash")
    @Index(name="idx_string_not_null_hash")
    String getString_not_null_hash();
    void setString_not_null_hash(String value);

    @Column(name="string_not_null_btree")
    @Index(name="idx_string_not_null_btree")
    String getString_not_null_btree();
    void setString_not_null_btree(String value);

    @Column(name="string_not_null_both")
    @Index(name="idx_string_not_null_both")
    String getString_not_null_both();
    void setString_not_null_both(String value);

    @Column(name="string_not_null_none")
    String getString_not_null_none();
    void setString_not_null_none(String value);

}
