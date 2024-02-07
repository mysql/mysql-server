/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.
   Use is subject to license terms.

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

@Indices({
    @Index(name="idx_int_not_null_both", columns=@Column(name="int_not_null_both")),
    @Index(name="idx_int_null_both", columns=@Column(name="int_null_both")),
    @Index(name="idx_byte_not_null_both", columns=@Column(name="byte_not_null_both")),
    @Index(name="idx_byte_null_both", columns=@Column(name="byte_null_both")),
    @Index(name="idx_short_not_null_both", columns=@Column(name="short_not_null_both")),
    @Index(name="idx_short_null_both", columns=@Column(name="short_null_both")),
    @Index(name="idx_long_not_null_both", columns=@Column(name="long_not_null_both")),
    @Index(name="idx_long_null_both", columns=@Column(name="long_null_both"))
})
@PersistenceCapable(table="allprimitives")
@PrimaryKey(column="id")
public interface AllPrimitives extends IdBase {

    int getId();
    void setId(int id);

    // Integer
    @Column(name="int_not_null_hash")
    @Index(name="idx_int_not_null_hash")
    int getInt_not_null_hash();
    void setInt_not_null_hash(int value);
    @Column(name="int_not_null_btree")
    @Index(name="idx_int_not_null_btree")
    int getInt_not_null_btree();
    void setInt_not_null_btree(int value);
    @Column(name="int_not_null_both")
    int getInt_not_null_both();
    void setInt_not_null_both(int value);
    @Column(name="int_not_null_none")
    int getInt_not_null_none();
    void setInt_not_null_none(int value);
    @Column(name="int_null_hash")
    @Index(name="idx_int_null_hash")
    Integer getInt_null_hash();
    void setInt_null_hash(Integer value);
    @Column(name="int_null_btree")
    @Index(name="idx_int_null_btree")
    Integer getInt_null_btree();
    void setInt_null_btree(Integer value);
    @Column(name="int_null_both")
    Integer getInt_null_both();
    void setInt_null_both(Integer value);
    @Column(name="int_null_none")
    Integer getInt_null_none();
    void setInt_null_none(Integer value);

    // Byte
    @Column(name="byte_not_null_hash")
    @Index(name="idx_byte_not_null_hash")
    byte getByte_not_null_hash();
    void setByte_not_null_hash(byte value);
    @Column(name="byte_not_null_btree")
    @Index(name="idx_byte_not_null_btree")
    byte getByte_not_null_btree();
    void setByte_not_null_btree(byte value);
    @Column(name="byte_not_null_both")
    byte getByte_not_null_both();
    void setByte_not_null_both(byte value);
    @Column(name="byte_not_null_none")
    byte getByte_not_null_none();
    void setByte_not_null_none(byte value);
    @Column(name="byte_null_hash")
    @Index(name="idx_byte_null_hash")
    Byte getByte_null_hash();
    void setByte_null_hash(Byte value);
    @Column(name="byte_null_btree")
    @Index(name="idx_byte_null_btree")
    Byte getByte_null_btree();
    void setByte_null_btree(Byte value);
    @Column(name="byte_null_both")
    Byte getByte_null_both();
    void setByte_null_both(Byte value);
    @Column(name="byte_null_none")
    Byte getByte_null_none();
    void setByte_null_none(Byte value);

    // Short
    @Column(name="short_not_null_hash")
    @Index(name="idx_short_not_null_hash")
    short getShort_not_null_hash();
    void setShort_not_null_hash(short value);
    @Column(name="short_not_null_btree")
    @Index(name="idx_short_not_null_btree")
    short getShort_not_null_btree();
    void setShort_not_null_btree(short value);
    @Column(name="short_not_null_both")
    short getShort_not_null_both();
    void setShort_not_null_both(short value);
    @Column(name="short_not_null_none")
    short getShort_not_null_none();
    void setShort_not_null_none(short value);
    @Column(name="short_null_hash")
    @Index(name="idx_short_null_hash")
    Short getShort_null_hash();
    void setShort_null_hash(Short value);
    @Column(name="short_null_btree")
    @Index(name="idx_short_null_btree")
    Short getShort_null_btree();
    void setShort_null_btree(Short value);
    @Column(name="short_null_both")
    Short getShort_null_both();
    void setShort_null_both(Short value);
    @Column(name="short_null_none")
    Short getShort_null_none();
    void setShort_null_none(Short value);

    // Long
    @Column(name="long_not_null_hash")
    @Index(name="idx_long_not_null_hash")
    long getLong_not_null_hash();
    void setLong_not_null_hash(long value);
    @Column(name="long_not_null_btree")
    @Index(name="idx_long_not_null_btree")
    long getLong_not_null_btree();
    void setLong_not_null_btree(long value);
    @Column(name="long_not_null_both")
    long getLong_not_null_both();
    void setLong_not_null_both(long value);
    @Column(name="long_not_null_none")
    long getLong_not_null_none();
    void setLong_not_null_none(long value);
    @Column(name="long_null_hash")
    @Index(name="idx_long_null_hash")
    Long getLong_null_hash();
    void setLong_null_hash(Long value);
    @Column(name="long_null_btree")
    @Index(name="idx_long_null_btree")
    Long getLong_null_btree();
    void setLong_null_btree(Long value);
    @Column(name="long_null_both")
    Long getLong_null_both();
    void setLong_null_both(Long value);
    @Column(name="long_null_none")
    Long getLong_null_none();
    void setLong_null_none(Long value);


}
