/*
Copyright (c) 2003, 2011, 2013, Oracle and/or its affiliates. All rights
reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of
the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/

#ifndef VALUE_ADAPTER_INCLUDED
#define	VALUE_ADAPTER_INCLUDED

#include "protocol.h"
#include <my_global.h>
#include <mysql.h>
#include <climits>
#include <iostream>
#define MAX_TIME_WIDTH 10
#define MAX_DATETIME_WIDTH 19
#define DATETIME_MAX_DECIMALS 6

using namespace mysql;
namespace mysql {
/**
 This helper function calculates the size in bytes of a particular field in a
 row type event as defined by the field_ptr and metadata_ptr arguments.
 @param column_type Field type code
 @param field_ptr The field data
 @param metadata_ptr The field metadata

 @note We need the actual field data because the string field size is not
 part of the meta data. :(

 @return The size in bytes of a particular field
*/
uint32_t calc_field_size(unsigned char column_type, const unsigned char *field_ptr,
                    uint32_t metadata);


/**
 * A value object class which encapsluate a tuple
 * (value type, metadata, storage)
 * and provide for views to this storage through a well defined interface.
 *
 * Can be used with a Converter to convert between different Values.
 */
class Value
{
public:
    Value(enum_field_types type, uint32_t metadata, const char *storage) :
      m_type(type), m_storage(storage), m_metadata(metadata), m_is_null(false)
    {
      m_size= calc_field_size((unsigned char)type,
                              (const unsigned char*)storage,
                              metadata);
    };

    Value()
    {
      m_size= 0;
      m_storage= 0;
      m_metadata= 0;
      m_is_null= false;
    }

    /**
     * Copy constructor
     */
    Value(const Value& val);

    Value &operator=(const Value &val);
    bool operator==(const Value &val) const;
    bool operator!=(const Value &val) const;

    ~Value() {}

    void is_null(bool s) { m_is_null= s; }
    bool is_null(void) const { return m_is_null; }

    const char *storage() const { return m_storage; }

    /**
     * Get the length in bytes of the entire storage (any metadata part +
     * atual data)
     */
    uint32_t length() const { return m_size; }
    enum_field_types type() const { return m_type; }
    uint32_t metadata() const { return m_metadata; }

    /**
     * Returns the integer representation of a storage of a pre-specified
     * type.
     */
    int32_t as_int32() const;

    /**
     * Returns the integer representation of a storage of pre-specified
     * type.
     */
    int64_t as_int64() const;

    /**
     * Returns the integer representation of a storage of pre-specified
     * type.
     */
    int8_t as_int8() const;

    /**
     * Returns the integer representation of a storage of pre-specified
     * type.
     */
    int16_t as_int16() const;

    /**
     * Returns a pointer to the character data of a string type stored
     * in the pre-defined storage.
     * @note The position is an offset of the storage pointer determined
     * by the metadata and type.
     *
     * @param[out] size The size in bytes of the character string.
     *
     */
    char *as_c_str(unsigned long &size) const;

    /**
     * Returns a pointer to the byte data of a blob type stored in the pre-
     * defined storage.
     * @note The position is an offset of the storage pointer determined
     * by the metadata and type.
     *
     * @param[out] size The size in bytes of the blob data.
     */
    unsigned char *as_blob(unsigned long &size) const;

    float as_float() const;
    double as_double() const;

private:
    enum_field_types m_type;
    uint32_t m_size;
		const char *m_storage;
    uint32_t m_metadata;
    bool m_is_null;
};

class Converter
{
public:
    /**
     * Converts and copies the sql value to a std::string object.
     * @param[out] str The target string
     * @param[in] val The value object to be converted
     */
    void to(std::string &str, const Value &val) const;

    /**
     * Converts and copies the sql value to a long integer.
     * @param[out] out The target variable
     * @param[in] val The value object to be converted
     */
    void to(long &out, const Value &val) const;

    /**
     * Converts and copies the sql value to a floating point number.
     * @param[out] out The target variable
     * @param[in] val The value object to be converted
     */
    void to(float &out, const Value &val) const;
};


} // end namespace mysql
#endif	/* VALUE_ADAPTER_INCLUDED */
