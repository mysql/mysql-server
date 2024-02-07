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

import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.PrimaryKey;

/** Schema
 *
drop table if exists bittypes;
create table bittypes (
 id int not null primary key,

 bit1 bit(1),
 bit2 bit(2),
 bit4 bit(4),
 bit8 bit(8),
 bit16 bit(16),
 bit32 bit(32),
 bit64 bit(64)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

 */

@PersistenceCapable(table="bittypes")
@PrimaryKey(column="id")
public interface BitTypes extends IdBase {

    int getId();
    void setId(int id);

    // boolean
    boolean getBit1();
    void setBit1(boolean value);

    // byte
    byte getBit2();
    void setBit2(byte value);

    // short
    short getBit4();
    void setBit4(short value);

    // int
    int getBit8();
    void setBit8(int value);

    // long
    long getBit16();
    void setBit16(long value);

    // int
    int getBit32();
    void setBit32(int value);

    // long
    long getBit64();
    void setBit64(long value);

}
