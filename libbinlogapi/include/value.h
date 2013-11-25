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
#include <climits>
#include <iostream>
#ifndef DATETIME_MAX_DECIMALS
  #define DATETIME_MAX_DECIMALS 6
#endif

//TODO: Remove the ifdef condition
/*
  The following enum is defined in mysql_com.h. It is not feasible to include
  the complete header here, therefore it might be a good idea to define it in
  a header file common to both the server (not only replication code), but the
  client code here, and binlogapi.
  //TODO: Add a common header file if reviewers approve the idea.
*/
#ifndef   _mysql_com_h
enum enum_field_types { MYSQL_TYPE_DECIMAL, MYSQL_TYPE_TINY,
                        MYSQL_TYPE_SHORT,  MYSQL_TYPE_LONG,
                        MYSQL_TYPE_FLOAT,  MYSQL_TYPE_DOUBLE,
                        MYSQL_TYPE_NULL,   MYSQL_TYPE_TIMESTAMP,
                        MYSQL_TYPE_LONGLONG,MYSQL_TYPE_INT24,
                        MYSQL_TYPE_DATE,   MYSQL_TYPE_TIME,
                        MYSQL_TYPE_DATETIME, MYSQL_TYPE_YEAR,
                        MYSQL_TYPE_NEWDATE, MYSQL_TYPE_VARCHAR,
                        MYSQL_TYPE_BIT,
                        MYSQL_TYPE_TIMESTAMP2,
                        MYSQL_TYPE_DATETIME2,
                        MYSQL_TYPE_TIME2,
                        MYSQL_TYPE_NEWDECIMAL=246,
                        MYSQL_TYPE_ENUM=247,
                        MYSQL_TYPE_SET=248,
                        MYSQL_TYPE_TINY_BLOB=249,
                        MYSQL_TYPE_MEDIUM_BLOB=250,
                        MYSQL_TYPE_LONG_BLOB=251,
                        MYSQL_TYPE_BLOB=252,
                        MYSQL_TYPE_VAR_STRING=253,
                        MYSQL_TYPE_STRING=254,
                        MYSQL_TYPE_GEOMETRY=255

};
#endif
using namespace binary_log;
namespace binary_log {
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
                    unsigned int metadata);


/**
   Compute the maximum display length of a field.

   @param sql_type Type of the field
   @param metadata The metadata from the master for the field.
   @return Maximum length of the field in bytes.
 */
uint32_t
max_display_length_for_field(enum_field_types sql_type, unsigned int metadata);

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


} // end namespace binary_log
#endif	/* VALUE_ADAPTER_INCLUDED */
