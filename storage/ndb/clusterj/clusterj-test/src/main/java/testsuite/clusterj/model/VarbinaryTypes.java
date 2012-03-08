/*
 *  Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package testsuite.clusterj.model;

import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.PrimaryKey;

/** Schema
 *
drop table if exists varbinarytypes;
create table varbinarytypes (
 id int not null primary key,

 binary1 varbinary(1),
 binary2 varbinary(2),
 binary4 varbinary(4),
 binary8 varbinary(8),
 binary16 varbinary(16),
 binary32 varbinary(32),
 binary64 varbinary(64),
 binary128 varbinary(128),
 binary256 varbinary(256),
 binary512 varbinary(512),
 binary1024 varbinary(1024),
 binary2048 varbinary(2048)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

 */

@PersistenceCapable(table="varbinarytypes")
@PrimaryKey(column="id")
public interface VarbinaryTypes extends IdBase {

    int getId();
    void setId(int id);

    // Byte Array
    byte[] getBinary1();
    void setBinary1(byte[] value);

    // Byte Array
    byte[] getBinary2();
    void setBinary2(byte[] value);

    // Byte Array
    byte[] getBinary4();
    void setBinary4(byte[] value);

    // Byte Array
    byte[] getBinary8();
    void setBinary8(byte[] value);

    // Byte Array
    byte[] getBinary16();
    void setBinary16(byte[] value);

    // Byte Array
    byte[] getBinary32();
    void setBinary32(byte[] value);

    // Byte Array
    byte[] getBinary64();
    void setBinary64(byte[] value);

    // Byte Array
    byte[] getBinary128();
    void setBinary128(byte[] value);

    // Byte Array
    byte[] getBinary256();
    void setBinary256(byte[] value);

    // Byte Array
    byte[] getBinary512();
    void setBinary512(byte[] value);

    // Byte Array
    byte[] getBinary1024();
    void setBinary1024(byte[] value);

    // Byte Array
    byte[] getBinary2048();
    void setBinary2048(byte[] value);

}
