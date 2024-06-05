/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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

/** Schema
 *
drop table if exists bytestype;
create table bytestype (
 id int not null primary key,

 bytes_null_hash varbinary(8),
 bytes_null_btree varbinary(8),
 bytes_null_both varbinary(8),
 bytes_null_none varbinary(8),
key idx_bytes_null_btree (bytes_null_btree),
unique key idx_bytes_null_both (bytes_null_both),
unique key idx_bytes_null_hash (bytes_null_hash) using hash

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

 */
@Indices({
    @Index(name="idx_bytes_null_both", columns=@Column(name="bytes_null_both"))
})
@PersistenceCapable(table="bytestype")
@PrimaryKey(column="id")
public interface ByteArrayTypes extends IdBase {

    int getId();
    void setId(int id);

    // Byte Array
    @Column(name="bytes_null_hash")
    @Index(name="idx_bytes_null_hash")
    byte[] getBytes_null_hash();
    void setBytes_null_hash(byte[] value);

    @Column(name="bytes_null_btree")
    @Index(name="idx_bytes_null_btree")
    byte[] getBytes_null_btree();
    void setBytes_null_btree(byte[] value);

    @Column(name="bytes_null_both")
    byte[] getBytes_null_both();
    void setBytes_null_both(byte[] value);

    @Column(name="bytes_null_none")
    byte[] getBytes_null_none();
    void setBytes_null_none(byte[] value);

}
