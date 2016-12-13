/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "json_binary.h"
#include "json_dom.h"
#include "mysqld.h"             // key_memory_JSON
#include "sql_class.h"          // THD
#include "template_utils.h"     // down_cast
#include <algorithm>            // std::min

#define JSONB_TYPE_SMALL_OBJECT   0x0
#define JSONB_TYPE_LARGE_OBJECT   0x1
#define JSONB_TYPE_SMALL_ARRAY    0x2
#define JSONB_TYPE_LARGE_ARRAY    0x3
#define JSONB_TYPE_LITERAL        0x4
#define JSONB_TYPE_INT16          0x5
#define JSONB_TYPE_UINT16         0x6
#define JSONB_TYPE_INT32          0x7
#define JSONB_TYPE_UINT32         0x8
#define JSONB_TYPE_INT64          0x9
#define JSONB_TYPE_UINT64         0xA
#define JSONB_TYPE_DOUBLE         0xB
#define JSONB_TYPE_STRING         0xC
#define JSONB_TYPE_OPAQUE         0xF

#define JSONB_NULL_LITERAL        '\x00'
#define JSONB_TRUE_LITERAL        '\x01'
#define JSONB_FALSE_LITERAL       '\x02'

/*
  The size of offset or size fields in the small and the large storage
  format for JSON objects and JSON arrays.
*/
#define SMALL_OFFSET_SIZE         2
#define LARGE_OFFSET_SIZE         4

/*
  The size of key entries for objects when using the small storage
  format or the large storage format. In the small format it is 4
  bytes (2 bytes for key length and 2 bytes for key offset). In the
  large format it is 6 (2 bytes for length, 4 bytes for offset).
*/
#define KEY_ENTRY_SIZE_SMALL      (2 + SMALL_OFFSET_SIZE)
#define KEY_ENTRY_SIZE_LARGE      (2 + LARGE_OFFSET_SIZE)

/*
  The size of value entries for objects or arrays. When using the
  small storage format, the entry size is 3 (1 byte for type, 2 bytes
  for offset). When using the large storage format, it is 5 (1 byte
  for type, 4 bytes for offset).
*/
#define VALUE_ENTRY_SIZE_SMALL    (1 + SMALL_OFFSET_SIZE)
#define VALUE_ENTRY_SIZE_LARGE    (1 + LARGE_OFFSET_SIZE)

namespace json_binary
{

/// Status codes for JSON serialization.
enum enum_serialization_result
{
  /**
    Success. The JSON value was successfully serialized.
  */
  OK,
  /**
    The JSON value was too big to be serialized. If this status code
    is returned, and the small storage format is in use, the caller
    should retry the serialization with the large storage format. If
    this status code is returned, and the large format is in use,
    my_error() will already have been called.
  */
  VALUE_TOO_BIG,
  /**
    Some other error occurred. my_error() will have been called with
    more specific information about the failure.
  */
  FAILURE
};

static enum_serialization_result
serialize_json_value(const Json_dom *dom, size_t type_pos, String *dest,
                     size_t depth, bool small_parent);

bool serialize(const Json_dom *dom, String *dest)
{
  // Reset the destination buffer.
  dest->length(0);
  dest->set_charset(&my_charset_bin);

  // Reserve space (one byte) for the type identifier.
  if (dest->append('\0'))
    return true;                              /* purecov: inspected */
  return serialize_json_value(dom, 0, dest, 0, false) != OK;
}


/** Encode a 16-bit int at the end of the destination string. */
static bool append_int16(String *dest, int16 value)
{
  if (dest->reserve(2))
    return true;                              /* purecov: inspected */
  int2store(const_cast<char *>(dest->ptr()) + dest->length(), value);
  dest->length(dest->length() + 2);
  return false;
}


/** Encode a 32-bit int at the end of the destination string. */
static bool append_int32(String *dest, int32 value)
{
  if (dest->reserve(4))
    return true;                              /* purecov: inspected */
  int4store(const_cast<char *>(dest->ptr()) + dest->length(), value);
  dest->length(dest->length() + 4);
  return false;
}


/** Encode a 64-bit int at the end of the destination string. */
static bool append_int64(String *dest, int64 value)
{
  if (dest->reserve(8))
    return true;                              /* purecov: inspected */
  int8store(const_cast<char *>(dest->ptr()) + dest->length(), value);
  dest->length(dest->length() + 8);
  return false;
}


/**
  Append an offset or a size to a String.

  @param dest  the destination String
  @param offset_or_size  the offset or size to append
  @param large  if true, use the large storage format (4 bytes);
                otherwise, use the small storage format (2 bytes)
  @return false if successfully appended, true otherwise
*/
static bool append_offset_or_size(String *dest, size_t offset_or_size,
                                  bool large)
{
  if (large)
    return append_int32(dest, static_cast<int32>(offset_or_size));
  else
    return append_int16(dest, static_cast<int16>(offset_or_size));
}


/**
  Insert an offset or a size at the specified position in a String. It
  is assumed that the String has already allocated enough space to
  hold the value.

  @param dest  the destination String
  @param pos   the position in the String
  @param offset_or_size  the offset or size to append
  @param large  if true, use the large storage format (4 bytes);
                otherwise, use the small storage format (2 bytes)
*/
static void insert_offset_or_size(String *dest, size_t pos,
                                  size_t offset_or_size, bool large)
{
  char *to= const_cast<char*>(dest->ptr()) + pos;
  if (large)
  {
    DBUG_ASSERT(pos + LARGE_OFFSET_SIZE <= dest->alloced_length());
    int4store(to, static_cast<uint32>(offset_or_size));
  }
  else
  {
    DBUG_ASSERT(pos + SMALL_OFFSET_SIZE <= dest->alloced_length());
    int2store(to, static_cast<uint16>(offset_or_size));
  }
}


/**
  Append a length to a String. The number of bytes used to store the length
  uses a variable number of bytes depending on how large the length is. If the
  highest bit in a byte is 1, then the length is continued on the next byte.
  The least significant bits are stored in the first byte.

  @param  dest   the destination String
  @param  length the length to write
  @return false on success, true on error
*/
static bool append_variable_length(String *dest, size_t length)
{
  do
  {
    // Filter out the seven least significant bits of length.
    uchar ch= (length & 0x7F);

    /*
      Right-shift length to drop the seven least significant bits. If there
      is more data in length, set the high bit of the byte we're writing
      to the String.
    */
    length>>= 7;
    if (length != 0)
      ch|= 0x80;

    if (dest->append(ch))
      return true;                            /* purecov: inspected */
  }
  while (length != 0);

  // Successfully appended the length.
  return false;
}


/**
  Read a variable length written by append_variable_length().

  @param[in] data  the buffer to read from
  @param[in] data_length  the maximum number of bytes to read from data
  @param[out] length  the length that was read
  @param[out] num  the number of bytes needed to represent the length
  @return  false on success, true on error
*/
static bool read_variable_length(const char *data, size_t data_length,
                                 size_t *length, size_t *num)
{
  /*
    It takes five bytes to represent UINT_MAX32, which is the largest
    supported length, so don't look any further.
  */
  const size_t max_bytes= std::min(data_length, static_cast<size_t>(5));

  size_t len= 0;
  for (size_t i= 0; i < max_bytes; i++)
  {
    // Get the next 7 bits of the length.
    len|= (data[i] & 0x7f) << (7 * i);
    if ((data[i] & 0x80) == 0)
    {
      // The length shouldn't exceed 32 bits.
      if (len > UINT_MAX32)
        return true;                          /* purecov: inspected */

      // This was the last byte. Return successfully.
      *num= i + 1;
      *length= len;
      return false;
    }
  }

  // No more available bytes. Return true to signal error.
  return true;                                /* purecov: inspected */
}


/**
  Check if the specified offset or size is too big to store in the
  binary JSON format.

  If the small storage format is used, the caller is expected to retry
  serialization in the large storage format, so no error is generated
  if the offset or size is too big. If the large storage format is
  used, an error will be generated if the offset or size is too big.

  @param offset_or_size  the offset or size to check
  @param large    if true, we are using the large storage format
    for JSON arrays and objects, which allows offsets and sizes that
    fit in a uint32; otherwise, we are using the small storage format,
    which allow offsets and sizes that fit in a uint16.
  @return true if offset_or_size is too big for the format, false
    otherwise
*/
static bool is_too_big_for_json(size_t offset_or_size, bool large)
{
  if (offset_or_size > UINT_MAX16)
  {
    if (!large)
      return true;

    if (offset_or_size > UINT_MAX32)
    {
      /* purecov: begin inspected */
      my_error(ER_JSON_VALUE_TOO_BIG, MYF(0));
      return true;
      /* purecov: end */
    }
  }

  return false;
}


/**
  Check if a value is small enough to be inlined in the value entry at the
  beginning of an object or an array.

  @param[in] value the JSON value
  @param[in] large true if the large storage format is used
  @param[out] inlined_val the numeric value to inline
  @param[out] inlined_type the type of the inlined value
  @return true if the value should be inlined, false otherwise
*/
static bool should_inline_value(const Json_dom *value, bool large,
                                int32 *inlined_val, uint8 *inlined_type)
{
  switch (value->json_type())
  {
  case Json_dom::J_NULL:
    *inlined_val= JSONB_NULL_LITERAL;
    *inlined_type= JSONB_TYPE_LITERAL;
    return true;
  case Json_dom::J_BOOLEAN:
    *inlined_val= (down_cast<const Json_boolean*>(value)->value()) ?
                JSONB_TRUE_LITERAL : JSONB_FALSE_LITERAL;
    *inlined_type= JSONB_TYPE_LITERAL;
    return true;
  case Json_dom::J_INT:
    {
      const Json_int *i= down_cast<const Json_int*>(value);
      if (i->is_16bit() || (large && i->is_32bit()))
      {
        *inlined_val= static_cast<int32>(i->value());
        *inlined_type= i->is_16bit() ? JSONB_TYPE_INT16 : JSONB_TYPE_INT32;
        return true;
      }
      return false;
    }
  case Json_dom::J_UINT:
    {
      const Json_uint *i= down_cast<const Json_uint*>(value);
      if (i->is_16bit() || (large && i->is_32bit()))
      {
        *inlined_val= static_cast<int32>(i->value());
        *inlined_type= i->is_16bit() ? JSONB_TYPE_UINT16 : JSONB_TYPE_UINT32;
        return true;
      }
      return false;
    }
  default:
    return false;
  }
}


/**
  Append a Json_dom value to the end of the destination buffer, and go
  back and update the value entry at the beginning of the parent array or
  object.

  @param dest the destination buffer
  @param value the value to append
  @param start_pos the position in the destination buffer where the
    parent array or object starts
  @param entry_pos the position in the destination buffer where the
    entry for the appended value is located
  @param large if true, the value is appended to a large array or object;
    otherwise, it is appended to a small array or object
  @param depth the current nesting level
  @return serialization status
*/
static enum_serialization_result
append_value(String *dest, const Json_dom *value, size_t start_pos,
             size_t entry_pos, bool large, size_t depth)
{
  if (depth >= JSON_DOCUMENT_MAX_DEPTH)
  {
    my_error(ER_JSON_DOCUMENT_TOO_DEEP, MYF(0));
    return FAILURE;
  }

  uint8 element_type;
  int32 inlined_value;
  if (should_inline_value(value, large, &inlined_value, &element_type))
  {
    (*dest)[entry_pos]= element_type;
    insert_offset_or_size(dest, entry_pos + 1, inlined_value, large);
    return OK;
  }

  size_t offset= dest->length() - start_pos;
  if (is_too_big_for_json(offset, large))
    return VALUE_TOO_BIG;

  insert_offset_or_size(dest, entry_pos + 1, offset, large);
  return serialize_json_value(value, entry_pos, dest, depth, !large);
}


/**
  Serialize a JSON array at the end of the destination string.

  @param array  the JSON array to serialize
  @param dest   the destination string
  @param large  if true, the large storage format will be used
  @param depth  the current nesting level
  @return serialization status
*/
static enum_serialization_result
serialize_json_array(const Json_array *array, String *dest, bool large,
                     size_t depth)
{
  const size_t start_pos= dest->length();
  const size_t size= array->size();

  if (is_too_big_for_json(size, large))
    return VALUE_TOO_BIG;

  // First write the number of elements in the array.
  if (append_offset_or_size(dest, size, large))
    return FAILURE;                             /* purecov: inspected */

  // Reserve space for the size of the array in bytes. To be filled in later.
  const size_t size_pos= dest->length();
  if (append_offset_or_size(dest, 0, large))
    return FAILURE;                             /* purecov: inspected */

  size_t entry_pos= dest->length();

  // Reserve space for the value entries at the beginning of the array.
  const size_t entry_size=
    large ? VALUE_ENTRY_SIZE_LARGE : VALUE_ENTRY_SIZE_SMALL;
  if (dest->fill(dest->length() + size * entry_size, 0))
    return FAILURE;                             /* purecov: inspected */

  for (uint32 i= 0; i < size; i++)
  {
    const Json_dom *elt= (*array)[i];
    enum_serialization_result res= append_value(dest, elt, start_pos,
                                                entry_pos, large, depth + 1);
    if (res != OK)
      return res;

    entry_pos+= entry_size;
  }

  // Finally, write the size of the object in bytes.
  size_t bytes= dest->length() - start_pos;
  if (is_too_big_for_json(bytes, large))
    return VALUE_TOO_BIG;                     /* purecov: inspected */
  insert_offset_or_size(dest, size_pos, bytes, large);

  return OK;
}


/**
  Serialize a JSON object at the end of the destination string.

  @param object the JSON object to serialize
  @param dest   the destination string
  @param large  if true, the large storage format will be used
  @param depth  the current nesting level
  @return serialization status
*/
static enum_serialization_result
serialize_json_object(const Json_object *object, String *dest, bool large,
                      size_t depth)
{
  const size_t start_pos= dest->length();
  const size_t size= object->cardinality();

  if (is_too_big_for_json(size, large))
    return VALUE_TOO_BIG;                       /* purecov: inspected */

  // First write the number of members in the object.
  if (append_offset_or_size(dest, size, large))
    return FAILURE;                             /* purecov: inspected */

  // Reserve space for the size of the object in bytes. To be filled in later.
  const size_t size_pos= dest->length();
  if (append_offset_or_size(dest, 0, large))
    return FAILURE;                             /* purecov: inspected */

  const size_t key_entry_size=
    large ? KEY_ENTRY_SIZE_LARGE : KEY_ENTRY_SIZE_SMALL;
  const size_t value_entry_size=
    large ? VALUE_ENTRY_SIZE_LARGE : VALUE_ENTRY_SIZE_SMALL;

  /*
    Calculate the offset of the first key relative to the start of the
    object. The first key comes right after the value entries.
  */
  size_t offset= dest->length() +
    size * (key_entry_size + value_entry_size) - start_pos;

#ifndef DBUG_OFF
  const std::string *prev_key= NULL;
#endif

  // Add the key entries.
  for (Json_object::const_iterator it= object->begin();
       it != object->end(); ++it)
  {
    const std::string *key= &it->first;
    size_t len= key->length();

#ifndef DBUG_OFF
    // Check that the DOM returns the keys in the correct order.
    if (prev_key)
    {
      DBUG_ASSERT(prev_key->length() <= len);
      if (len == prev_key->length())
        DBUG_ASSERT(memcmp(prev_key->data(), key->data(), len) < 0);
    }
    prev_key= key;
#endif

    // We only have two bytes for the key size. Check if the key is too big.
    if (len > UINT_MAX16)
    {
      my_error(ER_JSON_KEY_TOO_BIG, MYF(0));
      return FAILURE;
    }

    if (is_too_big_for_json(offset, large))
      return VALUE_TOO_BIG;                   /* purecov: inspected */

    if (append_offset_or_size(dest, offset, large) ||
        append_int16(dest, static_cast<int16>(len)))
      return FAILURE;                         /* purecov: inspected */
    offset+= len;
  }

  const size_t start_of_value_entries= dest->length();

  // Reserve space for the value entries. Will be filled in later.
  dest->fill(dest->length() + size * value_entry_size, 0);

  // Add the actual keys.
  for (Json_object::const_iterator it= object->begin(); it != object->end();
       ++it)
  {
    if (dest->append(it->first.c_str(), it->first.length()))
      return FAILURE;                         /* purecov: inspected */
  }

  // Add the values, and update the value entries accordingly.
  size_t entry_pos= start_of_value_entries;
  for (Json_object::const_iterator it= object->begin(); it != object->end();
       ++it)
  {
    enum_serialization_result res= append_value(dest, it->second,
                                                start_pos, entry_pos, large,
                                                depth + 1);
    if (res != OK)
      return res;
    entry_pos+= value_entry_size;
  }

  // Finally, write the size of the object in bytes.
  size_t bytes= dest->length() - start_pos;
  if (is_too_big_for_json(bytes, large))
    return VALUE_TOO_BIG;
  insert_offset_or_size(dest, size_pos, bytes, large);

  return OK;
}


/**
  Serialize a JSON value at the end of the destination string.

  Also go back and update the type specifier for the value to specify
  the correct type. For top-level documents, the type specifier is
  located in the byte right in front of the value. For documents that
  are nested within other documents, the type specifier is located in
  the value entry portion at the beginning of the parent document.

  @param dom       the JSON value to serialize
  @param type_pos  the position of the type specifier to update
  @param dest      the destination string
  @param depth     the current nesting level
  @param small_parent
                   tells if @a dom is contained in an array or object
                   which is stored in the small storage format
  @return          serialization status
*/
static enum_serialization_result
serialize_json_value(const Json_dom *dom, size_t type_pos, String *dest,
                     size_t depth, bool small_parent)
{
  const size_t start_pos= dest->length();
  DBUG_ASSERT(type_pos < start_pos);

  enum_serialization_result result;

  switch (dom->json_type())
  {
  case Json_dom::J_ARRAY:
    {
      const Json_array *array= down_cast<const Json_array*>(dom);
      (*dest)[type_pos]= JSONB_TYPE_SMALL_ARRAY;
      result= serialize_json_array(array, dest, false, depth);
      /*
        If the array was too large to fit in the small storage format,
        reset the destination buffer and retry with the large storage
        format.

        Possible future optimization: Analyze size up front and pick the
        correct format on the first attempt, so that we don't have to
        redo parts of the serialization.
      */
      if (result == VALUE_TOO_BIG)
      {
        // If the parent uses the small storage format, it needs to grow too.
        if (small_parent)
          return VALUE_TOO_BIG;
        dest->length(start_pos);
        (*dest)[type_pos]= JSONB_TYPE_LARGE_ARRAY;
        result= serialize_json_array(array, dest, true, depth);
      }
      break;
    }
  case Json_dom::J_OBJECT:
    {
      const Json_object *object= down_cast<const Json_object*>(dom);
      (*dest)[type_pos]= JSONB_TYPE_SMALL_OBJECT;
      result= serialize_json_object(object, dest, false, depth);
      /*
        If the object was too large to fit in the small storage format,
        reset the destination buffer and retry with the large storage
        format.

        Possible future optimization: Analyze size up front and pick the
        correct format on the first attempt, so that we don't have to
        redo parts of the serialization.
      */
      if (result == VALUE_TOO_BIG)
      {
        // If the parent uses the small storage format, it needs to grow too.
        if (small_parent)
          return VALUE_TOO_BIG;
        dest->length(start_pos);
        (*dest)[type_pos]= JSONB_TYPE_LARGE_OBJECT;
        result= serialize_json_object(object, dest, true, depth);
      }
      break;
    }
  case Json_dom::J_STRING:
    {
      const Json_string *jstr= down_cast<const Json_string*>(dom);
      size_t size= jstr->size();
      if (append_variable_length(dest, size) ||
          dest->append(jstr->value().c_str(), size))
        return FAILURE;                       /* purecov: inspected */
      (*dest)[type_pos]= JSONB_TYPE_STRING;
      result= OK;
      break;
    }
  case Json_dom::J_INT:
    {
      const Json_int *i= down_cast<const Json_int*>(dom);
      longlong val= i->value();
      if (i->is_16bit())
      {
        if (append_int16(dest, static_cast<int16>(val)))
          return FAILURE;                     /* purecov: inspected */
        (*dest)[type_pos]= JSONB_TYPE_INT16;
      }
      else if (i->is_32bit())
      {
        if (append_int32(dest, static_cast<int32>(val)))
          return FAILURE;                     /* purecov: inspected */
        (*dest)[type_pos]= JSONB_TYPE_INT32;
      }
      else
      {
        if (append_int64(dest, val))
          return FAILURE;                     /* purecov: inspected */
        (*dest)[type_pos]= JSONB_TYPE_INT64;
      }
      result= OK;
      break;
    }
  case Json_dom::J_UINT:
    {
      const Json_uint *i= down_cast<const Json_uint*>(dom);
      ulonglong val= i->value();
      if (i->is_16bit())
      {
        if (append_int16(dest, static_cast<int16>(val)))
          return FAILURE;                     /* purecov: inspected */
        (*dest)[type_pos]= JSONB_TYPE_UINT16;
      }
      else if (i->is_32bit())
      {
        if (append_int32(dest, static_cast<int32>(val)))
          return FAILURE;                     /* purecov: inspected */
        (*dest)[type_pos]= JSONB_TYPE_UINT32;
      }
      else
      {
        if (append_int64(dest, val))
          return FAILURE;                     /* purecov: inspected */
        (*dest)[type_pos]= JSONB_TYPE_UINT64;
      }
      result= OK;
      break;
    }
  case Json_dom::J_DOUBLE:
    {
      // Store the double in a platform-independent eight-byte format.
      const Json_double *d= down_cast<const Json_double*>(dom);
      if (dest->reserve(8))
        return FAILURE;                       /* purecov: inspected */
      float8store(const_cast<char *>(dest->ptr()) + dest->length(), d->value());
      dest->length(dest->length() + 8);
      (*dest)[type_pos]= JSONB_TYPE_DOUBLE;
      result= OK;
      break;
    }
  case Json_dom::J_NULL:
    if (dest->append(JSONB_NULL_LITERAL))
      return FAILURE;                         /* purecov: inspected */
    (*dest)[type_pos]= JSONB_TYPE_LITERAL;
    result= OK;
    break;
  case Json_dom::J_BOOLEAN:
    {
      char c= (down_cast<const Json_boolean*>(dom)->value()) ?
        JSONB_TRUE_LITERAL : JSONB_FALSE_LITERAL;
      if (dest->append(c))
        return FAILURE;                       /* purecov: inspected */
      (*dest)[type_pos]= JSONB_TYPE_LITERAL;
      result= OK;
      break;
    }
  case Json_dom::J_OPAQUE:
    {
      const Json_opaque *o= down_cast<const Json_opaque*>(dom);
      if (dest->append(static_cast<char>(o->type())) ||
          append_variable_length(dest, o->size()) ||
          dest->append(o->value(), o->size()))
        return FAILURE;                       /* purecov: inspected */
      (*dest)[type_pos]= JSONB_TYPE_OPAQUE;
      result= OK;
      break;
    }
  case Json_dom::J_DECIMAL:
    {
      // Store DECIMALs as opaque values.
      const Json_decimal *jd= down_cast<const Json_decimal*>(dom);
      const int bin_size= jd->binary_size();
      char buf[Json_decimal::MAX_BINARY_SIZE];
      if (jd->get_binary(buf))
        return FAILURE;                       /* purecov: inspected */
      Json_opaque o(MYSQL_TYPE_NEWDECIMAL, buf, bin_size);
      result= serialize_json_value(&o, type_pos, dest, depth, small_parent);
      break;
    }
  case Json_dom::J_DATETIME:
  case Json_dom::J_DATE:
  case Json_dom::J_TIME:
  case Json_dom::J_TIMESTAMP:
    {
      // Store datetime as opaque values.
      const Json_datetime *jdt= down_cast<const Json_datetime*>(dom);
      char buf[Json_datetime::PACKED_SIZE];
      jdt->to_packed(buf);
      Json_opaque o(jdt->field_type(), buf, Json_datetime::PACKED_SIZE);
      result= serialize_json_value(&o, type_pos, dest, depth, small_parent);
      break;
    }
  default:
    /* purecov: begin deadcode */
    DBUG_ABORT();
    my_error(ER_INTERNAL_ERROR, MYF(0), "JSON serialization failed");
    return FAILURE;
    /* purecov: end */
  }

  if (result == OK &&
      dest->length() > current_thd->variables.max_allowed_packet)
  {
    my_error(ER_WARN_ALLOWED_PACKET_OVERFLOWED, MYF(0),
             "json_binary::serialize",
             current_thd->variables.max_allowed_packet);
    return FAILURE;
  }

  return result;
}


// Constructor for literals and errors.
Value::Value(enum_type t)
  : m_type(t), m_field_type(), m_data(), m_element_count(), m_length(),
    m_int_value(), m_double_value(), m_large()
{
  DBUG_ASSERT(t == LITERAL_NULL || t == LITERAL_TRUE || t == LITERAL_FALSE ||
              t == ERROR);
}


// Constructor for int and uint.
Value::Value(enum_type t, int64 val)
  : m_type(t), m_field_type(), m_data(), m_element_count(), m_length(),
    m_int_value(val), m_double_value(), m_large()
{
  DBUG_ASSERT(t == INT || t == UINT);
}


// Constructor for double.
Value::Value(double d)
  : m_type(DOUBLE), m_field_type(), m_data(), m_element_count(), m_length(),
    m_int_value(), m_double_value(d), m_large()
{}


// Constructor for string.
Value::Value(const char *data, size_t len)
  : m_type(STRING), m_field_type(), m_data(data), m_element_count(),
    m_length(len), m_int_value(), m_double_value(), m_large()
{}


// Constructor for arrays and objects.
Value::Value(enum_type t, const char *data, size_t bytes,
             size_t element_count, bool large)
  : m_type(t), m_field_type(), m_data(data), m_element_count(element_count),
    m_length(bytes), m_int_value(), m_double_value(), m_large(large)
{
  DBUG_ASSERT(t == ARRAY || t == OBJECT);
}


// Constructor for opaque values.
Value::Value(enum_field_types ft, const char *data, size_t len)
  : m_type(OPAQUE), m_field_type(ft), m_data(data), m_element_count(),
    m_length(len), m_int_value(), m_double_value(), m_large()
{}


bool Value::is_valid() const
{
  switch (m_type)
  {
  case ERROR:
    return false;
  case ARRAY:
    // Check that all the array elements are valid.
    for (size_t i= 0; i < element_count(); i++)
      if (!element(i).is_valid())
        return false;                         /* purecov: inspected */
    return true;
  case OBJECT:
    {
      /*
        Check that all keys and values are valid, and that the keys come
        in the correct order.
      */
      const char *prev_key= NULL;
      size_t prev_key_len= 0;
      for (size_t i= 0; i < element_count(); i++)
      {
        Value k= key(i);
        if (!k.is_valid() || !element(i).is_valid())
          return false;                       /* purecov: inspected */
        const char *curr_key= k.get_data();
        size_t curr_key_len= k.get_data_length();
        if (i > 0)
        {
          if (prev_key_len > curr_key_len)
            return false;                     /* purecov: inspected */
          if (prev_key_len == curr_key_len &&
              (memcmp(prev_key, curr_key, curr_key_len) >= 0))
            return false;                     /* purecov: inspected */
        }
        prev_key= curr_key;
        prev_key_len= curr_key_len;
      }
      return true;
    }
  default:
    // This is a valid scalar value.
    return true;
  }
}


/**
  Get a pointer to the beginning of the STRING or OPAQUE data
  represented by this instance.
*/
const char *Value::get_data() const
{
  DBUG_ASSERT(m_type == STRING || m_type == OPAQUE);
  return m_data;
}


/**
  Get the length in bytes of the STRING or OPAQUE value represented by
  this instance.
*/
size_t Value::get_data_length() const
{
  DBUG_ASSERT(m_type == STRING || m_type == OPAQUE);
  return m_length;
}


/**
  Get the value of an INT.
*/
int64 Value::get_int64() const
{
  DBUG_ASSERT(m_type == INT);
  return m_int_value;
}


/**
  Get the value of a UINT.
*/
uint64 Value::get_uint64() const
{
  DBUG_ASSERT(m_type == UINT);
  return static_cast<uint64>(m_int_value);
}


/**
  Get the value of a DOUBLE.
*/
double Value::get_double() const
{
  DBUG_ASSERT(m_type == DOUBLE);
  return m_double_value;
}


/**
  Get the number of elements in an array, or the number of members in
  an object.
*/
size_t Value::element_count() const
{
  DBUG_ASSERT(m_type == ARRAY || m_type == OBJECT);
  return m_element_count;
}


/**
  Get the MySQL field type of an opaque value. Identifies the type of
  the value stored in the data portion of an opaque value.
*/
enum_field_types Value::field_type() const
{
  DBUG_ASSERT(m_type == OPAQUE);
  return m_field_type;
}


/**
  Create a Value object that represents an error condition.
*/
static Value err()
{
  return Value(Value::ERROR);
}


/**
  Parse a JSON scalar value.

  @param type   the binary type of the scalar
  @param data   pointer to the start of the binary representation of the scalar
  @param len    the maximum number of bytes to read from data
  @return  an object that represents the scalar value
*/
static Value parse_scalar(uint8 type, const char *data, size_t len)
{
  switch (type)
  {
  case JSONB_TYPE_LITERAL:
    if (len < 1)
      return err();                           /* purecov: inspected */
    switch (static_cast<uint8>(*data))
    {
    case JSONB_NULL_LITERAL:
      return Value(Value::LITERAL_NULL);
    case JSONB_TRUE_LITERAL:
      return Value(Value::LITERAL_TRUE);
    case JSONB_FALSE_LITERAL:
      return Value(Value::LITERAL_FALSE);
    default:
      return err();                           /* purecov: inspected */
    }
  case JSONB_TYPE_INT16:
    if (len < 2)
      return err();                           /* purecov: inspected */
    return Value(Value::INT, sint2korr(data));
  case JSONB_TYPE_INT32:
    if (len < 4)
      return err();                           /* purecov: inspected */
    return Value(Value::INT, sint4korr(data));
  case JSONB_TYPE_INT64:
    if (len < 8)
      return err();                           /* purecov: inspected */
    return Value(Value::INT, sint8korr(data));
  case JSONB_TYPE_UINT16:
    if (len < 2)
      return err();                           /* purecov: inspected */
    return Value(Value::UINT, uint2korr(data));
  case JSONB_TYPE_UINT32:
    if (len < 4)
      return err();                           /* purecov: inspected */
    return Value(Value::UINT, uint4korr(data));
  case JSONB_TYPE_UINT64:
    if (len < 8)
      return err();                           /* purecov: inspected */
    return Value(Value::UINT, uint8korr(data));
  case JSONB_TYPE_DOUBLE:
    {
      if (len < 8)
        return err();                         /* purecov: inspected */
      double d;
      float8get(&d, data);
      return Value(d);
    }
  case JSONB_TYPE_STRING:
    {
      size_t str_len;
      size_t n;
      if (read_variable_length(data, len, &str_len, &n))
        return err();                         /* purecov: inspected */
      if (len < n + str_len)
        return err();                         /* purecov: inspected */
      return Value(data + n, str_len);
    }
  case JSONB_TYPE_OPAQUE:
    {
      /*
        There should always be at least one byte, which tells the field
        type of the opaque value.
      */
      if (len < 1)
        return err();                         /* purecov: inspected */

      // The type is encoded as a uint8 that maps to an enum_field_types.
      uint8 type_byte= static_cast<uint8>(*data);
      enum_field_types field_type= static_cast<enum_field_types>(type_byte);

      // Then there's the length of the value.
      size_t val_len;
      size_t n;
      if (read_variable_length(data + 1, len - 1, &val_len, &n))
        return err();                         /* purecov: inspected */
      if (len < 1 + n + val_len)
        return err();                         /* purecov: inspected */
      return Value(field_type, data + 1 + n, val_len);
    }
  default:
    // Not a valid scalar type.
    return err();
  }
}


/**
  Read an offset or size field from a buffer. The offset could be either
  a two byte unsigned integer or a four byte unsigned integer.

  @param data  the buffer to read from
  @param large tells if the large or small storage format is used; true
               means read four bytes, false means read two bytes
*/
static size_t read_offset_or_size(const char *data, bool large)
{
  return large ? uint4korr(data) : uint2korr(data);
}


/**
  Parse a JSON array or object.

  @param t      type (either ARRAY or OBJECT)
  @param data   pointer to the start of the array or object
  @param len    the maximum number of bytes to read from data
  @param large  if true, the array or object is stored using the large
                storage format; otherwise, it is stored using the small
                storage format
  @return  an object that allows access to the array or object
*/
static Value parse_array_or_object(Value::enum_type t, const char *data,
                                   size_t len, bool large)
{
  DBUG_ASSERT(t == Value::ARRAY || t == Value::OBJECT);

  /*
    Make sure the document is long enough to contain the two length fields
    (both number of elements or members, and number of bytes).
  */
  const size_t offset_size= large ? LARGE_OFFSET_SIZE : SMALL_OFFSET_SIZE;
  if (len < 2 * offset_size)
    return err();
  const size_t element_count= read_offset_or_size(data, large);
  const size_t bytes= read_offset_or_size(data + offset_size, large);

  // The value can't have more bytes than what's available in the data buffer.
  if (bytes > len)
    return err();

  /*
    Calculate the size of the header. It consists of:
    - two length fields
    - if it is a JSON object, key entries with pointers to where the keys
      are stored
    - value entries with pointers to where the actual values are stored
  */
  size_t header_size= 2 * offset_size;
  if (t == Value::OBJECT)
    header_size+= element_count *
      (large ? KEY_ENTRY_SIZE_LARGE : KEY_ENTRY_SIZE_SMALL);
  header_size+= element_count *
    (large ? VALUE_ENTRY_SIZE_LARGE : VALUE_ENTRY_SIZE_SMALL);

  // The header should not be larger than the full size of the value.
  if (header_size > bytes)
    return err();                             /* purecov: inspected */

  return Value(t, data, bytes, element_count, large);
}


/**
  Parse a JSON value within a larger JSON document.

  @param type   the binary type of the value to parse
  @param data   pointer to the start of the binary representation of the value
  @param len    the maximum number of bytes to read from data
  @return  an object that allows access to the value
*/
static Value parse_value(uint8 type, const char *data, size_t len)
{
  switch (type)
  {
  case JSONB_TYPE_SMALL_OBJECT:
    return parse_array_or_object(Value::OBJECT, data, len, false);
  case JSONB_TYPE_LARGE_OBJECT:
    return parse_array_or_object(Value::OBJECT, data, len, true);
  case JSONB_TYPE_SMALL_ARRAY:
    return parse_array_or_object(Value::ARRAY, data, len, false);
  case JSONB_TYPE_LARGE_ARRAY:
    return parse_array_or_object(Value::ARRAY, data, len, true);
  default:
    return parse_scalar(type, data, len);
  }
}


Value parse_binary(const char *data, size_t len)
{
  // Each document should start with a one-byte type specifier.
  if (len < 1)
    return err();                             /* purecov: inspected */

  return parse_value(data[0], data + 1, len - 1);
}


/**
  Get the element at the specified position of a JSON array or a JSON
  object. When called on a JSON object, it returns the value
  associated with the key returned by key(pos).

  @param pos  the index of the element
  @return a value representing the specified element, or a value where
  type() returns ERROR if pos does not point to an element
*/
Value Value::element(size_t pos) const
{
  DBUG_ASSERT(m_type == ARRAY || m_type == OBJECT);

  if (pos >= m_element_count)
    return err();

  /*
    Value entries come after the two length fields if it's an array, or
    after the two length fields and all the key entries if it's an object.
  */
  size_t first_entry_offset=
    2 * (m_large ? LARGE_OFFSET_SIZE : SMALL_OFFSET_SIZE);
  if (type() == OBJECT)
    first_entry_offset+=
      m_element_count * (m_large ? KEY_ENTRY_SIZE_LARGE : KEY_ENTRY_SIZE_SMALL);

  const size_t entry_size=
    m_large ? VALUE_ENTRY_SIZE_LARGE : VALUE_ENTRY_SIZE_SMALL;
  const size_t entry_offset= first_entry_offset + entry_size * pos;

  uint8 type= m_data[entry_offset];

  /*
    Check if this is an inlined scalar value. If so, return it.
    The scalar will be inlined just after the byte that identifies the
    type, so it's found on entry_offset + 1.
  */
  if (type == JSONB_TYPE_INT16 || type == JSONB_TYPE_UINT16 ||
      type == JSONB_TYPE_LITERAL ||
      (m_large && (type == JSONB_TYPE_INT32 || type == JSONB_TYPE_UINT32)))
    return parse_scalar(type, m_data + entry_offset + 1, entry_size - 1);

  /*
    Otherwise, it's a non-inlined value, and the offset to where the value
    is stored, can be found right after the type byte in the entry.
  */
  size_t value_offset= read_offset_or_size(m_data + entry_offset + 1, m_large);

  if (m_length < value_offset)
    return err();                             /* purecov: inspected */

  return parse_value(type, m_data + value_offset, m_length - value_offset);
}


/**
  Get the key of the member stored at the specified position in a JSON
  object.

  @param pos  the index of the member
  @return the key of the specified member, or a value where type()
  returns ERROR if pos does not point to a member
*/
Value Value::key(size_t pos) const
{
  DBUG_ASSERT(m_type == OBJECT);

  if (pos >= m_element_count)
    return err();

  const size_t offset_size= m_large ? LARGE_OFFSET_SIZE : SMALL_OFFSET_SIZE;
  const size_t key_entry_size=
    m_large ? KEY_ENTRY_SIZE_LARGE : KEY_ENTRY_SIZE_SMALL;
  const size_t value_entry_size=
    m_large ? VALUE_ENTRY_SIZE_LARGE : VALUE_ENTRY_SIZE_SMALL;

  // The key entries are located after two length fields of size offset_size.
  const size_t entry_offset= 2 * offset_size + key_entry_size * pos;

  // The offset of the key is the first part of the key entry.
  const size_t key_offset= read_offset_or_size(m_data + entry_offset, m_large);

  // The length of the key is the second part of the entry, always two bytes.
  const size_t key_length= uint2korr(m_data + entry_offset + offset_size);

  /*
    The key must start somewhere after the last value entry, and it must
    end before the end of the m_data buffer.
  */
  if ((key_offset < entry_offset +
                    (m_element_count - pos) * key_entry_size +
                    m_element_count * value_entry_size) ||
      (m_length < key_offset + key_length))
    return err();                             /* purecov: inspected */

  return Value(m_data + key_offset, key_length);
}


/**
  Get the value associated with the specified key in a JSON object.

  @param[in] key  pointer to the key
  @param[in] len  length of the key
  @return the value associated with the key, if there is one. otherwise,
  returns ERROR
*/
Value Value::lookup(const char *key, size_t len) const
{
  DBUG_ASSERT(m_type == OBJECT);

  const size_t offset_size=
    (m_large ? LARGE_OFFSET_SIZE : SMALL_OFFSET_SIZE);

  const size_t entry_size=
    (m_large ? KEY_ENTRY_SIZE_LARGE : KEY_ENTRY_SIZE_SMALL);

  // The first key entry is located right after the two length fields.
  const size_t first_entry_offset= 2 * offset_size;

  size_t lo= 0U;                // lower bound for binary search (inclusive)
  size_t hi= m_element_count;   // upper bound for binary search (exclusive)

  while (lo < hi)
  {
    // Find the entry in the middle of the search interval.
    size_t idx= (lo + hi) / 2;
    size_t entry_offset= first_entry_offset + idx * entry_size;

    // Keys are ordered on length, so check length first.
    size_t key_len= uint2korr(m_data + entry_offset + offset_size);
    if (len > key_len)
      lo= idx + 1;
    else if (len < key_len)
      hi= idx;
    else
    {
      // The keys had the same length, so compare their contents.
      size_t key_offset= read_offset_or_size(m_data + entry_offset, m_large);

      int cmp= memcmp(key, m_data + key_offset, len);
      if (cmp > 0)
        lo= idx + 1;
      else if (cmp < 0)
        hi= idx;
      else
        return element(idx);
    }
  }

  return err();
}


/**
  Copy the binary representation of this value into a buffer,
  replacing the contents of the receiving buffer.

  @param buf  the receiving buffer
  @return false on success, true otherwise
*/
bool Value::raw_binary(String *buf) const
{
  // Reset the buffer.
  buf->length(0);
  buf->set_charset(&my_charset_bin);

  switch (m_type)
  {
  case OBJECT:
  case ARRAY:
    {
      char tp= m_large ?
        (m_type == OBJECT ? JSONB_TYPE_LARGE_OBJECT : JSONB_TYPE_LARGE_ARRAY) :
        (m_type == OBJECT ? JSONB_TYPE_SMALL_OBJECT : JSONB_TYPE_SMALL_ARRAY);
      return buf->append(tp) || buf->append(m_data, m_length);
    }
  case STRING:
    return buf->append(JSONB_TYPE_STRING) ||
      append_variable_length(buf, m_length) ||
      buf->append(m_data, m_length);
  case INT:
    {
      Json_int i(get_int64());
      return serialize(&i, buf) != OK;
    }
  case UINT:
    {
      Json_uint i(get_uint64());
      return serialize(&i, buf) != OK;
    }
  case DOUBLE:
    {
      Json_double d(get_double());
      return serialize(&d, buf) != OK;
    }
  case LITERAL_NULL:
    {
      Json_null n;
      return serialize(&n, buf) != OK;
    }
  case LITERAL_TRUE:
  case LITERAL_FALSE:
    {
      Json_boolean b(m_type == LITERAL_TRUE);
      return serialize(&b, buf) != OK;
    }
  case OPAQUE:
    return buf->append(JSONB_TYPE_OPAQUE) ||
      buf->append(field_type()) ||
      append_variable_length(buf, m_length) ||
      buf->append(m_data, m_length);
  case ERROR:
    break;                                    /* purecov: inspected */
  }

  /* purecov: begin deadcode */
  DBUG_ABORT();
  return true;
  /* purecov: end */
}



} // end namespace json_binary
