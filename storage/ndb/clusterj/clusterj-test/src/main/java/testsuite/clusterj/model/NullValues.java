/*
   Copyright (c) 2010, 2023, Oracle and/or its affiliates.
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
import com.mysql.clusterj.annotation.NullValue;
import com.mysql.clusterj.annotation.PersistenceCapable;
import com.mysql.clusterj.annotation.Persistent;
import com.mysql.clusterj.annotation.PrimaryKey;

@PersistenceCapable(table="nullvalues")
@PrimaryKey(column="id")
public interface NullValues extends IdBase {

    int getId();
    void setId(int id);

    // Integer
    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="int_not_null_default_null_value_default")
    public Integer getIntNotNullDefaultNullValueDefault();
    public void setIntNotNullDefaultNullValueDefault(Integer integer);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="int_not_null_default_null_value_exception")
    public Integer getIntNotNullDefaultNullValueException();
    public void setIntNotNullDefaultNullValueException(Integer integer);

    @Column(name="int_not_null_default_null_value_none")
    public Integer getIntNotNullDefaultNullValueNone();
    public void setIntNotNullDefaultNullValueNone(Integer integer);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="int_not_null_no_default_null_value_default")
    public Integer getIntNotNullNoDefaultNullValueDefault();
    public void setIntNotNullNoDefaultNullValueDefault(Integer integer);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="int_not_null_no_default_null_value_exception")
    public Integer getIntNotNullNoDefaultNullValueException();
    public void setIntNotNullNoDefaultNullValueException(Integer integer);

    @Column(name="int_not_null_no_default_null_value_none")
    public Integer getIntNotNullNoDefaultNullValueNone();
    public void setIntNotNullNoDefaultNullValueNone(Integer integer);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="int_null_default_null_value_default")
    public Integer getIntNullDefaultNullValueDefault();
    public void setIntNullDefaultNullValueDefault(Integer integer);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="int_null_default_null_value_exception")
    public Integer getIntNullDefaultNullValueException();
    public void setIntNullDefaultNullValueException(Integer integer);

    @Column(name="int_null_default_null_value_none")
    public Integer getIntNullDefaultNullValueNone();
    public void setIntNullDefaultNullValueNone(Integer integer);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="int_null_no_default_null_value_default")
    public Integer getIntNullNoDefaultNullValueDefault();
    public void setIntNullNoDefaultNullValueDefault(Integer integer);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="int_null_no_default_null_value_exception")
    public Integer getIntNullNoDefaultNullValueException();
    public void setIntNullNoDefaultNullValueException(Integer integer);

    @Column(name="int_null_no_default_null_value_none")
    public Integer getIntNullNoDefaultNullValueNone();
    public void setIntNullNoDefaultNullValueNone(Integer integer);

    // Long
    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="long_not_null_default_null_value_default")
    public Long getLongNotNullDefaultNullValueDefault();
    public void setLongNotNullDefaultNullValueDefault(Long value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="long_not_null_default_null_value_exception")
    public Long getLongNotNullDefaultNullValueException();
    public void setLongNotNullDefaultNullValueException(Long value);

    @Column(name="long_not_null_default_null_value_none")
    public Long getLongNotNullDefaultNullValueNone();
    public void setLongNotNullDefaultNullValueNone(Long value);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="long_not_null_no_default_null_value_default")
    public Long getLongNotNullNoDefaultNullValueDefault();
    public void setLongNotNullNoDefaultNullValueDefault(Long value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="long_not_null_no_default_null_value_exception")
    public Long getLongNotNullNoDefaultNullValueException();
    public void setLongNotNullNoDefaultNullValueException(Long value);

    @Column(name="long_not_null_no_default_null_value_none")
    public Long getLongNotNullNoDefaultNullValueNone();
    public void setLongNotNullNoDefaultNullValueNone(Long value);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="long_null_default_null_value_default")
    public Long getLongNullDefaultNullValueDefault();
    public void setLongNullDefaultNullValueDefault(Long value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="long_null_default_null_value_exception")
    public Long getLongNullDefaultNullValueException();
    public void setLongNullDefaultNullValueException(Long value);

    @Column(name="long_null_default_null_value_none")
    public Long getLongNullDefaultNullValueNone();
    public void setLongNullDefaultNullValueNone(Long value);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="long_null_no_default_null_value_default")
    public Long getLongNullNoDefaultNullValueDefault();
    public void setLongNullNoDefaultNullValueDefault(Long value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="long_null_no_default_null_value_exception")
    public Long getLongNullNoDefaultNullValueException();
    public void setLongNullNoDefaultNullValueException(Long value);

    @Column(name="long_null_no_default_null_value_none")
    public Long getLongNullNoDefaultNullValueNone();
    public void setLongNullNoDefaultNullValueNone(Long value);

    // Byte
    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="byte_not_null_default_null_value_default")
    public Byte getByteNotNullDefaultNullValueDefault();
    public void setByteNotNullDefaultNullValueDefault(Byte value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="byte_not_null_default_null_value_exception")
    public Byte getByteNotNullDefaultNullValueException();
    public void setByteNotNullDefaultNullValueException(Byte value);

    @Column(name="byte_not_null_default_null_value_none")
    public Byte getByteNotNullDefaultNullValueNone();
    public void setByteNotNullDefaultNullValueNone(Byte value);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="byte_not_null_no_default_null_value_default")
    public Byte getByteNotNullNoDefaultNullValueDefault();
    public void setByteNotNullNoDefaultNullValueDefault(Byte value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="byte_not_null_no_default_null_value_exception")
    public Byte getByteNotNullNoDefaultNullValueException();
    public void setByteNotNullNoDefaultNullValueException(Byte value);

    @Column(name="byte_not_null_no_default_null_value_none")
    public Byte getByteNotNullNoDefaultNullValueNone();
    public void setByteNotNullNoDefaultNullValueNone(Byte value);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="byte_null_default_null_value_default")
    public Byte getByteNullDefaultNullValueDefault();
    public void setByteNullDefaultNullValueDefault(Byte value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="byte_null_default_null_value_exception")
    public Byte getByteNullDefaultNullValueException();
    public void setByteNullDefaultNullValueException(Byte value);

    @Column(name="byte_null_default_null_value_none")
    public Byte getByteNullDefaultNullValueNone();
    public void setByteNullDefaultNullValueNone(Byte value);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="byte_null_no_default_null_value_default")
    public Byte getByteNullNoDefaultNullValueDefault();
    public void setByteNullNoDefaultNullValueDefault(Byte value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="byte_null_no_default_null_value_exception")
    public Byte getByteNullNoDefaultNullValueException();
    public void setByteNullNoDefaultNullValueException(Byte value);

    @Column(name="byte_null_no_default_null_value_none")
    public Byte getByteNullNoDefaultNullValueNone();
    public void setByteNullNoDefaultNullValueNone(Byte value);

    // Short
    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="short_not_null_default_null_value_default")
    public Short getShortNotNullDefaultNullValueDefault();
    public void setShortNotNullDefaultNullValueDefault(Short value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="short_not_null_default_null_value_exception")
    public Short getShortNotNullDefaultNullValueException();
    public void setShortNotNullDefaultNullValueException(Short value);

    @Column(name="short_not_null_default_null_value_none")
    public Short getShortNotNullDefaultNullValueNone();
    public void setShortNotNullDefaultNullValueNone(Short value);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="short_not_null_no_default_null_value_default")
    public Short getShortNotNullNoDefaultNullValueDefault();
    public void setShortNotNullNoDefaultNullValueDefault(Short value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="short_not_null_no_default_null_value_exception")
    public Short getShortNotNullNoDefaultNullValueException();
    public void setShortNotNullNoDefaultNullValueException(Short value);

    @Column(name="short_not_null_no_default_null_value_none")
    public Short getShortNotNullNoDefaultNullValueNone();
    public void setShortNotNullNoDefaultNullValueNone(Short value);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="short_null_default_null_value_default")
    public Short getShortNullDefaultNullValueDefault();
    public void setShortNullDefaultNullValueDefault(Short value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="short_null_default_null_value_exception")
    public Short getShortNullDefaultNullValueException();
    public void setShortNullDefaultNullValueException(Short value);

    @Column(name="short_null_default_null_value_none")
    public Short getShortNullDefaultNullValueNone();
    public void setShortNullDefaultNullValueNone(Short value);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="short_null_no_default_null_value_default")
    public Short getShortNullNoDefaultNullValueDefault();
    public void setShortNullNoDefaultNullValueDefault(Short value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="short_null_no_default_null_value_exception")
    public Short getShortNullNoDefaultNullValueException();
    public void setShortNullNoDefaultNullValueException(Short value);

    @Column(name="short_null_no_default_null_value_none")
    public Short getShortNullNoDefaultNullValueNone();
    public void setShortNullNoDefaultNullValueNone(Short value);

    // String
    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="string_not_null_default_null_value_default")
    public String getStringNotNullDefaultNullValueDefault();
    public void setStringNotNullDefaultNullValueDefault(String value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="string_not_null_default_null_value_exception")
    public String getStringNotNullDefaultNullValueException();
    public void setStringNotNullDefaultNullValueException(String value);

    @Column(name="string_not_null_default_null_value_none")
    public String getStringNotNullDefaultNullValueNone();
    public void setStringNotNullDefaultNullValueNone(String value);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="string_not_null_no_default_null_value_default")
    public String getStringNotNullNoDefaultNullValueDefault();
    public void setStringNotNullNoDefaultNullValueDefault(String value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="string_not_null_no_default_null_value_exception")
    public String getStringNotNullNoDefaultNullValueException();
    public void setStringNotNullNoDefaultNullValueException(String value);

    @Column(name="string_not_null_no_default_null_value_none")
    public String getStringNotNullNoDefaultNullValueNone();
    public void setStringNotNullNoDefaultNullValueNone(String value);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="string_null_default_null_value_default")
    public String getStringNullDefaultNullValueDefault();
    public void setStringNullDefaultNullValueDefault(String value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="string_null_default_null_value_exception")
    public String getStringNullDefaultNullValueException();
    public void setStringNullDefaultNullValueException(String value);

    @Column(name="string_null_default_null_value_none")
    public String getStringNullDefaultNullValueNone();
    public void setStringNullDefaultNullValueNone(String value);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="string_null_no_default_null_value_default")
    public String getStringNullNoDefaultNullValueDefault();
    public void setStringNullNoDefaultNullValueDefault(String value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="string_null_no_default_null_value_exception")
    public String getStringNullNoDefaultNullValueException();
    public void setStringNullNoDefaultNullValueException(String value);

    @Column(name="string_null_no_default_null_value_none")
    public String getStringNullNoDefaultNullValueNone();
    public void setStringNullNoDefaultNullValueNone(String value);

    // Float
    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="float_not_null_default_null_value_default")
    public Float getFloatNotNullDefaultNullValueDefault();
    public void setFloatNotNullDefaultNullValueDefault(Float value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="float_not_null_default_null_value_exception")
    public Float getFloatNotNullDefaultNullValueException();
    public void setFloatNotNullDefaultNullValueException(Float value);

    @Column(name="float_not_null_default_null_value_none")
    public Float getFloatNotNullDefaultNullValueNone();
    public void setFloatNotNullDefaultNullValueNone(Float value);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="float_not_null_no_default_null_value_default")
    public Float getFloatNotNullNoDefaultNullValueDefault();
    public void setFloatNotNullNoDefaultNullValueDefault(Float value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="float_not_null_no_default_null_value_exception")
    public Float getFloatNotNullNoDefaultNullValueException();
    public void setFloatNotNullNoDefaultNullValueException(Float value);

    @Column(name="float_not_null_no_default_null_value_none")
    public Float getFloatNotNullNoDefaultNullValueNone();
    public void setFloatNotNullNoDefaultNullValueNone(Float value);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="float_null_default_null_value_default")
    public Float getFloatNullDefaultNullValueDefault();
    public void setFloatNullDefaultNullValueDefault(Float value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="float_null_default_null_value_exception")
    public Float getFloatNullDefaultNullValueException();
    public void setFloatNullDefaultNullValueException(Float value);

    @Column(name="float_null_default_null_value_none")
    public Float getFloatNullDefaultNullValueNone();
    public void setFloatNullDefaultNullValueNone(Float value);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="float_null_no_default_null_value_default")
    public Float getFloatNullNoDefaultNullValueDefault();
    public void setFloatNullNoDefaultNullValueDefault(Float value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="float_null_no_default_null_value_exception")
    public Float getFloatNullNoDefaultNullValueException();
    public void setFloatNullNoDefaultNullValueException(Float value);

    @Column(name="float_null_no_default_null_value_none")
    public Float getFloatNullNoDefaultNullValueNone();
    public void setFloatNullNoDefaultNullValueNone(Float value);

    // Double
    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="double_not_null_default_null_value_default")
    public Double getDoubleNotNullDefaultNullValueDefault();
    public void setDoubleNotNullDefaultNullValueDefault(Double value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="double_not_null_default_null_value_exception")
    public Double getDoubleNotNullDefaultNullValueException();
    public void setDoubleNotNullDefaultNullValueException(Double value);

    @Column(name="double_not_null_default_null_value_none")
    public Double getDoubleNotNullDefaultNullValueNone();
    public void setDoubleNotNullDefaultNullValueNone(Double value);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="double_not_null_no_default_null_value_default")
    public Double getDoubleNotNullNoDefaultNullValueDefault();
    public void setDoubleNotNullNoDefaultNullValueDefault(Double value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="double_not_null_no_default_null_value_exception")
    public Double getDoubleNotNullNoDefaultNullValueException();
    public void setDoubleNotNullNoDefaultNullValueException(Double value);

    @Column(name="double_not_null_no_default_null_value_none")
    public Double getDoubleNotNullNoDefaultNullValueNone();
    public void setDoubleNotNullNoDefaultNullValueNone(Double value);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="double_null_default_null_value_default")
    public Double getDoubleNullDefaultNullValueDefault();
    public void setDoubleNullDefaultNullValueDefault(Double value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="double_null_default_null_value_exception")
    public Double getDoubleNullDefaultNullValueException();
    public void setDoubleNullDefaultNullValueException(Double value);

    @Column(name="double_null_default_null_value_none")
    public Double getDoubleNullDefaultNullValueNone();
    public void setDoubleNullDefaultNullValueNone(Double value);

    @Persistent(nullValue=NullValue.DEFAULT)
    @Column(name="double_null_no_default_null_value_default")
    public Double getDoubleNullNoDefaultNullValueDefault();
    public void setDoubleNullNoDefaultNullValueDefault(Double value);

    @Persistent(nullValue=NullValue.EXCEPTION)
    @Column(name="double_null_no_default_null_value_exception")
    public Double getDoubleNullNoDefaultNullValueException();
    public void setDoubleNullNoDefaultNullValueException(Double value);

    @Column(name="double_null_no_default_null_value_none")
    public Double getDoubleNullNoDefaultNullValueNone();
    public void setDoubleNullNoDefaultNullValueNone(Double value);

}
