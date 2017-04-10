#ifndef JSON_DOM_INCLUDED
#define JSON_DOM_INCLUDED

/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <stddef.h>
#include <functional>
#include <map>
#include <new>
#include <string>
#include <type_traits>          // is_base_of
#include <utility>

#include "binary_log_types.h"   // enum_field_types
#include "json_binary.h"        // json_binary::Value
#include "malloc_allocator.h"   // Malloc_allocator
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_decimal.h"         // my_decimal
#include "my_inttypes.h"
#include "mysql_time.h"         // MYSQL_TIME
#include "prealloced_array.h"   // Prealloced_array
#include "sql_alloc.h"          // Sql_alloc

class Json_dom;
class Json_path;
class Json_path_leg;
class Json_seekable_path;
class Json_wrapper;
class String;
class THD;

typedef Prealloced_array<Json_wrapper, 16> Json_wrapper_vector;
typedef Prealloced_array<Json_dom *, 16> Json_dom_vector;

/// The maximum number of nesting levels allowed in a JSON document.
#define JSON_DOCUMENT_MAX_DEPTH 100

/**
  @file sql/json_dom.h
  JSON DOM.

  When a JSON value is retrieved from a column, a prior it exists in
  a binary form, cf. Json_binary::Value class.

  However, when we need to manipulate the JSON values we mostly convert them
  from binary form to a structured in-memory from called DOM (from domain
  object model) which uses a recursive tree representation of the JSON value
  corresponding closely to a parse tree. This form is more suitable for
  manipulation.

  The JSON type is mostly represented internally as a Json_wrapper which hides
  if the representation is a binary or DOM one. This makes is possible to avoid
  building a DOM unless we really need one.

  The file defines two sets of classes: a) The Json_dom hierarchy and
  b) Json_wrapper and its companion class Json_wrapper_object_iterator.
  For both sets, arrays are traversed using an operator[].
*/

/**
  Json values in MySQL comprises the stand set of JSON values plus a
  MySQL specific set. A Json _number_ type is subdivided into _int_,
  _uint_, _double_ and _decimal_.

  MySQL also adds four built-in date/time values: _date_, _time_,
  _datetime_ and _timestamp_.  An additional _opaque_ value can
  store any other MySQL type.

  The enumeration is common to Json_dom and Json_wrapper.

  The enumeration is also used by Json_wrapper::compare() to
  determine the ordering when comparing values of different types,
  so the order in which the values are defined in the enumeration,
  is significant. The expected order is null < number < string <
  object < array < boolean < date < time < datetime/timestamp <
  opaque.
*/
enum class enum_json_type
{
  J_NULL,
  J_DECIMAL,
  J_INT,
  J_UINT,
  J_DOUBLE,
  J_STRING,
  J_OBJECT,
  J_ARRAY,
  J_BOOLEAN,
  J_DATE,
  J_TIME,
  J_DATETIME,
  J_TIMESTAMP,
  J_OPAQUE,
  J_ERROR
};

/**
  JSON DOM abstract base class.

  MySQL representation of in-memory JSON objects used by the JSON type
  Supports access, deep cloning, and updates. See also Json_wrapper and
  json_binary::Value.
  Uses heap for space allocation for now. FIXME.

  Class hierarchy:
  <code><pre>
      Json_dom (abstract)
       Json_scalar (abstract)
         Json_string
         Json_number (abstract)
           Json_decimal
           Json_int
           Json_uint
           Json_double
         Json_boolean
         Json_null
         Json_datetime
         Json_opaque
       Json_object
       Json_array
  </pre></code>
  At the outset, object and array add/insert/append operations takes
  a clone unless specified in the method, e.g. add_alias hands the
  responsibility for the passed in object over to the object.
*/
class Json_dom
{
  // so that these classes can call set_parent()
  friend class Json_object;
  friend class Json_array;
protected:
  Json_dom() : m_parent(NULL) {}

  /**
    Set the parent dom to which this dom is attached.

    @param[in] parent the parent we're being attached to
  */
  void set_parent(Json_dom *parent) { m_parent= parent; }

public:
  virtual ~Json_dom() {}

  /**
    Allocate space on the heap for a Json_dom object.

    @return pointer to the allocated memory, or NULL if memory could
    not be allocated (in which case my_error() will have been called
    with the appropriate error message)
  */
  void *operator new(size_t size, const std::nothrow_t&) throw();

  /**
    Deallocate the space used by a Json_dom object.
  */
  void operator delete(void *ptr) throw();

  /**
    Nothrow delete.
  */
  void operator delete(void *ptr, const std::nothrow_t&) throw();

  /**
    Get the parent dom to which this dom is attached.

    @return the parent dom.
  */
  Json_dom *parent() const { return m_parent; }

  /**
    @return the type corresponding to the actual Json_dom subclass
  */
  virtual enum_json_type json_type() const= 0;

  /**
    @return true if the object is a subclass of Json_scalar
  */
  virtual bool is_scalar() const { return false; }

  /**
    @return true of the object is a subclass of Json_number
  */
  virtual bool is_number() const { return false; }

  /**
    @return depth of the DOM. "abc", [] and {} have depth 1. ["abc"] and
           {"a": "abc"} have depth 2.
  */
  virtual uint32 depth() const= 0;

  /**
    Make a deep clone. The ownership of the returned object is
    henceforth with the caller.

    @return a cloned Json_dom object.
  */
  virtual Json_dom *clone() const= 0;

  /**
    Parse Json text to DOM (using rapidjson). The text must be valid JSON.
    The results when supplying an invalid document is undefined.
    The ownership of the returned object is henceforth with the caller.

    If the parsing fails because of a syntax error, the errmsg and
    offset arguments will be given values that point to a detailed
    error message and where the syntax error was located. The caller
    will have to generate an error message with my_error() in this
    case.

    If the parsing fails because of some other error (such as out of
    memory), errmsg will point to a location that holds the value
    NULL. In this case, parse() will already have called my_error(),
    and the caller doesn't need to generate an error message.

    @param[in]  text   the JSON text
    @param[in]  length the length of the text
    @param[out] errmsg any syntax error message (will be ignored if it is NULL)
    @param[out] offset the position in the parsed string a syntax error was
                       found (will be ignored if it is NULL)
    @param[in]  handle_numbers_as_double whether numbers should be handled as
                                         double. If set to TRUE, all numbers are
                                         parsed as DOUBLE

    @result the built DOM if JSON text was parseable, else NULL
  */
  static Json_dom *parse(const char *text, size_t length,
                         const char **errmsg, size_t *offset,
                         bool handle_numbers_as_double= false);

  /**
    Construct a DOM object based on a binary JSON value. The ownership
    of the returned object is henceforth with the caller.

    @param thd  current session
    @param v    the binary value to parse
    @return a DOM representation of the binary value, or NULL on error
  */
  static Json_dom* parse(const THD *thd, const json_binary::Value &v);

  /**
    Replace oldv contained inside this container array or object) with
    newv.
    If this container does not contain oldv, calling the method is
    a no-op. Do not call this method on a DOM object which is not a container.

    @param[in] oldv the value to be replace
    @param[in,out] newv the new value to put in the container
  */
  /* purecov: begin deadcode */
  virtual void replace_dom_in_container(Json_dom *oldv MY_ATTRIBUTE((unused)),
                                        Json_dom *newv MY_ATTRIBUTE((unused)))
  {
    /*
      Array and object should override this method. Not expected to be
      called on other DOM objects.
    */
    DBUG_ABORT();
  }
  /* purecov: end */

  /**
    Get the path location of this dom, measured from the outermost
    document it nests inside.
  */
  Json_path get_location();

  /**
    Finds all of the json sub-documents which match the path expression.
    Adds a vector element for each match.

    See the header comment for Json_wrapper.seek() for a discussion
    of complexities involving path expression with more than one
    ellipsis (**) token.

    @param[in]  path  the (possibly wildcarded) address of the sub-documents
    @param[out] hits  one element per match
    @param[in]  auto_wrap
                      if true, match a tailing [0] to scalar at that position.
    @param[in]  only_need_one True if we can stop after finding one match
    @return false on success, true on error
  */
  bool seek(const Json_seekable_path &path,
            Json_dom_vector *hits, bool auto_wrap,
            bool only_need_one);

private:

  /** Parent pointer */
  Json_dom *m_parent;

  /**
     Return the child Json_doms identified by the given path leg.
     The child doms are added to a vector.

     See the header comment for Json_wrapper.seek() for a discussion
     of complexities involving path expressions with more than one
     ellipsis (**) token.

   @param[in]     path_leg identifies the child
   @param[in]     auto_wrap if true, match final scalar with [0] is need be
   @param[in]     only_need_one True if we can stop after finding one match
   @param[in,out] duplicates helps to identify duplicate arrays and objects
                  introduced by daisy-chained ** tokens
   @param[in,out] result the vector of qualifying children
   @return false on success, true on error
  */
  bool find_child_doms(const Json_path_leg *path_leg,
                       bool auto_wrap,
                       bool only_need_one,
                       Json_dom_vector *duplicates,
                       Json_dom_vector *result);
};


/**
  A comparator that is used for ordering keys in a Json_object. It
  orders the keys on length, and lexicographically if the keys have
  the same length. The ordering is ascending. This ordering was chosen
  for speed of look-up. See usage in Json_object_map.
*/
struct Json_key_comparator
  : std::binary_function<std::string, std::string, bool>
{
  bool operator() (const std::string &key1, const std::string &key2) const;
};


/**
  A type used to hold JSON object elements in a map, see the
  Json_object class.
*/
typedef std::map<std::string, Json_dom *, Json_key_comparator,
  Malloc_allocator<std::pair<const std::string, Json_dom *> > > Json_object_map;

/**
  Represents a JSON container value of type "object" (ECMA), type
  J_OBJECT here.
*/
class Json_object : public Json_dom
{
  friend class Json_wrapper;
private:
  /**
    Map to hold the object elements.
  */
  Json_object_map m_map;
public:
  Json_object();
  ~Json_object();
  enum_json_type json_type() const { return enum_json_type::J_OBJECT; }

  /**
    Add a clone of the value to the object iff the key isn't already set.  If
    it is set, the value is not modified by this call ("first value wins").

    @param[in]  key    the JSON element key of to be added
    @param[in]  value  a JSON value: the element key's value
    @retval false on success
    @retval true on failure
  */
  bool add_clone(const std::string &key, const Json_dom *value);

  /**
    Add the value to the object iff they key isn't already set.

    Ownership of the value is effectively transferred to the
    object and the value will be deallocated by the object so only add
    values that can be deallocated safely (no stack variables please!)

    @param[in]  key    the JSON key of to be added
    @param[in]  value  a JSON value: the key's value
    @retval false on success
    @retval true on failure
  */
  bool add_alias(const std::string &key, Json_dom *value); // no clone

  /**
    Transfer all of the key/value pairs in the other object into this
    object. The other object is deleted. If this object and the other
    object share a key, then the two values of the key are merged.

    @param [in] other    a pointer to the object which will be consumed
    @retval false on success
    @retval true on failure
  */
  bool consume(Json_object *other);

  /**
    Return the value at key. The value is not cloned, so make
    one if you need it. Do not delete the returned value, please!
    If the key is not present, return a null pointer.

    @param[in]  key the key of the element whose value we want
    @return the value associated with the key, or NULL if the key is not found
  */
  Json_dom *get(const std::string &key) const;

  /**
    Remove the child element addressed by key. The removed child is deleted.

    @param[in]  child the child to remove

    @return true If that really was a child of this object.
  */
  bool remove(const Json_dom *child);

  /**
    @return The number of elements in the JSON object.
  */
  size_t cardinality() const;

  // See base class documentation.
  uint32 depth() const;

  // See base class documentation.
  Json_dom *clone() const;

  // See base class documentation.
  void replace_dom_in_container(Json_dom *oldv, Json_dom *newv);

  /**
    Remove all elements in the object.
  */
  void clear();

  /**
    Constant iterator over the elements in the JSON object. Each
    element is represented as a std::pair where first is a std::string
    that represents the key name, and second is a pointer to a
    Json_dom that represents the value.
  */
  typedef Json_object_map::const_iterator const_iterator;

  /// Returns a const_iterator that refers to the first element.
  const_iterator begin() const { return m_map.begin(); }

  /// Returns a const_iterator that refers past the last element.
  const_iterator end() const { return m_map.end(); }
};


/**
  Represents a JSON array container, i.e. type J_ARRAY here.
*/
class Json_array : public Json_dom
{
private:
  Json_dom_vector m_v;                     //!< Holds the array values
public:
  Json_array();
  ~Json_array();

  // See base class documentation.
  enum_json_type json_type() const { return enum_json_type::J_ARRAY; }

  /**
    Append a clone of the value to the end of the array.
    @param[in] value a JSON value to be appended
    @retval false on success
    @retval true on failure
  */
  bool append_clone(const Json_dom *value);

  /**
    Append the value to the end of the array.

    Ownership of the value is effectively transferred to the array and
    the value will be deallocated by the array so only append values
    that can be deallocated safely (no stack variables please!)

    @param[in]  value a JSON value to be appended
    @retval false on success
    @retval true on failure
  */
  bool append_alias(Json_dom *value); // makes no clone of value

  /**
    Moves all of the elements in the other array to the end of
    this array. The other array is deleted.

    @param [in] other     a pointer to the array which will be consumed
    @retval false on success
    @retval true on failure
  */
  bool consume(Json_array *other);

  /**
    Insert a clone of the value at position index of the array. If beyond the
    end, insert at the end.

    @param[in]  index the position at which to insert
    @param[in]  value a JSON value to be inserted
    @retval false on success
    @retval true on failure
  */
  bool insert_clone(size_t index, const Json_dom *value);

  /**
    Insert the value at position index of the array.
    If beyond the end, insert at the end.

    Ownership of the value is effectively transferred to the array and
    the value will be deallocated by the array so only append values
    that can be deallocated safely (no stack variables please!)

    @param[in]  index the position at which to insert
    @param[in]  value a JSON value to be inserted
    @retval false on success
    @retval true on failure
  */
  bool insert_alias(size_t index, Json_dom *value);

  /**
    Remove the value at this index. A no-op if index is larger than
    size. Deletes the value.
    @param[in]  index
    @return true of a value was removed, false otherwise.
  */
  bool remove(size_t index);

  /**
    Remove the child element addressed by key. Deletes the child.

    @param[in]  child the child to remove

    @return true If that really was a child of this object.
  */
  bool remove(const Json_dom *child);

  /**
    The cardinality of the array (number of values).
    @return the size
  */
  size_t size() const
  {
    return m_v.size();
  }

  // See base class documentation.
  uint32 depth() const;

  // See base class documentation.
  Json_dom *clone() const;

  /**
    Get the value at position index. The value has not been cloned so
    it is the responsibility of the user to make a copy if needed.  Do
    not try to deallocate the returned value - it is owned by the array
    and will be deallocated by it in time.  It is admissible to modify
    its contents (in place; without a clone being taken) if it is a
    compound.

    @param[in] index  the array index
    @return the value at index
  */
  Json_dom *operator[](size_t index) const
  {
    DBUG_ASSERT(m_v[index]->parent() == this);
    return m_v[index];
  }

  /**
    Remove the values in the array.
  */
  void clear();

  /**
     Auto-wrapping constructor. Wraps an array around a dom.
     Ownership of the dom belongs to this array.

     @param [in] innards The dom to autowrap.
  */
  explicit Json_array(Json_dom *innards);

  void replace_dom_in_container(Json_dom *oldv, Json_dom *newv);
};


/**
  Abstract base class for all Json scalars.
*/
class Json_scalar : public Json_dom {
public:
  // See base class documentation.
  uint32 depth() const { return 1; }

  // See base class documentation.
  bool is_scalar() const { return true; }

protected:

  Json_scalar() : Json_dom() {}
};


/**
  Represents a JSON string value (ECMA), of type J_STRING here.
*/
class Json_string : public Json_scalar
{
private:
  std::string m_str; //!< holds the string
public:
  /*
    Construct a Json_string object.
    @param args any arguments accepted by std::string's constructors
  */
  template <typename... Args>
  explicit Json_string(Args&&... args)
    : Json_scalar(), m_str(std::forward<Args>(args)...)
  {}
  ~Json_string() {}

  // See base class documentation
  enum_json_type json_type() const { return enum_json_type::J_STRING; }
  // See base class documentation.
  Json_dom *clone() const { return new (std::nothrow) Json_string(m_str); }

  /**
    Get the reference to the value of the JSON string.
    @return the string reference
  */
  const std::string &value() const { return m_str; }

  /**
    Get the number of characters in the string.
    @return the number of characters
  */
  size_t size() const { return m_str.size(); }
};


/**
  Abstract base class of all JSON number (ECMA) types (subclasses
  represent MySQL extensions).
*/
class Json_number : public Json_scalar {
public:
  // See base class documentation
  bool is_number() const { return true; }

protected:
 Json_number() : Json_scalar() {}
};


/**
  Represents a MySQL decimal number, type J_DECIMAL.
*/
class Json_decimal : public Json_number
{
private:
  my_decimal m_dec; //!< holds the decimal number

public:
  static const int MAX_BINARY_SIZE= DECIMAL_MAX_FIELD_SIZE + 2;

  explicit Json_decimal(const my_decimal &value);
  ~Json_decimal() {}

  /**
    Get the number of bytes needed to store this decimal in a Json_opaque.
    @return the number of bytes.
  */
  int binary_size() const;

  /**
    Get the binary representation of the wrapped my_decimal, so that this
    value can be stored inside of a Json_opaque.

    @param dest the destination buffer to which the binary representation
                is written
    @return false on success, true on error
  */
  bool get_binary(char *dest) const;

  // See base class documentation
  enum_json_type json_type() const { return enum_json_type::J_DECIMAL; }

  /**
    Get a pointer to the MySQL decimal held by this object. Ownership
    is _not_ transferred.
    @return the decimal
  */
  const my_decimal *value() const { return &m_dec; }

  // See base class documentation
  Json_dom *clone() const { return new (std::nothrow) Json_decimal(m_dec); }

  /**
    Convert a binary value produced by get_binary() back to a my_decimal.

    @param[in]   bin  the binary representation
    @param[in]   len  the length of the binary representation
    @param[out]  dec  the my_decimal object to receive the value
    @return  false on success, true on failure
  */
  static bool convert_from_binary(const char *bin, size_t len, my_decimal *dec);
};


/**
  Represents a MySQL double JSON scalar (an extension of the ECMA
  number value), type J_DOUBLE.
*/
class Json_double : public Json_number
{
private:
  double m_f; //!< holds the double value
public:
  explicit Json_double(double value) : Json_number(), m_f(value) {}
  ~Json_double() {}

  // See base class documentation
  enum_json_type json_type() const { return enum_json_type::J_DOUBLE; }

  // See base class documentation
  Json_dom *clone() const;

  /**
    Return the double value held by this object.
    @return the value
  */
  double value() const { return m_f; }
};


/**
  Represents a MySQL integer (64 bits signed) JSON scalar (an extension
  of the ECMA number value), type J_INT.
*/
class Json_int : public Json_number
{
private:
  longlong m_i; //!< holds the value
public:
   explicit Json_int(longlong value) : Json_number(), m_i(value) {}
  ~Json_int() {}

  // See base class documentation
  enum_json_type json_type() const { return enum_json_type::J_INT; }

  /**
    Return the signed int held by this object.
    @return the value
  */
  longlong value() const { return m_i; }

  /**
    @return true if the number can be held by a 16 bit signed integer
  */
  bool is_16bit() const { return INT_MIN16 <= m_i && m_i <= INT_MAX16; }

  /**
    @return true if the number can be held by a 32 bit signed integer
  */
  bool is_32bit() const { return INT_MIN32 <= m_i && m_i <= INT_MAX32; }

  // See base class documentation
  Json_dom *clone() const { return new (std::nothrow) Json_int(m_i); }
};


/**
  Represents a MySQL integer (64 bits unsigned) JSON scalar (an extension
  of the ECMA number value), type J_UINT.
*/

class Json_uint : public Json_number
{
private:
  ulonglong m_i; //!< holds the value
public:
  explicit Json_uint(ulonglong value) : Json_number(), m_i(value) {}
  ~Json_uint() {}

  // See base class documentation
  enum_json_type json_type() const { return enum_json_type::J_UINT; }

  /**
    Return the unsigned int held by this object.
    @return the value
  */
  ulonglong value() const { return m_i; }

  /**
    @return true if the number can be held by a 16 bit unsigned
    integer.
  */
  bool is_16bit() const { return m_i <= UINT_MAX16; }

  /**
    @return true if the number can be held by a 32 bit unsigned
    integer.
  */
  bool is_32bit() const { return m_i <= UINT_MAX32; }

  // See base class documentation
  Json_dom *clone() const { return new (std::nothrow) Json_uint(m_i); }
};


/**
  Represents a JSON null type (ECMA), type J_NULL here.
*/
class Json_null : public Json_scalar
{
public:
   Json_null() : Json_scalar() {}
  ~Json_null() {}

  // See base class documentation
  enum_json_type json_type() const { return enum_json_type::J_NULL; }

  // See base class documentation
  Json_dom *clone() const { return new (std::nothrow) Json_null(); }
};


/**
  Represents a MySQL date/time value (DATE, TIME, DATETIME or
  TIMESTAMP) - an extension to the ECMA set of JSON scalar types, types
  J_DATE, J_TIME, J_DATETIME and J_TIMESTAMP respectively. The method
  field_type identifies which of the four it is.
*/
class Json_datetime : public Json_scalar
{
  friend class Json_dom;
  friend class Json_wrapper;

private:
  MYSQL_TIME m_t;                //!< holds the date/time value
  enum_field_types m_field_type; //!< identifies which type of date/time

public:
  /**
    Constructs a object to hold a MySQL date/time value.

    @param[in] t   the time/value
    @param[in] ft  the field type: must be one of MYSQL_TYPE_TIME,
                   MYSQL_TYPE_DATE, MYSQL_TYPE_DATETIME or
                   MYSQL_TYPE_TIMESTAMP.
  */
  Json_datetime(const MYSQL_TIME &t, enum_field_types ft)
    : Json_scalar(), m_t(t), m_field_type(ft) {}
  ~Json_datetime() {}

  // See base class documentation
  enum_json_type json_type() const;

  // See base class documentation
  Json_dom *clone() const;

  /**
    Return a pointer the date/time value. Ownership is _not_ transferred.
    To identify which time time the value represents, use @c field_type.
    @return the pointer
  */
  const MYSQL_TIME *value() const { return &m_t; }

  /**
    Return what kind of date/time value this object holds.
    @return One of MYSQL_TYPE_TIME, MYSQL_TYPE_DATE, MYSQL_TYPE_DATETIME
            or MYSQL_TYPE_TIMESTAMP.
  */
  enum_field_types field_type() const { return m_field_type; }

  /**
    Convert the datetime to the packed format used when storing
    datetime values.
    @param dest the destination buffer to write the packed datetime to
    (must at least have size PACKED_SIZE)
  */
  void to_packed(char *dest) const;

  /**
    Convert a packed datetime back to a MYSQL_TIME.
    @param from the buffer to read from (must have at least PACKED_SIZE bytes)
    @param ft   the field type of the value
    @param to   the MYSQL_TIME to write the value to
  */
  static void from_packed(const char *from, enum_field_types ft,
                          MYSQL_TIME *to);

  /** Datetimes are packed in eight bytes. */
  static const size_t PACKED_SIZE= 8;

};


/**
  Represents a MySQL value opaquely, i.e. the Json DOM can not
  serialize or deserialize these values.  This should be used to store
  values that don't map to the other Json_scalar classes.  Using the
  "to_string" method on such values (via Json_wrapper) will yield a base
  64 encoded string tagged with the MySQL type with this syntax:

  "base64:typeXX:<base 64 encoded value>"
*/
class Json_opaque : public Json_scalar
{
private:
  enum_field_types m_mytype;
  std::string m_val;
public:
  /**
    An opaque MySQL value.

    @param[in] mytype  the MySQL type of the value
    @param[in] v       the binary value to be stored in the DOM.
                       A copy is taken.
    @param[in] size    the size of the binary value in bytes
    @see #enum_field_types
    @see Class documentation
  */
  Json_opaque(enum_field_types mytype, const char *v, size_t size);
  ~Json_opaque() {}

  // See base class documentation
  enum_json_type json_type() const { return enum_json_type::J_OPAQUE; }

  /**
    @return a pointer to the opaque value. Use #size() to get its size.
  */
  const char *value() const { return m_val.data(); }

  /**
    @return the MySQL type of the value
  */
  enum_field_types type() const { return m_mytype; }
  /**
    @return the size in bytes of the value
  */
  size_t size() const { return m_val.size(); }

   // See base class documentation
  Json_dom *clone() const;
};


/**
  Represents a JSON true or false value, type J_BOOLEAN here.
*/
class Json_boolean : public Json_scalar
{
private:
  bool m_v; //!< false or true: represents the eponymous JSON literal
public:
  explicit Json_boolean(bool value) : Json_scalar(), m_v(value) {}
  ~Json_boolean() {}

  // See base class documentation
  enum_json_type json_type() const { return enum_json_type::J_BOOLEAN; }

  /**
    @return false for JSON false, true for JSON true
  */
  bool value() const { return m_v; }

  // See base class documentation
  Json_dom *clone() const { return new (std::nothrow) Json_boolean(m_v); }
};


/**
  Function for double-quoting a string and escaping characters
  to make up a valid EMCA Json text.

  @param[in]     cptr    the unquoted character string
  @param[in]     length  its length
  @param[in,out] buf     the destination buffer

  @return false on success, true on error
*/
bool double_quote(const char *cptr, size_t length, String *buf);

/**
 Merge two doms. The right dom is either subsumed into the left dom
 or the contents of the right dom are transferred to the left dom
 and the right dom is deleted. After calling this function, the
 caller should not reference the right dom again. It has been
 deleted.

 Returns NULL if there is a memory allocation failure. In this case
 both doms are deleted.

 scalars - If any of the documents that are being merged is a scalar,
 each scalar document is autowrapped as a single value array before merging.

 arrays - When merging a left array with a right array,
 then the result is the left array concatenated
 with the right array. For instance, [ 1, 2 ] merged with [ 3, 4 ]
 is [ 1, 2, 3, 4 ].

 array and object - When merging an array with an object,
 the object is autowrapped as an array and then the rule above
 is applied. So [ 1, 2 ] merged with { "a" : true }
 is [ 1, 2, { "a": true } ].

 objects - When merging two objects, the two objects are concatenated
 into a single, larger object. So { "a" : "foo" } merged with { "b" : 5 }
 is { "a" : "foo", "b" : 5 }.

 duplicates - When two objects are merged and they share a key,
 the values associated with the shared key are merged.

 @param [in,out] left  The recipient dom.
 @param [in,out] right  The dom to be consumed

 @return A composite dom which subsumes the left and right doms, or NULL
 if a failure happened while merging
*/
Json_dom *merge_doms(Json_dom *left, Json_dom *right);


class Json_wrapper_object_iterator
{
private:
  bool m_is_dom; //!< if true, we iterate over a DOM, else a binary object

  // only used for Json_dom
  Json_object::const_iterator m_iter;
  Json_object::const_iterator m_end;

  // only used for json_binary::Value
  size_t m_element_count;
  size_t m_curr_element;
  const json_binary::Value *m_value;
public:
  /**
    @param[in] obj  the JSON object to iterate over
  */
  explicit Json_wrapper_object_iterator(const Json_object *obj);
  /**
    @param[in]  value must contain a JSON object at the top level.
  */
  Json_wrapper_object_iterator(const json_binary::Value *value);
  ~Json_wrapper_object_iterator() {}

  bool empty() const;   //!< Returns true of no more elements
  void next();          //!< Advances iterator to next element

  /**
    Get the current element as a pair: the first part is the element key,
    the second part is the element's value. If the underlying object is
    a DOM, the returned wrapper for the element will hold an alias for
    the element DOM, that is, no copy of the DOM will be taken. In other
    words, the ownership of the element DOM still resides with whoever owns
    the DOM of which the element is part.
  */
  std::pair<const std::string, Json_wrapper> elt() const;
};

/**
  Abstraction for accessing JSON values irrespective of whether they
  are (started out as) binary JSON values or JSON DOM values. The
  purpose of this is to allow uniform access for callers. It allows us
  to access binary JSON values without necessarily building a DOM (and
  thus having to read the entire value unless necessary, e.g. for
  accessing only a single array slot or object field).

  Instances of this class are usually created on the stack. In some
  cases instances are cached in an Item and reused, in which case they
  are allocated from query-duration memory (which is why the class
  inherits from Sql_alloc).
*/
class Json_wrapper : Sql_alloc
{
private:
  /*
    A Json_wrapper wraps either a Json_dom or a json_binary::Value,
    never both at the same time.
  */
  union
  {
    /// The DOM representation, only used if m_is_dom is true.
    struct {
      Json_dom *m_dom_value;
      /// If true, don't deallocate m_dom_value in destructor.
      bool m_dom_alias;
    };
    /// The binary representation, only used if m_is_dom is false.
    json_binary::Value m_value;
  };
  bool m_is_dom;      //!< Wraps a DOM iff true

  /**
    Get the wrapped datetime value in the packed format.

    @param[in,out] buffer a char buffer with space for at least
    Json_datetime::PACKED_SIZE characters
    @return a char buffer that contains the packed representation of the
    datetime (may or may not be the same as buffer)
  */
  const char *get_datetime_packed(char *buffer) const;

public:
  /**
    Create an empty wrapper. Cf #empty().
  */
  Json_wrapper() : m_dom_value(nullptr), m_is_dom(true)
  {
    // Workaround for Solaris Studio, initialize in CTOR body.
    m_dom_alias= true;
  }

  using Sql_alloc::operator new;
  using Sql_alloc::operator delete;

  /** Placement new. */
  void *operator new(size_t, void *ptr,
                     const std::nothrow_t &arg MY_ATTRIBUTE((unused))
                     = std::nothrow) throw()
  { return ptr; }

  /** Placement delete. */
  void operator delete(void*, void*,
                       const std::nothrow_t&) throw ()
  {}

  /**
    Wrap the supplied DOM value (no copy taken). The wrapper takes
    ownership, unless @c set_alias is called after construction.
    In the latter case the lifetime of the DOM is determined by
    the owner of the DOM, so clients need to ensure that that
    lifetime is sufficient, lest dead storage is attempted accessed.

    @param[in,out] dom_value  the DOM value
  */
  explicit Json_wrapper(Json_dom *dom_value);

  /**
    Only meaningful iff the wrapper encapsulates a DOM. Marks the
    wrapper as not owning the DOM object, i.e. it will not be
    deallocated in the wrapper's destructor. Useful if one wants a wrapper
    around a DOM owned by someone else.
  */
  void set_alias() { m_dom_alias= true; }

  /**
    Wrap a binary value. Does not copy the underlying buffer, so
    lifetime is limited the that of the supplied value.

    @param[in] value  the binary value
  */
  explicit Json_wrapper(const json_binary::Value &value);

  /**
    Copy constructor. Does a deep copy of any owned DOM. If a DOM
    os not owned (aliased), the copy will also be aliased.
  */
  Json_wrapper(const Json_wrapper &old);

  /**
    Move constructor. Take over the ownership of the other wrapper's
    DOM, unless it's aliased. If the other wrapper is aliased, this
    wrapper becomes an alias too. Any already owned DOM will be
    deallocated.

    @param old the wrapper whose contents to take over
  */
  Json_wrapper(Json_wrapper &&old);

  /**
    Assignment operator. Does a deep copy of any owned DOM. If a DOM
    os not owned (aliased), the copy will also be aliased. Any owned
    DOM in the left side will be deallocated.
  */
  Json_wrapper &operator=(const Json_wrapper &old);

  /**
    Move-assignment operator. Take over the ownership of the other
    wrapper's DOM, unless it's aliased. If the other wrapper is
    aliased, this wrapper becomes an alias too. Any already owned DOM
    will be deallocated.

    @param old the wrapper whose contents to take over
  */
  Json_wrapper &operator=(Json_wrapper &&old);

  ~Json_wrapper();

  /**
    A Wrapper is defined to be empty if it is passed a NULL value with the
    constructor for JSON dom, or if the default constructor is used.

    @return true if the wrapper is empty.
  */
  bool empty() const { return m_is_dom && !m_dom_value; }

  /**
    Get the wrapped contents in DOM form. The DOM is (still) owned by the
    wrapper. If this wrapper originally held a value, it is now converted
    to hold (and eventually release) the DOM version.

    @param thd current session
    @return pointer to a DOM object, or NULL if the DOM could not be allocated
  */
  Json_dom *to_dom(const THD *thd);

  /**
    Get the wrapped contents in DOM form. Same as to_dom(), except it returns
    a clone of the original DOM instead of the actual, internal DOM tree.

    @param thd current session
    @return pointer to a DOM object, or NULL if the DOM could not be allocated
  */
  Json_dom *clone_dom(const THD *thd) const;

  /**
    Get the wrapped contents in binary value form.

    @param[in]     thd  current session
    @param[in,out] str  a string that will be filled with the binary value
    @retval false on success
    @retval true  on error
  */
  bool to_binary(const THD *thd, String *str) const;

  /**
    Check if the wrapped JSON document is a binary value (a
    json_binary::Value), and if that binary is pointing to data stored in the
    given string.

    This function can be used to check if overwriting the data in the string
    might overwrite and corrupt the document contained in this wrapper.

    @param str    a string which contains JSON binary data
    @retval true  if the string contains data that the wrapped document
                  points to from its json_binary::Value representation
    @retval false otherwise
  */
  bool is_binary_backed_by(const String *str) const
  {
    return !m_is_dom && m_value.is_backed_by(str);
  }

  /**
    Format the JSON value to an external JSON string in buffer in
    the format of ISO/IEC 10646.

    @param[in,out] buffer      the formatted string is appended, so make sure
                               the length is set correctly before calling
    @param[in]     json_quoted if the JSON value is a string and json_quoted
                               is false, don't perform quoting on the string.
                               This is only used by JSON_UNQUOTE.
    @param[in]     func_name   The name of the function that called to_string().

    @return false formatting went well, else true
  */
  bool to_string(String *buffer, bool json_quoted, const char *func_name) const;

  /**
    Format the JSON value to an external JSON string in buffer in the format of
    ISO/IEC 10646. Add newlines and indentation for readability.

    @param[in,out] buffer     the buffer that receives the formatted string
                              (the string is appended, so make sure the length
                              is set correctly before calling)
    @param[in]     func_name  the name of the calling function

    @retval false on success
    @retval true on error
  */
  bool to_pretty_string(String *buffer, const char *func_name) const;

  // Accessors

  /**
    Return the type of the wrapped JSON value

    @return the type, or Json_dom::J_ERROR if the wrapper does not contain
    a JSON value
  */
  enum_json_type type() const;

  /**
    Return the MYSQL type of the opaque value, see #type(). Valid for
    J_OPAQUE.  Calling this method if the type is not J_OPAQUE will give
    undefined results.

    @return the type
  */
  enum_field_types field_type() const;

  /**
    If this wrapper holds a JSON object, return an iterator over the
    elements.  Valid for J_OBJECT.  Calling this method if the type is
    not J_OBJECT will give undefined results.

    @return the iterator
  */
  Json_wrapper_object_iterator object_iterator() const;

  /**
    If this wrapper holds a JSON array, get an array value by indexing
    into the array. Valid for J_ARRAY.  Calling this method if the type is
    not J_ARRAY will give undefined results.

    @return the array value
  */
  Json_wrapper operator[](size_t index) const;

  /**
    If this wrapper holds a JSON object, get the value corresponding
    to the member key. Valid for J_OBJECT.  Calling this method if the type is
    not J_OBJECT will give undefined results.

    @param[in]     key name for identifying member
    @param[in]     len length of that member name

    @return The member value. If there is no member with the specified
    name, a value with type Json_dom::J_ERROR is returned.
  */
  Json_wrapper lookup(const char *key, size_t len) const;

  /**
    Get a pointer to the data of a JSON string or JSON opaque value.
    The data is still owner by the wrapper. The data may not be null
    terminated, so use in conjunction with @c get_data_length.
    Valid for J_STRING and J_OPAQUE.  Calling this method if the type is
    not one of those will give undefined results.

    @return the pointer
  */
  const char *get_data() const;

  /**
    Get the length to the data of a JSON string or JSON opaque value.
    Valid for J_STRING and J_OPAQUE.  Calling this method if the type is
    not one of those will give undefined results.

    @return the length
  */
  size_t get_data_length() const;

  /**
    Get the MySQL representation of a JSON decimal value.
    Valid for J_DECIMAL.  Calling this method if the type is
    not J_DECIMAL will give undefined results.

    @param[out] d  the decimal value
    @return false on success, true on failure (which would indicate an
    internal error)
  */
  bool get_decimal_data(my_decimal *d) const;

  /**
    Get the value of a JSON double number.
    Valid for J_DOUBLE.  Calling this method if the type is
    not J_DOUBLE will give undefined results.

    @return the value
  */
  double get_double() const;

  /**
    Get the value of a JSON signed integer number.
    Valid for J_INT.  Calling this method if the type is
    not J_INT will give undefined results.

    @return the value
  */
  longlong get_int() const;

  /**
    Get the value of a JSON unsigned integer number.
    Valid for J_UINT.  Calling this method if the type is
    not J_UINT will give undefined results.

    @return the value
  */
  ulonglong get_uint() const;

  /**
    Get the value of a JSON date/time value.  Valid for J_TIME,
    J_DATETIME, J_DATE and J_TIMESTAMP.  Calling this method if the type
    is not one of those will give undefined results.

    @param[out] t  the date/time value
  */
  void get_datetime(MYSQL_TIME *t) const;

  /**
    Get a boolean value (a JSON true or false literal).
    Valid for J_BOOLEAN.  Calling this method if the type is
    not J_BOOLEAN will give undefined results.

    @return the value
  */
  bool get_boolean() const;

  /**
    Finds all of the json sub-documents which match the path expression.
    Puts the matches on an evolving vector of results.
    This is a bit inefficient for binary wrappers because you can't
    build up a binary array incrementally from its cells. Instead, you
    have to turn each cell into a dom and then add the doms to a
    dom array.

    Calling this if #empty() returns true is an error.

    Special care must be taken when the path expression contains more than one
    ellipsis (**) token. That is because multiple paths with ellipses may
    identify the same value. Consider the following document:

    { "a": { "x" : { "b": { "y": { "b": { "z": { "c": 100 } } } } } } }

    The innermost value (the number 100) has the following unique,
    non-wildcarded address:

    $.a.x.b.y.b.z.c

    That location is reached by both of the following paths which include
    the ellipsis token:

    $.a.x.b**.c
    $.a.x.b.y.b**.c

    And those addresses both satisfy the following path expression which has
    two ellipses:

    $.a**.b**.c

    In this case, we only want to return one instance of $.a.x.b.y.b.z.c

    @param[in] path   the (possibly wildcarded) address of the sub-documents
    @param[out] hits  the result of the search
    @param[in] auto_wrap true of we match a final scalar with search for [0]
    @param[in]  only_need_one True if we can stop after finding one match

    @retval false on success
    @retval true on error
  */
  bool seek(const Json_seekable_path &path,
            Json_wrapper_vector *hits, bool auto_wrap,
            bool only_need_one);

  /**
    Finds all of the json sub-documents which match the path expression.
    Puts the matches on an evolving vector of results. This is a fast-track
    method for paths which don't contain ellipses. Those paths can take
    advantage of the efficient positioning logic of json_binary::Value.

    @param[in] path   the (possibly wildcarded) address of the sub-documents
    @param[out] hits  the result of the search
    @param[in] leg_number  the 0-based index of the current path leg
    @param[in] auto_wrap true of we match a final scalar with search for [0]
    @param[in]  only_need_one True if we can stop after finding one match

    @returns false if there was no error, otherwise true on error
  */
  bool seek_no_ellipsis(const Json_seekable_path &path,
                        Json_wrapper_vector *hits,
                        const size_t leg_number,
                        bool auto_wrap,
                        bool only_need_one)
    const;

  /**
    Compute the length of a document. This is the value which would be
    returned by the JSON_LENGTH() system function. So, this returns

    - for scalar values: 1
    - for objects: the number of members
    - for arrays: the number of cells

    @returns 1, the number of members, or the number of cells
  */
  size_t length() const;

  /**
    Compute the depth of a document. This is the value which would be
    returned by the JSON_DEPTH() system function.

    - for scalar values, empty array and empty object: 1
    - for non-empty array: 1+ max(depth of array elements)
    - for non-empty objects: 1+ max(depth of object values)

    For example:
    "abc", [] and {} have depth 1.
    ["abc", [3]] and {"a": "abc", "b": [3]} have depth 3.

    @param thd current session
    @return the depth of the document
  */
  size_t depth(const THD *thd) const;

  /**
    Compare this JSON value to another JSON value.
    @param[in] other the other JSON value
    @retval -1 if this JSON value is less than the other JSON value
    @retval 0 if the two JSON values are equal
    @retval 1 if this JSON value is greater than the other JSON value
  */
  int compare(const Json_wrapper &other) const;

  /**
    Extract an int (signed or unsigned) from the JSON if possible
    coercing if need be.
    @param[in] msgnam to use in error message in conversion failed
    @returns json value coerced to int
  */
  longlong coerce_int(const char *msgnam) const;

  /**
    Extract a real from the JSON if possible, coercing if need be.

    @param[in] msgnam to use in error message in conversion failed
    @returns json value coerced to real
  */
  double coerce_real(const char *msgnam) const;

  /**
    Extract a decimal from the JSON if possible, coercing if need be.

    @param[in,out] decimal_value a value buffer
    @param[in] msgnam to use in error message in conversion failed
    @returns json value coerced to decimal
  */
  my_decimal *coerce_decimal(my_decimal *decimal_value, const char *msgnam)
    const;

  /**
    Extract a date from the JSON if possible, coercing if need be.

    @param[in,out] ltime a value buffer
    @param msgnam
    @returns json value coerced to date
   */
  bool coerce_date(MYSQL_TIME *ltime, const char *msgnam) const;

  /**
    Extract a time value from the JSON if possible, coercing if need be.

    @param[in,out] ltime a value buffer
    @param msgnam

    @returns json value coerced to time
  */
  bool coerce_time(MYSQL_TIME *ltime, const char *msgnam) const;

  /**
    Make a sort key that can be used by filesort to order JSON values.

    @param[out] to      a buffer to which the sort key is written
    @param[in]  length  the length of the sort key

    @details Key storage format is following:
    @verbatim
      |<json type><   sort key    >|
       1 byte    / variable length /
    @endverbatim

    JSON is assumed to be non-sql-null and valid (checked by caller).
    Key length contains full length - the len prefix itself, json type and the
    sort key.
    All numeric types are stored as a number, without distinction to
    double/decimal/int/etc. See @c make_json_numeric_sort_key().
    Same is done to DATETIME and TIMESTAMP types.
    For string and opaque types only the prefix that fits into the output buffer
    is stored.
    For JSON objects and arrays only their length (number of elements) is
    stored, this is a limitation of current implementation.
  */
  size_t make_sort_key(uchar *to, size_t length) const;

  /**
    Make a hash key that can be used by sql_executor.cc/unique_hash
    in order to support SELECT DISTINCT

    @param[in]  hash_val  An initial hash value.
  */
  ulonglong make_hash_key(ulonglong *hash_val);
};

/**
  Check if a string contains valid JSON text, without generating a
  Json_dom representation of the document.

  @param[in] text    pointer to the beginning of the string
  @param[in] length  the length of the string
  @return true if the string is valid JSON text, false otherwise
*/
bool is_valid_json_syntax(const char *text, size_t length);

/**
  A class that is capable of holding objects of any sub-type of
  Json_scalar. Used for pre-allocating space in query-duration memory
  for JSON scalars that are to be returned by get_json_atom_wrapper().
*/
class Json_scalar_holder : public Sql_alloc
{
  /**
    Union of all concrete subclasses of Json_scalar. The union is
    never instantiated. It is only used for finding how much space
    needs to be allocated for #m_buffer.
  */
  union Any_json_scalar
  {
    Json_string m_string;
    Json_decimal m_decimal;
    Json_int m_int;
    Json_uint m_uint;
    Json_double m_double;
    Json_boolean m_boolean;
    Json_null m_null;
    Json_datetime m_datetime;
    Json_opaque m_opaque;
    // Need explicitly deleted destructor to silence warning on MSVC.
    ~Any_json_scalar() = delete;
  };

  /// The buffer in which the Json_scalar value is stored.
  char m_buffer[sizeof(Any_json_scalar)];

  /// True if and only if a value has been assigned to the holder.
  bool m_assigned= false;

  /// Clear the holder, and destroy the held value if there is one.
  void clear()
  {
    if (m_assigned)
    {
      get()->~Json_scalar();
      m_assigned= false;
    }
  }
public:
  /// Destructor. The held value is destroyed, if there is one.
  ~Json_scalar_holder() { clear(); }

  /// Get a pointer to the held object, or nullptr if there is none.
  Json_scalar *get()
  {
    void *ptr= m_assigned ? &m_buffer : nullptr;
    return static_cast<Json_scalar *>(ptr);
  }

  /**
    Construct a new Json_scalar value in this Json_scalar_holder.
    If a value is already held, the old value is destroyed and replaced.
    @tparam T which type of Json_scalar to create
    @param args the arguments to T's constructor
  */
  template <typename T, typename... Args> void emplace(Args&&... args)
  {
    static_assert(std::is_base_of<Json_scalar, T>::value, "Not a Json_scalar");
    static_assert(sizeof(T) <= sizeof(m_buffer), "Buffer is too small");
    clear();
    ::new (&m_buffer) T(std::forward<Args>(args)...);
    m_assigned= true;
  }
};

#endif /* JSON_DOM_INCLUDED */
