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
drop table if exists binarytypes;
create table binarytypes (
 id int not null primary key,

 binary1 binary(1),
 binary2 binary(2),
 binary4 binary(4),
 binary8 binary(8),
 binary16 binary(16),
 binary32 binary(32),
 binary64 binary(64),
 binary128 binary(128)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

 */

@PersistenceCapable(table="binarytypes")
@PrimaryKey(column="id")
public interface BinaryTypes extends IdBase {

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

}
