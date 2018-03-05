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

#include "json_dom.h"
#include "json_path.h"
#include "sql_class.h"          // THD
#include "sql_time.h"
#include "sql_const.h"
#include "derror.h"
#include "base64.h"
#include "m_string.h"           // my_gcvt, _dig_vec_lower
#include "mysqld_error.h"       // ER_*
#include "prealloced_array.h"   // Prealloced_array
#include "template_utils.h"     // down_cast, pointer_cast

#include "rapidjson/rapidjson.h"
#include "rapidjson/reader.h"
#include "rapidjson/memorystream.h"
#include "rapidjson/error/en.h"

#include <algorithm>            // std::min, std::max
#include <memory>               // std::auto_ptr

using namespace rapidjson;

const char * Json_dom::json_type_string_map[]= {
  "NULL",
  "DECIMAL",
  "INTEGER",
  "UNSIGNED INTEGER",
  "DOUBLE",
  "STRING",
  "OBJECT",
  "ARRAY",
  "BOOLEAN",
  "DATE",
  "TIME",
  "DATETIME",
  "TIMESTAMP",
  "OPAQUE",
  "ERROR",

  // OPAQUE types with special names
  "BLOB",
  "BIT",
  "GEOMETRY",

  NULL
};


/**
 Auto-wrap a dom. Delete the dom if there is a memory
 allocation failure.
*/
Json_array *wrap_in_array(Json_dom *dom)
{
  Json_array *array= new (std::nothrow) Json_array(dom);
  if (array == NULL)
    delete dom;                               /* purecov: inspected */
  return array;
}


/**
  A dom is mergeable if it is an array or an object. All other
  types must be wrapped in an array in order to be merged.
  Delete the candidate if there is a memory allocation failure.
*/
Json_dom *make_mergeable(Json_dom *candidate)
{
  switch (candidate->json_type())
  {
  case Json_dom::J_ARRAY:
  case Json_dom::J_OBJECT:
    {
      return candidate;
    }
  default:
    {
      return wrap_in_array(candidate);
    }
  }
}

Json_dom *merge_doms(Json_dom *left, Json_dom *right)
{
  left= make_mergeable(left);
  if (!left)
  {
    /* purecov: begin inspected */
    delete right;
    return NULL;
    /* purecov: end */
  }

  right= make_mergeable(right);
  if (!right)
  {
    /* purecov: begin inspected */
    delete left;
    return NULL;
    /* purecov: end */
  }

  // at this point, the arguments are either objects or arrays
  bool left_is_array= (left->json_type() == Json_dom::J_ARRAY);
  bool right_is_array= (right->json_type() == Json_dom::J_ARRAY);

  if (left_is_array || right_is_array)
  {
    if (!left_is_array)
      left= wrap_in_array(left);
    if (!left)
    {
      /* purecov: begin inspected */
      delete right;
      return NULL;
      /* purecov: end */
    }

    if (!right_is_array)
      right= wrap_in_array(right);
    if (!right)
    {
      /* purecov: begin inspected */
      delete left;
      return NULL;
      /* purecov: end */
    }

    if (down_cast<Json_array *>(left)->consume(down_cast<Json_array *>(right)))
    {
      delete left;                              /* purecov: inspected */
      return NULL;                              /* purecov: inspected */
    }
  }
  else  // otherwise, both doms are objects
  {
    if (down_cast<Json_object *>(left)
        ->consume(down_cast<Json_object *>(right)))
    {
      delete left;                              /* purecov: inspected */
      return NULL;                              /* purecov: inspected */
    }
  }

  return left;
}


void *Json_dom::operator new(size_t size, const std::nothrow_t&) throw()
{
  /*
    Call my_malloc() with the MY_WME flag to make sure that it will
    write an error message if the memory could not be allocated.
  */
  return my_malloc(key_memory_JSON, size, MYF(MY_WME));
}


void Json_dom::operator delete(void *ptr) throw()
{
  my_free(ptr);
}


/*
  This operator is included in order to silence warnings on some
  compilers. It is called if the constructor throws an exception when
  an object is allocated with nothrow new. This is not supposed to
  happen and is therefore hard to test, so annotate it to avoid
  cluttering the test coverage reports.
*/
/* purecov: begin inspected */
void Json_dom::operator delete(void *ptr, const std::nothrow_t&) throw()
{
  operator delete(ptr);
}
/* purecov: end */


/**
  Compute the maximum length of the string representation of the Json type
  literals which we use as output from JSON_TYPE.

  @return the length of the longest literal + 1 (for terminating NUL).
*/
static uint32 compute_max_typelit()
{
  size_t maxl= 0;
  for (const char **s= &Json_dom::json_type_string_map[0]; *s; ++s)
  {
    maxl= std::max(std::strlen(*s), maxl);
  }
  return static_cast<uint32>(maxl + 1);
}

const uint32 Json_dom::typelit_max_length= compute_max_typelit();

static bool seen_already(Json_dom_vector *result, Json_dom *cand)
{
  Json_dom_vector::iterator it= std::find(result->begin(),
                                          result->end(),
                                          cand);
  return it != result->end();
}

/**
  Add a value to a vector if it isn't already there.

  @param[in] candidate value to add
  @param[in,out] duplicates set of values added
  @param[in,out] result vector
  @return false on success, true on error
*/
static bool add_if_missing(Json_dom *candidate,
                           Json_dom_vector *duplicates,
                           Json_dom_vector *result)
{
  if (duplicates->insert_unique(candidate).second)
  {
    return result->push_back(candidate);
  }
  return false;
}


/**
  Check if a seek operation performed by Json_dom::find_child_doms()
  or Json_dom::seek() is done.

  @return true if only one result is needed and a result has been found
*/
template <class Result_vector>
static inline bool is_seek_done(const Result_vector *hits, bool only_need_one)
{
  return only_need_one && hits->size() > 0;
}


bool Json_dom::find_child_doms(const Json_path_leg *path_leg,
                               bool auto_wrap,
                               bool only_need_one,
                               Json_dom_vector *duplicates,
                               Json_dom_vector *result)
{
  Json_dom::enum_json_type dom_type= json_type();
  enum_json_path_leg_type leg_type= path_leg->get_type();

  if (is_seek_done(result, only_need_one))
    return false;

  switch (leg_type)
  {
  case jpl_array_cell:
    {
      size_t array_cell_index= path_leg->get_array_cell_index();

      if (dom_type == Json_dom::J_ARRAY)
      {
        const Json_array * const array= down_cast<const Json_array *>(this);

        if (array_cell_index < array->size() &&
            add_if_missing((*array)[array_cell_index], duplicates, result))
          return true;                        /* purecov: inspected */
      }
      else if (array_cell_index == 0 && auto_wrap)
      {
        if (!seen_already(result, this))
        {
          // auto-wrap non-arrays
          if (add_if_missing(this, duplicates, result))
            return true;                      /* purecov: inspected */
        }
      }

      return false;
    }
  case jpl_ellipsis:
    {
      if (add_if_missing(this, duplicates, result))
        return true;                          /* purecov: inspected */

      if (dom_type == Json_dom::J_ARRAY)
      {
        const Json_array * const array= down_cast<const Json_array *>(this);

        for (unsigned eidx= 0; eidx < array->size(); eidx++)
        {
          Json_dom * child= (*array)[eidx];
          if (add_if_missing(child, duplicates, result))
            return true;                      /* purecov: inspected */
          if (is_seek_done(result, only_need_one))
            return false;                     /* purecov: inspected */

          Json_dom::enum_json_type child_type= child->json_type();
          if ((child_type == Json_dom::J_ARRAY) ||
              (child_type == Json_dom::J_OBJECT))
          {
            // now recurse and add all objects and arrays under the child
            if (child->find_child_doms(path_leg, auto_wrap, only_need_one,
                                       duplicates, result))
              return true;                    /* purecov: inspected */
          }
        } // end of loop through children
      }
      else if (dom_type == Json_dom::J_OBJECT)
      {
        const Json_object *const object=
          down_cast<const Json_object *>(this);

        for (Json_object::const_iterator iter= object->begin();
             iter != object->end(); ++iter)
        {
          Json_dom *child= iter->second;
          Json_dom::enum_json_type child_type= child->json_type();

          if (add_if_missing(child, duplicates, result))
            return true;                      /* purecov: inspected */
          if (is_seek_done(result, only_need_one))
            return false;                     /* purecov: inspected */

          if ((child_type == Json_dom::J_ARRAY) ||
              (child_type == Json_dom::J_OBJECT))
          {
            // now recurse and add all objects and arrays under the child
            if (child->find_child_doms(path_leg, auto_wrap, only_need_one,
                                       duplicates, result))
              return true;                    /* purecov: inspected */
          }
        } // end of loop through children
      }

      return false;
    }
  case jpl_array_cell_wildcard:
    {
      if (dom_type == Json_dom::J_ARRAY)
      {
        const Json_array * array= down_cast<const Json_array *>(this);

        for (unsigned idx= 0; idx < array->size(); idx++)
        {
          if (add_if_missing((*array)[idx], duplicates, result))
            return true;                      /* purecov: inspected */
          if (is_seek_done(result, only_need_one))
            return false;
        }
      }

      return false;
    }
  case jpl_member:
    {
      if (dom_type == Json_dom::J_OBJECT)
      {
        const Json_object * object= down_cast<const Json_object *>(this);
        std::string member_name(path_leg->get_member_name(),
                                path_leg->get_member_name_length());
        Json_dom * child= object->get(member_name);

        if (child != NULL && add_if_missing(child, duplicates, result))
          return true;                        /* purecov: inspected */
      }

      return false;
    }
  case jpl_member_wildcard:
    {
      if (dom_type == Json_dom::J_OBJECT)
      {
        const Json_object * object= down_cast<const Json_object *>(this);

        for (Json_object::const_iterator iter= object->begin();
             iter != object->end(); ++iter)
        {
          if (add_if_missing(iter->second, duplicates, result))
            return true;                      /* purecov: inspected */
          if (is_seek_done(result, only_need_one))
            return false;
        }
      }

      return false;
    }
  }

  /* purecov: begin deadcode */
  DBUG_ABORT();
  return true;
  /* purecov: end */
}


Json_object::Json_object()
  : Json_dom(),
    m_map(Json_object_map::key_compare(),
          Json_object_map::allocator_type(key_memory_JSON))
{}


Json_object::~Json_object()
{
  clear();
}


/**
  Check if the depth of a JSON document exceeds the maximum supported
  depth (JSON_DOCUMENT_MAX_DEPTH). Raise an error if the maximum depth
  has been exceeded.

  @param[in] depth  the current depth of the document
  @return true if the maximum depth is exceeded, false otherwise
*/
static bool check_json_depth(size_t depth)
{
  if (depth > JSON_DOCUMENT_MAX_DEPTH)
  {
    my_error(ER_JSON_DOCUMENT_TOO_DEEP, MYF(0));
    return true;
  }
  return false;
}


/**
  This class overrides the methods on BaseReaderHandler to make
  out own handler which will construct our DOM from the parsing
  of the JSON text.
  <code>
  bool Null() {   }
  bool Bool(bool) {   }
  bool Int(int) {   }
  bool Uint(unsigned) {   }
  bool Int64(int64_t) {   }
  bool Uint64(uint64_t) {   }
  bool Double(double) {   }
  bool String(const Ch*, SizeType, bool) {   }
  bool StartObject() {   }
  bool Key() {   }
  bool EndObject(SizeType) {   }
  bool StartArray() {   }
  bool EndArray(SizeType) {   }
  </code>
  @see Json_dom::parse
*/
class Rapid_json_handler : public BaseReaderHandler<UTF8<> >
{
private:

// std::cerr << "callback " << name << ':' << state << '\n'; std::cerr.flush()
#define DUMP_CALLBACK(name,state)

  enum enum_state
  {
    expect_anything,
    expect_array_value,
    expect_object_key,
    expect_object_value,
    expect_eof
  };

  struct Current_element
  {
    Current_element(bool object, const char *str, uint32 length,
                    Json_dom *value)
      : m_object(object), m_key(std::string(str, length)), m_value(value)
    {}

    Current_element(Json_dom *value)
      : m_object(false), m_value(value)
    {}

    bool m_object; //!< true of object, false if array
    std::string m_key; //!< only used if object
    Json_dom *m_value; //!< deallocated by clients
  };

  typedef Prealloced_array<Current_element, 8, false> Element_vector;

  struct Partial_compound
  {
    Partial_compound(bool is_object)
      : m_elements(key_memory_JSON),
        m_is_object(is_object)
    {}
    ~Partial_compound()
    {}

    Element_vector m_elements;
    bool m_is_object;
  };

  typedef Prealloced_array<Partial_compound, 8, false> Compound_vector;

  enum_state m_state;
  Compound_vector m_stack;
  Json_dom *m_dom_as_built;
  bool m_preserve_neg_zero_int;
public:
  Rapid_json_handler(bool preserve_neg_zero_int= false)
    : m_state(expect_anything),
      m_stack(key_memory_JSON),
      m_dom_as_built(NULL),
      m_preserve_neg_zero_int(preserve_neg_zero_int)
  {}

  ~Rapid_json_handler()
  {
    if (m_dom_as_built)
    {
      // We managed to build something, but found garbage after it
      delete m_dom_as_built;
    }
    else
    {
      // We have something half built, empty the allocated data in it
      for (Compound_vector::iterator iter= m_stack.begin();
           iter != m_stack.end(); ++iter)
      {
        for (Element_vector::iterator i= iter->m_elements.begin();
             i != iter->m_elements.end(); ++i)
        {
          delete i->m_value;
        }
      }
    }
  }

  /**
    @returns The built JSON DOM object.
    Deallocation is the returned value responsibility of the caller.
  */
  Json_dom *get_built_doc()
  {
    Json_dom *result= m_dom_as_built;
    m_dom_as_built= NULL;
    return result;
  }

  /**
    Function which is called on each scalar value found in the JSON
    document being parsed.

    @param[in] scalar the scalar that was seen
    @return true if parsing should continue, false if an error was
            found and parsing should stop
  */
  bool seeing_scalar(Json_scalar *scalar)
  {
    std::auto_ptr<Json_scalar> aptr(scalar);
    if (scalar == NULL || check_json_depth(m_stack.size() + 1))
      return false;
    switch (m_state)
    {
    case expect_anything:
      m_dom_as_built= scalar;
      m_state= expect_eof;
      break;
    case expect_array_value:
      if (m_stack.back().m_elements.push_back(Current_element(scalar)))
        return false;                           /* purecov: inspected */
      break;
    case expect_object_key:
    case expect_eof:
      /* purecov: begin inspected */
      DBUG_ABORT();
      return false;
      /* purecov: end */
    case expect_object_value:
      DBUG_ASSERT(!m_stack.back().m_elements.empty());
      DBUG_ASSERT(m_stack.back().m_elements.back().m_value == NULL);
      m_stack.back().m_elements.back().m_value= scalar;
      m_state= expect_object_key;
      break;
    }

    /*
      The scalar is owned by the Element_vector or m_dom_as_built now,
      so release it.
    */
    aptr.release();
    return true;
  }

  bool Null()
  {
    DUMP_CALLBACK("null", state);
    return seeing_scalar(new (std::nothrow) Json_null());
  }

  bool Bool(bool b)
  {
    DUMP_CALLBACK("bool", state);
    return seeing_scalar(new (std::nothrow) Json_boolean(b));
  }

  bool Int(int i)
  {
    DUMP_CALLBACK("int", state);
    return seeing_scalar(new (std::nothrow) Json_int(i));
  }

  bool Uint(unsigned u)
  {
    DUMP_CALLBACK("uint", state);
    return seeing_scalar(new (std::nothrow) Json_int(static_cast<longlong>(u)));
  }

  bool Int64(int64_t i)
  {
    DUMP_CALLBACK("int64", state);
    return seeing_scalar(new (std::nothrow) Json_int(i));
  }

  bool Uint64(uint64_t ui64)
  {
    DUMP_CALLBACK("uint64", state);
    return seeing_scalar(new (std::nothrow) Json_uint(ui64));
  }

  bool Double(double d, bool is_int= false)
  {
    if (is_int && !m_preserve_neg_zero_int)
    {
      /*
        The is_int flag is true only if -0 was seen. Handle it as an
        integer.
      */
      DBUG_ASSERT(d == 0.0);
      return Int64(static_cast<int64_t>(d));
    }
    else
    {
      DUMP_CALLBACK("double", state);
      return seeing_scalar(new (std::nothrow) Json_double(d));
    }
  }

  bool String(const char* str, SizeType length, bool copy)
  {
    if (check_json_depth(m_stack.size() + 1))
      return false;
    DUMP_CALLBACK("string", state);
    switch (m_state)
    {
    case expect_anything:
      m_dom_as_built= new (std::nothrow) Json_string(std::string(str, length));
      if (!m_dom_as_built)
        return false;                         /* purecov: inspected */
      m_state= expect_eof;
      break;
    case expect_array_value:
      {
        Json_string *jstr=
          new (std::nothrow) Json_string(std::string(str, length));
        if (jstr == NULL ||
            m_stack.back().m_elements.push_back(Current_element(jstr)))
        {
          /* purecov: begin inspected */
          delete jstr;
          return false;
          /* purecov: end */
        }
        break;
      }
    case expect_object_key:
      if (m_stack.back().m_elements.push_back(Current_element(true, str,
                                                              length, NULL)))
        return false;                         /* purecov: inspected */
      m_state= expect_object_value;
      break;
    case expect_eof:
      /* purecov: begin inspected */
      DBUG_ABORT();
      return false;
      /* purecov: end */
    case expect_object_value:
      DBUG_ASSERT(!m_stack.back().m_elements.empty());
      DBUG_ASSERT(m_stack.back().m_elements.back().m_value == NULL);
      m_stack.back().m_elements.back().m_value=
        new (std::nothrow) Json_string(std::string(str, length));
      m_state= expect_object_key;
      break;
    }
    return true;
  }

  bool StartObject()
  {
    DUMP_CALLBACK("start object {", state);
    switch (m_state)
    {
    case expect_anything:
    case expect_array_value:
    case expect_object_value:
      if (m_stack.push_back(Partial_compound(true)) ||
          check_json_depth(m_stack.size()))
        return false;
      m_state= expect_object_key;
      break;
    case expect_eof:
    case expect_object_key:
      /* purecov: begin inspected */
      DBUG_ABORT();
      return false;
      /* purecov: end */
    }
    return true;
  }

  bool EndObject(SizeType)
  {
    DUMP_CALLBACK("} end object", state);
    switch (m_state)
    {
    case expect_object_key:
      {
        std::auto_ptr<Json_object> o(new (std::nothrow) Json_object());
        if (o.get() == NULL)
          return false;                       /* purecov: inspected */
        for (Element_vector::const_iterator iter=
               m_stack.back().m_elements.begin();
             iter != m_stack.back().m_elements.end(); ++iter)
        {
          /* _alias: save superfluous copy/delete */
          if (o->add_alias(iter->m_key, iter->m_value))
            return false;                     /* purecov: inspected */
        }
        m_stack.pop_back();

        if (m_stack.empty())
        {
          m_dom_as_built= o.release();
          m_state= expect_eof;
        }
        else if (m_stack.back().m_is_object)
        {
          m_stack.back().m_elements.back().m_value= o.release();
          m_state= expect_object_key;
        }
        else
        {
          if (m_stack.back().m_elements.push_back(o.get()))
            return false;                     /* purecov: inspected */
          o.release();             // Owned by the Element_vector now.
          m_state= expect_array_value;
        }
      }
      break;
    case expect_array_value:
    case expect_eof:
    case expect_object_value:
    case expect_anything:
      /* purecov: begin inspected */
      DBUG_ABORT();
      return false;
      /* purecov: end */
    }
    return true;
  }

  bool StartArray()
  {
    DUMP_CALLBACK("start array [", state);
    switch (m_state)
    {
    case expect_anything:
    case expect_array_value:
    case expect_object_value:
      if (m_stack.push_back(Partial_compound(false)) ||
          check_json_depth(m_stack.size()))
        return false;
      m_state= expect_array_value;
      break;
    case expect_eof:
    case expect_object_key:
      /* purecov: begin inspected */
      DBUG_ABORT();
      return false;
      /* purecov: end */
    }
    return true;
  }

  bool EndArray(SizeType)
  {
    DUMP_CALLBACK("] end array", state);
    switch (m_state)
    {
    case expect_array_value:
      {
        std::auto_ptr<Json_array> a(new (std::nothrow) Json_array());
        if (a.get() == NULL)
          return false;                         /* purecov: inspected */
        for (Element_vector::const_iterator iter=
               m_stack.back().m_elements.begin();
             iter != m_stack.back().m_elements.end(); ++iter)
        {
          /* _alias: save superfluous copy/delete */
          if (a->append_alias(iter->m_value))
            return false;                       /* purecov: inspected */
        }
        m_stack.pop_back();

        if (m_stack.empty())
        {
          m_dom_as_built= a.release();
          m_state= expect_eof;
        }
        else
        {
          if (m_stack.back().m_is_object)
          {
            m_stack.back().m_elements.back().m_value= a.release();
            m_state= expect_object_key;
          }
          else
          {
            if (m_stack.back().m_elements.push_back(a.get()))
              return false;                     /* purecov: inspected */
            a.release();                // Owned by the Element_vector now.
            m_state= expect_array_value;
          }
        }
      }
      break;
    case expect_object_key:
    case expect_object_value:
    case expect_eof:
    case expect_anything:
      /* purecov: begin inspected */
      DBUG_ABORT();
      return false;
      /* purecov: end */
    }
    return true;
  }

  bool Key(const Ch* str, SizeType len, bool copy)
  {
    return String(str, len, copy);
  }
};


Json_dom *Json_dom::parse(const char *text, size_t length,
                          const char **syntaxerr, size_t *offset,
                          bool preserve_neg_zero_int)
{
  Rapid_json_handler handler(preserve_neg_zero_int);
  MemoryStream ss(text, length);
  Reader reader;
  bool success= reader.Parse<kParseDefaultFlags>(ss, handler);

  if (success)
  {
    Json_dom *dom= handler.get_built_doc();
    if (dom == NULL && syntaxerr != NULL)
    {
      // The parsing failed for some other reason than a syntax error.
      *syntaxerr= NULL;
    }
    return dom;
  }

  // Report the error offset and the error message if requested by the caller.
  if (offset != NULL)
    *offset= reader.GetErrorOffset();
  if (syntaxerr != NULL)
    *syntaxerr= GetParseError_En(reader.GetParseErrorCode());

  return NULL;
}


/**
  This class implements a handler for use with rapidjson::Reader when
  we want to check if a string is a valid JSON text. The handler does
  not build a DOM structure, so it is quicker than Json_dom::parse()
  in the cases where we don't care about the DOM, such as in the
  JSON_VALID() function.

  The handler keeps track of how deeply nested the document is, and it
  raises an error and stops parsing when the depth exceeds
  JSON_DOCUMENT_MAX_DEPTH.
*/
class Syntax_check_handler
{
private:
  size_t m_depth;        ///< The current depth of the document

  bool seeing_scalar()
  {
    return !check_json_depth(m_depth + 1);
  }

public:
  Syntax_check_handler() : m_depth(0) {}

  /*
    These functions are callbacks used by rapidjson::Reader when
    parsing a JSON document. They all follow the rapidjson convention
    of returning true on success and false on failure.
  */
  bool StartObject() { return !check_json_depth(++m_depth); }
  bool EndObject(SizeType) { --m_depth; return true; }
  bool StartArray() { return !check_json_depth(++m_depth); }
  bool EndArray(SizeType) { --m_depth; return true; }
  bool Null() { return seeing_scalar(); }
  bool Bool(bool) { return seeing_scalar(); }
  bool Int(int) { return seeing_scalar(); }
  bool Uint(unsigned) { return seeing_scalar(); }
  bool Int64(int64_t) { return seeing_scalar(); }
  bool Uint64(uint64_t) { return seeing_scalar(); }
  bool Double(double, bool is_int= false) { return seeing_scalar(); }
  bool String(const char*, SizeType, bool) { return seeing_scalar(); }
  bool Key(const char*, SizeType, bool) { return seeing_scalar(); }
};


bool is_valid_json_syntax(const char *text, size_t length)
{
  Syntax_check_handler handler;
  Reader reader;
  MemoryStream ms(text, length);
  return reader.Parse<rapidjson::kParseDefaultFlags>(ms, handler);
}


/**
  Map the JSON type used by the binary representation to the type
  used by Json_dom and Json_wrapper.
  <p/>
  Note: Does not look into opaque values to determine if they
  represent decimal or date/time values. For that, look into the
  Value an retrive field_type.

  @param[in]  bintype
  @returns the JSON_dom JSON type.
*/
static Json_dom::enum_json_type
bjson2json(const json_binary::Value::enum_type bintype)
{
  Json_dom::enum_json_type res= Json_dom::J_ERROR;

  switch (bintype)
  {
  case json_binary::Value::STRING:
    res= Json_dom::J_STRING;
    break;
  case json_binary::Value::INT:
    res= Json_dom::J_INT;
    break;
  case json_binary::Value::UINT:
    res= Json_dom::J_UINT;
    break;
  case json_binary::Value::DOUBLE:
    res= Json_dom::J_DOUBLE;
    break;
  case json_binary::Value::LITERAL_TRUE:
  case json_binary::Value::LITERAL_FALSE:
    res= Json_dom::J_BOOLEAN;
    break;
  case json_binary::Value::LITERAL_NULL:
    res= Json_dom::J_NULL;
    break;
  case json_binary::Value::ARRAY:
    res= Json_dom::J_ARRAY;
    break;
  case json_binary::Value::OBJECT:
    res= Json_dom::J_OBJECT;
    break;
  case json_binary::Value::ERROR:
    res= Json_dom::J_ERROR;
    break;
  case json_binary::Value::OPAQUE:
    res= Json_dom::J_OPAQUE;
    break;
  }

  return res;
}


Json_dom *Json_dom::parse(const json_binary::Value &v)
{
  Json_dom *result= NULL;

  switch (v.type())
  {
  case json_binary::Value::OBJECT:
    {
      std::auto_ptr<Json_object> jo(new (std::nothrow) Json_object());
      if (jo.get() == NULL)
        return NULL;                            /* purecov: inspected */
      for (uint32 i= 0; i < v.element_count(); ++i)
      {
        /*
          Add the key/value pair. Json_object::add_alias() guarantees
          that the value is deallocated if it cannot be added.
        */
        if (jo->add_alias(std::string(v.key(i).get_data(),
                                      v.key(i).get_data_length()),
                          parse(v.element(i))))
        {
          return NULL;                        /* purecov: inspected */
        }
      }
      result= jo.release();
      break;
    }
  case json_binary::Value::ARRAY:
    {
      std::auto_ptr<Json_array> jarr(new (std::nothrow) Json_array());
      if (jarr.get() == NULL)
        return NULL;                          /* purecov: inspected */
      for (uint32 i= 0; i < v.element_count(); ++i)
      {
        /*
          Add the element to the array. We need to make sure it is
          deallocated if it cannot be added. std::auto_ptr does that
          for us.
        */
        std::auto_ptr<Json_dom> elt(parse(v.element(i)));
        if (jarr->append_alias(elt.get()))
          return NULL;                        /* purecov: inspected */
        // The array owns the element now. Release it.
        elt.release();
      }
      result= jarr.release();
      break;
    }
  case json_binary::Value::DOUBLE:
    result= new (std::nothrow) Json_double(v.get_double());
    break;
  case json_binary::Value::INT:
    result= new (std::nothrow) Json_int(v.get_int64());
    break;
  case json_binary::Value::UINT:
    result= new (std::nothrow) Json_uint(v.get_uint64());
    break;
  case json_binary::Value::LITERAL_FALSE:
    result= new (std::nothrow) Json_boolean(false);
    break;
  case json_binary::Value::LITERAL_TRUE:
    result= new (std::nothrow) Json_boolean(true);
    break;
  case json_binary::Value::LITERAL_NULL:
    result= new (std::nothrow) Json_null();
    break;
  case json_binary::Value::OPAQUE:
    {
      const enum_field_types ftyp= v.field_type();

      if (ftyp == MYSQL_TYPE_NEWDECIMAL)
      {
        my_decimal m;
        if (Json_decimal::convert_from_binary(v.get_data(),
                                              v.get_data_length(),
                                              &m))
          return NULL;                        /* purecov: inspected */
        result= new (std::nothrow) Json_decimal(m);
      }
      else if (ftyp == MYSQL_TYPE_DATE ||
               ftyp == MYSQL_TYPE_TIME ||
               ftyp == MYSQL_TYPE_DATETIME ||
               ftyp == MYSQL_TYPE_TIMESTAMP)
      {
        MYSQL_TIME t;
        Json_datetime::from_packed(v.get_data(), ftyp, &t);
        result= new (std::nothrow) Json_datetime(t, ftyp);
      }
      else
      {
        result= new (std::nothrow) Json_opaque(v.field_type(),
                                               v.get_data(),
                                               v.get_data_length());
      }
      break;
    }
  case json_binary::Value::STRING:
    result= new (std::nothrow) Json_string(std::string(v.get_data(),
                                                       v.get_data_length()));
    break;

  case json_binary::Value::ERROR:
    {
      /* purecov: begin inspected */
      DBUG_ABORT();
      my_error(ER_INVALID_JSON_BINARY_DATA, MYF(0));
      break;
      /* purecov: end inspected */
    }
  }

  return result;
}


void Json_array::replace_dom_in_container(Json_dom *oldv, Json_dom *newv)
{
  Json_dom_vector::iterator it= std::find(m_v.begin(), m_v.end(), oldv);
  if (it != m_v.end())
  {
    delete oldv;
    *it= newv;
    newv->set_parent(this);
  }
}


void Json_object::replace_dom_in_container(Json_dom *oldv, Json_dom *newv)
{
  for (Json_object_map::iterator it= m_map.begin(); it != m_map.end(); ++it)
  {
    if (it->second == oldv)
    {
      delete oldv;
      it->second= newv;
      newv->set_parent(this);
      break;
    }
  }
}


bool Json_object::add_clone(const std::string &key, const Json_dom *value)
{
  if (!value)
    return true;                                /* purecov: inspected */
  return add_alias(key, value->clone());
}


bool Json_object::add_alias(const std::string &key, Json_dom *value)
{
  if (!value)
    return true;                                /* purecov: inspected */

  /*
    Wrap value in an auto_ptr to make sure it's released if we cannot
    add it to the object. The contract of add_alias() requires that it
    either gets added to the object or gets deleted.
  */
  std::auto_ptr<Json_dom> aptr(value);

  /*
    We have already an element with this key.  Note we compare utf-8 bytes
    directly here. It's complicated when when you take into account composed
    and decomposed forms of accented characters and ligatures: different
    sequences might encode the same glyphs but we ignore that for now.  For
    example, the code point U+006E (the Latin lowercase "n") followed by
    U+0303 (the combining tilde) is defined by Unicode to be canonically
    equivalent to the single code point U+00F1 (the lowercase letter of the
    Spanish alphabet).  For now, users must normalize themselves to avoid
    element dups.

    This is what ECMAscript does also: "Two IdentifierName that are
    canonically equivalent according to the Unicode standard are not equal
    unless they are represented by the exact same sequence of code units (in
    other words, conforming ECMAScript implementations are only required to
    do bitwise comparison on IdentifierName values). The intent is that the
    incoming source text has been converted to normalised form C before it
    reaches the compiler." (ECMA-262 5.1 edition June 2011)

    See WL-2048 Add function for Unicode normalization
  */
  std::pair<Json_object_map::const_iterator, bool> ret=
    m_map.insert(std::make_pair(key, value));

  if (ret.second)
  {
    // the element was inserted
    value->set_parent(this);
    aptr.release();
  }

  return false;
}

bool Json_object::consume(Json_object *other)
{
  // We've promised to delete other before returning.
  std::auto_ptr<Json_object> aptr(other);

  Json_object_map &this_map= m_map;
  Json_object_map &other_map= other->m_map;

  for (Json_object_map::iterator other_iter= other_map.begin();
       other_iter != other_map.end(); other_map.erase(other_iter++))
  {
    const std::string &key= other_iter->first;
    Json_dom *value= other_iter->second;
    other_iter->second= NULL;

    Json_object_map::iterator this_iter= this_map.find(key);

    if (this_iter == this_map.end())
    {
      // The key does not exist in this object, so add the key/value pair.
      if (add_alias(key, value))
        return true;                          /* purecov: inspected */
    }
    else
    {
      /*
        Oops. Duplicate key. Merge the values.
        This is where the recursion in JSON_MERGE() occurs.
      */
      this_iter->second= merge_doms(this_iter->second, value);
      if (this_iter->second == NULL)
        return true;                          /* purecov: inspected */
      this_iter->second->set_parent(this);
    }
  }

  return false;
}

Json_dom *Json_object::get(const std::string &key) const
{
  const Json_object_map::const_iterator iter= m_map.find(key);

  if (iter != m_map.end())
  {
    DBUG_ASSERT(iter->second->parent() == this);
    return iter->second;
  }

  return NULL;
}


bool Json_object::remove(const Json_dom *child)
{
  for (Json_object_map::iterator iter= m_map.begin();
       iter != m_map.end(); ++iter)
  {
    Json_dom *candidate= iter->second;

    if (child == candidate)
    {
      delete candidate;
      m_map.erase(iter);
      return true;
    }
  } // end of loop through children

  return false;
}


bool Json_object::remove(const std::string &key)
{
  Json_object_map::iterator it= m_map.find(key);
  if (it == m_map.end())
    return false;

  delete it->second;
  m_map.erase(it);
  return true;
}


size_t Json_object::cardinality() const
{
  return m_map.size();
}


uint32 Json_object::depth() const
{
  uint deepest_child= 0;

  for (Json_object_map::const_iterator iter= m_map.begin();
       iter != m_map.end(); ++iter)
  {
    deepest_child= std::max(deepest_child, iter->second->depth());
  }
  return 1 + deepest_child;
}


Json_dom *Json_object::clone() const
{
  Json_object * const o= new (std::nothrow) Json_object();
  if (!o)
    return NULL;                                /* purecov: inspected */

  for (Json_object_map::const_iterator iter= m_map.begin();
       iter != m_map.end(); ++iter)
  {
    if (o->add_clone(iter->first, iter->second))
    {
      delete o;                                 /* purecov: inspected */
      return NULL;                              /* purecov: inspected */
    }
  }
  return o;
}


void Json_object::clear()
{
  for (Json_object_map::const_iterator iter= m_map.begin();
       iter != m_map.end(); ++iter)
  {
    delete iter->second;
  }
  m_map.clear();
}


bool Json_object::merge_patch(Json_object *patch)
{
  std::auto_ptr<Json_object> aptr(patch); // We own it, and must make sure
                                          // to delete it.

  for (Json_object_map::iterator it= patch->m_map.begin();
       it != patch->m_map.end(); ++it)
  {
    const std::string &patch_key= it->first;
    std::auto_ptr<Json_dom> patch_value(it->second);
    it->second= NULL;

    // Remove the member if the value in the patch is the null literal.
    if (patch_value->json_type() == Json_dom::J_NULL)
    {
      remove(patch_key);
      continue;
    }

    // See if the target has this member, add it if not.
    std::pair<Json_object_map::iterator, bool>
      target_pair= m_map.insert(std::make_pair(patch_key,
                                               static_cast<Json_dom*>(NULL)));

    std::auto_ptr<Json_dom> target_value(target_pair.first->second);
    target_pair.first->second= NULL;

    /*
      If the value in the patch is not an object and not the null
      literal, the new value is the patch.
    */
    if (patch_value->json_type() != Json_dom::J_OBJECT)
    {
      patch_value->set_parent(this);
      target_pair.first->second= patch_value.release();
      continue;
    }

    /*
      If there is no target value, or if the target value is not an
      object, use an empty object as the target value.
    */
    if (target_value.get() == NULL ||
        target_value->json_type() != Json_dom::J_OBJECT)
    {
      target_value.reset(new (std::nothrow) Json_object());
    }

    // Recursively merge the target value with the patch.
    Json_object *target_obj= down_cast<Json_object*>(target_value.get());
    if (target_obj == NULL ||
        target_obj->merge_patch(down_cast<Json_object*>(patch_value.release())))
      return true;                            /* purecov: inspected */

    target_value->set_parent(this);
    target_pair.first->second= target_value.release();
  }

  return false;
}


/**
  Compare two keys from a JSON object and determine whether or not the
  first key is less than the second key. key1 is considered less than
  key2 if

  a) key1 is shorter than key2, or if

  b) key1 and key2 have the same length, but different contents, and
  the first byte that differs has a smaller value in key1 than in key2

  Otherwise, key1 is not less than key2.

  @param key1 the first key to compare
  @param key2 the second key to compare
  @return true if key1 is considered less than key2, false otherwise
*/
bool Json_key_comparator::operator() (const std::string &key1,
                                      const std::string &key2) const
{
  if (key1.length() != key2.length())
    return key1.length() < key2.length();

  return memcmp(key1.data(), key2.data(), key1.length()) < 0;
}


Json_array::Json_array()
  : Json_dom(), m_v(key_memory_JSON)
{}


Json_array::Json_array(Json_dom *innards)
  : Json_dom(), m_v(key_memory_JSON)
{
  append_alias(innards);
}


Json_array::~Json_array()
{
  delete_container_pointers(m_v);
}


bool Json_array::append_clone(const Json_dom *value)
{
  if (!value)
    return true;                                /* purecov: inspected */
  return append_alias(value->clone());
}


bool Json_array::append_alias(Json_dom *value)
{
  if (!value || m_v.push_back(value))
    return true;                                /* purecov: inspected */
  value->set_parent(this);
  return false;
}


bool Json_array::consume(Json_array *other)
{
  // We've promised to delete other before returning.
  std::auto_ptr<Json_array> aptr(other);

  Json_dom_vector &other_vector= other->m_v;

  for (Json_dom_vector::iterator iter= other_vector.begin();
       iter != other_vector.end(); ++iter)
  {
    if (append_alias(*iter))
      return true;                              /* purecov: inspected */
    *iter= NULL;
  }

  return false;
}


bool Json_array::insert_clone(size_t index, const Json_dom *value)
{
  if (!value)
    return true;                                /* purecov: inspected */
  return insert_alias(index, value->clone());
}


bool Json_array::insert_alias(size_t index, Json_dom *value)
{
  if (!value)
    return true;                                /* purecov: inspected */

  Json_dom_vector::iterator iter= m_v.begin();

  if (index < m_v.size())
  {
    m_v.insert(iter + index, value);
  }
  else
  {
    //append needed
    if (m_v.push_back(value))
      return true;                              /* purecov: inspected */
  }

  value->set_parent(this);
  return false;
}


bool Json_array::remove(size_t index)
{
  if (index < m_v.size())
  {
    const Json_dom_vector::iterator iter= m_v.begin() + index;
    delete *iter;
    m_v.erase(iter);
    return true;
  }

  return false;
}


bool Json_array::remove(const Json_dom *child)
{
  Json_dom_vector::iterator it= std::find(m_v.begin(), m_v.end(), child);
  if (it != m_v.end())
  {
    delete child;
    m_v.erase(it);
    return true;
  }

  return false;
}


uint32 Json_array::depth() const
{
  uint deepest_child= 0;

  for (Json_dom_vector::const_iterator it= m_v.begin(); it != m_v.end(); ++it)
  {
    deepest_child= std::max(deepest_child, (*it)->depth());
  }
  return 1 + deepest_child;
}

Json_dom *Json_array::clone() const
{
  Json_array * const vv= new (std::nothrow) Json_array();
  if (!vv)
    return NULL;                                /* purecov: inspected */

  for (Json_dom_vector::const_iterator it= m_v.begin(); it != m_v.end(); ++it)
  {
    if (vv->append_clone(*it))
    {
      delete vv;                                /* purecov: inspected */
      return NULL;                              /* purecov: inspected */
    }
  }

  return vv;
}


void Json_array::clear()
{
  delete_container_pointers(m_v);
}


/**
  Perform quoting on a JSON string to make an external representation
  of it. it wraps double quotes (text quotes) around the string (cptr)
  an also performs escaping according to the following table:
  <pre>
  Common name     C-style  Original unescaped     Transformed to
                  escape   UTF-8 bytes            escape sequence
                  notation                        in UTF-8 bytes
  ---------------------------------------------------------------
  quote           \"       %x22                    %x5C %x22
  backslash       \\       %x5C                    %x5C %x5C
  backspace       \b       %x08                    %x5C %x62
  formfeed        \f       %x0C                    %x5C %x66
  linefeed        \n       %x0A                    %x5C %x6E
  carriage-return \r       %x0D                    %x5C %x72
  tab             \t       %x09                    %x5C %x74
  unicode         \uXXXX  A hex number in the      %x5C %x75
                          range of 00-1F,          followed by
                          except for the ones      4 hex digits
                          handled above (backspace,
                          formfeed, linefeed,
                          carriage-return,
                          and tab).
  ---------------------------------------------------------------
  </pre>

  @param[in] cptr pointer to string data
  @param[in] length the length of the string
  @param[in,out] buf the destination buffer
  @retval false on success
  @retval true on error
*/
bool double_quote(const char *cptr, size_t length, String *buf)
{
  if (buf->append('"'))
    return true;                              /* purecov: inspected */

  for (size_t i= 0; i < length; i++)
  {
    char esc[2]= {'\\', cptr[i]};
    bool done= true;
    switch (cptr[i])
    {
    case '"' :
    case '\\' :
      break;
    case '\b':
      esc[1]= 'b';
      break;
    case '\f':
      esc[1]= 'f';
      break;
    case '\n':
      esc[1]= 'n';
      break;
    case '\r':
      esc[1]= 'r';
      break;
    case '\t':
      esc[1]= 't';
      break;
    default:
      done= false;
    }

    if (done)
    {
      if (buf->append(esc[0]) || buf->append(esc[1]))
        return true;                          /* purecov: inspected */
    }
    else if (((cptr[i] & ~0x7f) == 0) && // bit 8 not set
             (cptr[i] <= 0x1f))
    {
      /*
        Unprintable control character, use hex a hexadecimal number.
        The meaning of such a number determined by ISO/IEC 10646.
      */
      if (buf->append("\\u00") ||
          buf->append(_dig_vec_lower[(cptr[i] & 0xf0) >> 4]) ||
          buf->append(_dig_vec_lower[(cptr[i] & 0x0f)]))
        return true;                          /* purecov: inspected */
    }
    else if (buf->append(cptr[i]))
    {
      return true;                            /* purecov: inspected */
    }
  }
  return buf->append('"');
}


Json_decimal::Json_decimal(const my_decimal &value)
  : Json_number(), m_dec(value)
{}


int Json_decimal::binary_size() const
{
  /*
    We need two bytes for the precision and the scale, plus whatever
    my_decimal2binary() needs.
  */
  return 2 + my_decimal_get_binary_size(m_dec.precision(), m_dec.frac);
}


bool Json_decimal::get_binary(char* dest) const
{
  DBUG_ASSERT(binary_size() <= MAX_BINARY_SIZE);
  /*
    my_decimal2binary() loses the precision and the scale, so store them
    in the first two bytes.
  */
  dest[0]= static_cast<char>(m_dec.precision());
  dest[1]= static_cast<char>(m_dec.frac);
  // Then store the decimal value.
  return my_decimal2binary(E_DEC_ERROR, &m_dec,
                           pointer_cast<uchar*>(dest) + 2,
                           m_dec.precision(), m_dec.frac) != E_DEC_OK;
}


bool Json_decimal::convert_from_binary(const char *bin, size_t len,
                                       my_decimal *dec)
{
  // Expect at least two bytes, which contain precision and scale.
  bool error= (len < 2);

  if (!error)
  {
    int precision= bin[0];
    int scale= bin[1];

    // The decimal value is encoded after the two precision/scale bytes.
    size_t bin_size= my_decimal_get_binary_size(precision, scale);
    error=
      (bin_size != len - 2) ||
      (binary2my_decimal(E_DEC_ERROR,
                         pointer_cast<const uchar*>(bin) + 2,
                         dec, precision, scale) != E_DEC_OK);
  }

  if (error)
    my_error(ER_INVALID_JSON_BINARY_DATA, MYF(0)); /* purecov: inspected */

  return error;
}


Json_dom *Json_double::clone() const
{
  return new (std::nothrow) Json_double(m_f);
}


Json_dom::enum_json_type Json_datetime::json_type () const
{
  switch (m_field_type)
  {
  case MYSQL_TYPE_TIME : return J_TIME;
  case MYSQL_TYPE_DATETIME: return J_DATETIME;
  case MYSQL_TYPE_DATE: return J_DATE;
  case MYSQL_TYPE_TIMESTAMP: return J_TIMESTAMP;
  default: ;
  }
  /* purecov: begin inspected */
  DBUG_ABORT();
  return J_NULL;
  /* purecov: end inspected */
}


Json_dom *Json_datetime::clone() const
{
  return new (std::nothrow) Json_datetime(m_t, m_field_type);
}


void Json_datetime::to_packed(char *dest) const
{
  longlong packed= TIME_to_longlong_packed(&m_t);
  int8store(dest, packed);
}


void Json_datetime::from_packed(const char *from, enum_field_types ft,
                                MYSQL_TIME *to)
{
  TIME_from_longlong_packed(to, ft, sint8korr(from));
}


Json_opaque::Json_opaque(enum_field_types mytype, const char *v, size_t size)
  : Json_scalar(), m_mytype(mytype), m_val(v, size)
{}


Json_dom *Json_opaque::clone() const
{
  return new (std::nothrow) Json_opaque(m_mytype, value(), size());
}


Json_wrapper_object_iterator::
Json_wrapper_object_iterator(const Json_object *obj)
  : m_is_dom(true), m_iter(obj->begin()), m_end(obj->end()), m_element_count(-1)
{}


Json_wrapper_object_iterator::
Json_wrapper_object_iterator(const json_binary::Value *value)
  : m_is_dom(false), m_element_count(value->element_count()), m_value(value)
{
  m_curr_element= 0;
}


bool Json_wrapper_object_iterator::empty() const
{
  return m_is_dom ? (m_iter == m_end) : (m_curr_element >= m_element_count);
}


void Json_wrapper_object_iterator::next()
{
  if (m_is_dom)
  {
    m_iter++;
  }
  else
  {
    ++m_curr_element;
  }
}


std::pair<const std::string, Json_wrapper>
Json_wrapper_object_iterator::elt() const
{
  if (m_is_dom)
  {
    Json_wrapper wr(m_iter->second);
    // DOM possibly owned by object and we don't want to make a clone
    wr.set_alias();
    return std::make_pair(m_iter->first, wr);
  }

  std::string key(m_value->key(m_curr_element).get_data(),
                  m_value->key(m_curr_element).get_data_length());
  Json_wrapper wr(m_value->element(m_curr_element));
  return std::make_pair(key, wr);
}


Json_wrapper::Json_wrapper(Json_dom *dom_value)
  : m_is_dom(true), m_dom_alias(false), m_value(), m_dom_value(dom_value)
{
  if (!dom_value)
  {
    m_dom_alias= true; //!< no deallocation, make us empty
  }
}


void Json_wrapper::steal(Json_wrapper *old)
{
  if (old->m_is_dom)
  {
    bool old_is_aliased= old->m_dom_alias;
    old->m_dom_alias= true; // we want no deep copy now, or later
    *this= *old;
    this->m_dom_alias= old_is_aliased; // set it back
    // old is now marked as aliased, so any ownership is effectively
    // transferred to this.
  }
  else
  {
    *this= *old;
  }
}

Json_wrapper::Json_wrapper(const json_binary::Value &value)
  : m_is_dom(false), m_dom_alias(false), m_value(value), m_dom_value(NULL)
{}


Json_wrapper::Json_wrapper(const Json_wrapper &old) :
  m_is_dom(old.m_is_dom),
  m_dom_alias(old.m_dom_alias),
  m_value(old.m_value),
  m_dom_value(old.m_is_dom ?
              (m_dom_alias? old.m_dom_value : old.m_dom_value->clone()) :
              NULL)
{}


Json_wrapper::~Json_wrapper()
{
  if (m_is_dom && !m_dom_alias)
  {
    // we own our own copy, so we are responsible for deallocation
    delete m_dom_value;
  }
}


Json_wrapper &Json_wrapper::operator=(const Json_wrapper& from)
{
  if (this == &from)
  {
    return *this;   // self assignment: no-op
  }

  if (m_is_dom && !m_dom_alias &&!empty())
  {
    // we own our own copy, so we are responsible for deallocation
    delete m_dom_value;
  }

  m_is_dom= from.m_is_dom;

  if (from.m_is_dom)
  {
    if (from.m_dom_alias)
    {
      m_dom_value= from.m_dom_value;
    }
    else
    {
      m_dom_value= from.m_dom_value->clone();
    }

    m_dom_alias= from.m_dom_alias;
  }
  else
  {
    m_dom_value= NULL;
    m_value= from.m_value;
  }

  return *this;
}


Json_dom *Json_wrapper::to_dom()
{
  if (!m_is_dom)
  {
    // Build a DOM from the binary JSON value and
    // convert this wrapper to hold the DOM instead
    m_dom_value= Json_dom::parse(m_value);
    m_is_dom= true;
    m_dom_alias= false;
  }

  return m_dom_value;
}


Json_dom *Json_wrapper::clone_dom()
{
  // If we already have a DOM, return a clone of it.
  if (m_is_dom)
    return m_dom_value ? m_dom_value->clone() : NULL;

  // Otherwise, produce a new DOM tree from the binary representation.
  return Json_dom::parse(m_value);
}


bool Json_wrapper::to_binary(String *str) const
{
  if (empty())
  {
    /* purecov: begin inspected */
    my_error(ER_INVALID_JSON_BINARY_DATA, MYF(0));
    return true;
    /* purecov: end */
  }

  if (m_is_dom)
    return json_binary::serialize(m_dom_value, str);

  return m_value.raw_binary(str);
}


/**
  Possibly append a single quote to a buffer.
  @param[in,out] buffer receiving buffer
  @param[in] json_quoted whether or not a quote should be appended
  @return false if successful, true on error
*/
inline bool single_quote(String *buffer, bool json_quoted)
{
  return json_quoted && buffer->append('"');
}

/**
   Pretty-print a string to an evolving buffer, double-quoting if
   requested.

   @param[in] buffer the buffer to print to
   @param[in] json_quoted true if we should double-quote
   @param[in] data the string to print
   @param[in] length the string's length
   @return false on success, true on failure
*/
static int print_string(String *buffer, bool json_quoted,
                        const char *data, size_t length)
{
  return json_quoted ?
    double_quote(data, length, buffer) :
    buffer->append(data, length);
}


/**
  Helper function for wrapper_to_string() which adds a newline and indentation
  up to the specified level.

  @param[in,out] buffer  the buffer to write to
  @param[in]     level   how many nesting levels to add indentation for
  @retval false on success
  @retval true on error
*/
static bool newline_and_indent(String *buffer, size_t level)
{
  // Append newline and two spaces per indentation level.
  return buffer->append('\n') ||
    buffer->fill(buffer->length() + level * 2, ' ');
}


/**
  Helper function which does all the heavy lifting for
  Json_wrapper::to_string(). It processes the Json_wrapper
  recursively. The depth parameter keeps track of the current nesting
  level. When it reaches JSON_DOCUMENT_MAX_DEPTH, it gives up in order
  to avoid running out of stack space.

  @param[in]     wr          the value to convert to a string
  @param[in,out] buffer      the buffer to write to
  @param[in]     json_quoted quote strings if true
  @param[in]     pretty      add newlines and indentation if true
  @param[in]     func_name   the name of the calling function
  @param[in]     depth       the nesting level of @a wr

  @retval false on success
  @retval true on error
*/
static bool wrapper_to_string(const Json_wrapper &wr, String *buffer,
                              bool json_quoted, bool pretty,
                              const char *func_name, size_t depth)
{
  if (check_json_depth(++depth))
    return true;

  switch (wr.type())
  {
  case Json_dom::J_TIME:
  case Json_dom::J_DATE:
  case Json_dom::J_DATETIME:
  case Json_dom::J_TIMESTAMP:
    {
      // Make sure the buffer has space for the datetime and the quotes.
      if (buffer->reserve(MAX_DATE_STRING_REP_LENGTH + 2))
        return true;                           /* purecov: inspected */
      MYSQL_TIME t;
      wr.get_datetime(&t);
      if (single_quote(buffer, json_quoted))
        return true;                           /* purecov: inspected */
      char *ptr= const_cast<char *>(buffer->ptr()) + buffer->length();
      const int size= my_TIME_to_str(&t, ptr, 6);
      buffer->length(buffer->length() + size);
      if (single_quote(buffer, json_quoted))
        return true;                           /* purecov: inspected */
      break;
    }
  case Json_dom::J_ARRAY:
    {
      if (buffer->append('['))
        return true;                           /* purecov: inspected */

      size_t array_len= wr.length();
      for (uint32 i= 0; i < array_len; ++i)
      {
        if (i > 0 && buffer->append(pretty ? "," : ", "))
          return true;                         /* purecov: inspected */

        if (pretty && newline_and_indent(buffer, depth))
          return true;                         /* purecov: inspected */

        if (wrapper_to_string(wr[i], buffer, true, pretty, func_name, depth))
          return true;                         /* purecov: inspected */
      }

      if (pretty && array_len > 0 && newline_and_indent(buffer, depth - 1))
        return true;                           /* purecov: inspected */

      if (buffer->append(']'))
        return true;                           /* purecov: inspected */

      break;
    }
  case Json_dom::J_BOOLEAN:
    if (buffer->append(wr.get_boolean() ? "true" : "false"))
      return true;                             /* purecov: inspected */
    break;
  case Json_dom::J_DECIMAL:
    {
      int length= DECIMAL_MAX_STR_LENGTH + 1;
      if (buffer->reserve(length))
        return true;                           /* purecov: inspected */
      char *ptr= const_cast<char *>(buffer->ptr()) + buffer->length();
      my_decimal m;
      if (wr.get_decimal_data(&m) ||
          decimal2string(&m, ptr, &length, 0, 0, 0))
        return true;                           /* purecov: inspected */
      buffer->length(buffer->length() + length);
      break;
    }
  case Json_dom::J_DOUBLE:
    {
      if (buffer->reserve(MY_GCVT_MAX_FIELD_WIDTH + 1))
        return true;                           /* purecov: inspected */
      double d= wr.get_double();
      size_t len= my_gcvt(d, MY_GCVT_ARG_DOUBLE, MY_GCVT_MAX_FIELD_WIDTH,
                          const_cast<char *>(buffer->ptr()) + buffer->length(),
                          NULL);
      buffer->length(buffer->length() + len);
      break;
    }
  case Json_dom::J_INT:
    {
      if (buffer->append_longlong(wr.get_int()))
        return true;                           /* purecov: inspected */
      break;
    }
  case Json_dom::J_NULL:
    if (buffer->append("null"))
      return true;                             /* purecov: inspected */
    break;
  case Json_dom::J_OBJECT:
    {
      if (buffer->append('{'))
        return true;                           /* purecov: inspected */

      bool first= true;
      for (Json_wrapper_object_iterator iter= wr.object_iterator();
           !iter.empty(); iter.next())
      {
        if (!first && buffer->append(pretty ? "," : ", "))
          return true;                         /* purecov: inspected */

        first= false;

        if (pretty && newline_and_indent(buffer, depth))
          return true;                         /* purecov: inspected */

        const std::string &key= iter.elt().first;
        const char *key_data= key.c_str();
        size_t key_length= key.length();
        if (print_string(buffer, true, key_data, key_length) ||
            buffer->append(": ") ||
            wrapper_to_string(iter.elt().second, buffer, true, pretty,
                              func_name, depth))
          return true;                         /* purecov: inspected */
      }

      if (pretty && wr.length() > 0 && newline_and_indent(buffer, depth - 1))
        return true;                           /* purecov: inspected */

      if (buffer->append('}'))
        return true;                           /* purecov: inspected */

      break;
    }
  case Json_dom::J_OPAQUE:
    {
      if (wr.get_data_length() > base64_encode_max_arg_length())
      {
        /* purecov: begin inspected */
        buffer->append("\"<data too long to decode - unexpected error>\"");
        my_error(ER_INTERNAL_ERROR, MYF(0),
                 "JSON: could not decode opaque data");
        return true;
        /* purecov: end */
      }

      const size_t needed=
        static_cast<size_t>(base64_needed_encoded_length(wr.get_data_length()));

      if (single_quote(buffer, json_quoted) ||
          buffer->append("base64:type") ||
          buffer->append_ulonglong(wr.field_type()) ||
          buffer->append(':'))
        return true;                           /* purecov: inspected */

      // "base64:typeXX:<binary data>"
      size_t pos= buffer->length();
      if (buffer->reserve(needed) ||
          base64_encode(wr.get_data(), wr.get_data_length(),
                        const_cast<char*>(buffer->ptr() + pos)))
        return true;                           /* purecov: inspected */
      buffer->length(pos + needed - 1); // drop zero terminator space
      if (single_quote(buffer, json_quoted))
        return true;                           /* purecov: inspected */
      break;
    }
  case Json_dom::J_STRING:
    {
      const char *data= wr.get_data();
      size_t length= wr.get_data_length();

      if (print_string(buffer, json_quoted, data, length))
        return true;                           /* purecov: inspected */
      break;
    }
  case Json_dom::J_UINT:
    {
      if (buffer->append_ulonglong(wr.get_uint()))
        return true;                           /* purecov: inspected */
      break;
    }
  default:
    /* purecov: begin inspected */
    DBUG_ABORT();
    my_error(ER_INTERNAL_ERROR, MYF(0), "JSON wrapper: unexpected type");
    return true;
    /* purecov: end inspected */
  }

  if (buffer->length() > current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER_THD(current_thd, ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name, current_thd->variables.max_allowed_packet);
    return true;
  }

  return false;
}


bool Json_wrapper::to_string(String *buffer, bool json_quoted,
                             const char *func_name) const
{
  buffer->set_charset(&my_charset_utf8mb4_bin);
  return wrapper_to_string(*this, buffer, json_quoted, false, func_name, 0);
}


bool Json_wrapper::to_pretty_string(String *buffer, const char *func_name) const
{
  buffer->set_charset(&my_charset_utf8mb4_bin);
  return wrapper_to_string(*this, buffer, true, true, func_name, 0);
}


Json_dom::enum_json_type Json_wrapper::type() const
{
  if (empty())
  {
    return Json_dom::J_ERROR;
  }

  if (m_is_dom)
  {
    return m_dom_value->json_type();
  }

  json_binary::Value::enum_type typ= m_value.type();

  if (typ == json_binary::Value::OPAQUE)
  {
    const enum_field_types ftyp= m_value.field_type();

    switch (ftyp)
    {
    case MYSQL_TYPE_NEWDECIMAL:
      return Json_dom::J_DECIMAL;
    case MYSQL_TYPE_DATETIME:
      return Json_dom::J_DATETIME;
    case MYSQL_TYPE_DATE:
      return Json_dom::J_DATE;
    case MYSQL_TYPE_TIME:
      return Json_dom::J_TIME;
    case MYSQL_TYPE_TIMESTAMP:
      return Json_dom::J_TIMESTAMP;
    default: ;
      // ok, fall through
    }
  }

  return bjson2json(typ);
}


enum_field_types Json_wrapper::field_type() const
{
  if (m_is_dom)
  {
    return down_cast<Json_opaque *>(m_dom_value)->type();
  }

  return m_value.field_type();
}


Json_wrapper_object_iterator Json_wrapper::object_iterator() const
{
  DBUG_ASSERT(type() == Json_dom::J_OBJECT);

  if (m_is_dom)
  {
    const Json_object *o= down_cast<const Json_object *>(m_dom_value);
    return Json_wrapper_object_iterator(o);
  }

  return Json_wrapper_object_iterator(&m_value);
}


Json_wrapper Json_wrapper::lookup(const char *key, size_t len) const
{
  DBUG_ASSERT(type() == Json_dom::J_OBJECT);
  if (m_is_dom)
  {
    const Json_object *object= down_cast<const Json_object *>(m_dom_value);
    std::string member_name(key, len);
    Json_wrapper wr(object->get(member_name));
    wr.set_alias(); // wr doesn't own the supplied DOM: part of array DOM
    return wr;
  }

  return Json_wrapper(m_value.lookup(key, len));
}

Json_wrapper Json_wrapper::operator[](size_t index) const
{
  DBUG_ASSERT(type() == Json_dom::J_ARRAY);
  if (m_is_dom)
  {
    const Json_array *o= down_cast<const Json_array *>(m_dom_value);
    Json_wrapper wr((*o)[index]);
    wr.set_alias(); // wr doesn't own the supplied DOM: part of array DOM
    return wr;
  }

  return Json_wrapper(m_value.element(index));
}


const char *Json_wrapper::get_data() const
{
  if (m_is_dom)
  {
    return type() == Json_dom::J_STRING ?
      down_cast<Json_string *>(m_dom_value)->value().c_str() :
      down_cast<Json_opaque *>(m_dom_value)->value();
  }

  return m_value.get_data();
}


size_t Json_wrapper::get_data_length() const
{
  if (m_is_dom)
  {
    return type() == Json_dom::J_STRING ?
      down_cast<Json_string *>(m_dom_value)->size() :
      down_cast<Json_opaque *>(m_dom_value)->size();
  }

  return m_value.get_data_length();
}


bool Json_wrapper::get_decimal_data(my_decimal *d) const
{
  if (m_is_dom)
  {
    *d= *down_cast<Json_decimal *>(m_dom_value)->value();
    return false;
  }

  return Json_decimal::convert_from_binary(m_value.get_data(),
                                           m_value.get_data_length(),
                                           d);
}


double Json_wrapper::get_double() const
{
  if (m_is_dom)
  {
    return down_cast<Json_double *>(m_dom_value)->value();
  }

  return m_value.get_double();
}


longlong Json_wrapper::get_int() const
{
  if (m_is_dom)
  {
    return down_cast<Json_int *>(m_dom_value)->value();
  }

  return m_value.get_int64();
}


ulonglong Json_wrapper::get_uint() const
{
  if (m_is_dom)
  {
    return down_cast<Json_uint *>(m_dom_value)->value();
  }

  return m_value.get_uint64();
}


void Json_wrapper::get_datetime(MYSQL_TIME *t) const
{
  enum_field_types ftyp= MYSQL_TYPE_NULL;

  switch(type())
  {
  case Json_dom::J_DATE:
    ftyp= MYSQL_TYPE_DATE;
    break;
  case Json_dom::J_DATETIME:
  case Json_dom::J_TIMESTAMP:
    ftyp= MYSQL_TYPE_DATETIME;
    break;
  case Json_dom::J_TIME:
    ftyp= MYSQL_TYPE_TIME;
    break;
  default:
    DBUG_ABORT();                               /* purecov: inspected */
  }

  if (m_is_dom)
  {
    *t= *down_cast<Json_datetime *>(m_dom_value)->value();
  }
  else
  {
    Json_datetime::from_packed(m_value.get_data(), ftyp, t);
  }
}


const char *Json_wrapper::get_datetime_packed(char *buffer) const
{
  if (m_is_dom)
  {
    down_cast<Json_datetime *>(m_dom_value)->to_packed(buffer);
    return buffer;
  }

  DBUG_ASSERT(m_value.get_data_length() == Json_datetime::PACKED_SIZE);
  return m_value.get_data();
}


bool Json_wrapper::get_boolean() const
{
  if (m_is_dom)
  {
    return down_cast<Json_boolean *>(m_dom_value)->value();
  }

  return m_value.type() == json_binary::Value::LITERAL_TRUE;
}


Json_path Json_dom::get_location()
{
  if (m_parent == NULL)
  {
    Json_path result;
    return result;
  }

  Json_path result= m_parent->get_location();

  if (m_parent->json_type() == Json_dom::J_OBJECT)
  {
    Json_object *object= down_cast<Json_object *>(m_parent);
    for (Json_object::const_iterator it= object->begin();
         it != object->end(); ++it)
    {
      if (it->second == this)
      {
        Json_path_leg child_leg(it->first);
        result.append(child_leg);
        break;
      }
    }
  }
  else
  {
    DBUG_ASSERT(m_parent->json_type() == Json_dom::J_ARRAY);
    Json_array *array= down_cast<Json_array *>(m_parent);

    for (size_t idx= 0; idx < array->size(); idx++)
    {
      if ((*array)[idx] == this)
      {
        Json_path_leg child_leg(idx);
        result.append(child_leg);
        break;
      }
    }
  }

  return result;
}


bool Json_dom::seek(const Json_seekable_path &path,
                    Json_dom_vector *hits,
                    bool auto_wrap, bool only_need_one)
{
  Json_dom_vector candidates(key_memory_JSON);
  Json_dom_vector duplicates(key_memory_JSON);

  if (hits->push_back(this))
    return true;                              /* purecov: inspected */

  size_t path_leg_count= path.leg_count();
  for (size_t path_idx= 0; path_idx < path_leg_count; path_idx++)
  {
    const Json_path_leg *path_leg= path.get_leg_at(path_idx);
    duplicates.clear();
    candidates.clear();

    for (Json_dom_vector::iterator it= hits->begin(); it != hits->end(); ++it)
    {
      if ((*it)->find_child_doms(path_leg, auto_wrap,
                                 (only_need_one &&
                                  (path_idx == (path_leg_count-1))),
                                 &duplicates, &candidates))
        return true;                          /* purecov: inspected */
    }

    // swap the two lists so that they can be re-used
    hits->swap(candidates);
  }

  return false;
}


bool Json_wrapper::seek_no_ellipsis(const Json_seekable_path &path,
                                    Json_wrapper_vector *hits,
                                    const size_t leg_number,
                                    bool auto_wrap,
                                    bool only_need_one) const
{
  if (leg_number >= path.leg_count())
  {
    if (m_is_dom)
    {
      Json_wrapper clone(m_dom_value->clone());
      if (clone.empty() || hits->push_back(Json_wrapper()))
        return true;                          /* purecov: inspected */
      hits->back().steal(&clone);
      return false;
    }
    return hits->push_back(*this);
  }

  const Json_path_leg *path_leg= path.get_leg_at(leg_number);

  switch(path_leg->get_type())
  {
  case jpl_member:
    {
      switch(this->type())
      {
      case Json_dom::J_OBJECT:
        {
          const char *key= path_leg->get_member_name();
          size_t key_length= path_leg->get_member_name_length();
          Json_wrapper member= lookup(key, key_length);

          if (!member.empty() & !(member.type() == Json_dom::J_ERROR))
          {
            // recursion
            if (member.seek_no_ellipsis(path, hits, leg_number + 1, auto_wrap,
                                        only_need_one))
              return true;                    /* purecov: inspected */
          }
          return false;
        }

      default:
        {
          return false;
        }
      } // end inner switch on wrapper type
    }

  case jpl_member_wildcard:
    {
      switch(this->type())
      {
      case Json_dom::J_OBJECT:
        {
          for (Json_wrapper_object_iterator iter= object_iterator();
               !iter.empty(); iter.next())
          {
            if (is_seek_done(hits, only_need_one))
              return false;

            // recursion
            if (iter.elt().second.seek_no_ellipsis(path,
                                                   hits,
                                                   leg_number + 1, auto_wrap,
                                                   only_need_one))
              return true;                    /* purecov: inspected */
          }
          return false;
        }

      default:
        {
          return false;
        }
      } // end inner switch on wrapper type
    }

  case jpl_array_cell:
    {
      size_t cell_idx= path_leg->get_array_cell_index();

      // handle auto-wrapping
      if ((cell_idx == 0) &&
          auto_wrap &&
          (this->type() != Json_dom::J_ARRAY))
      {
        // recursion
        return seek_no_ellipsis(path, hits, leg_number + 1, auto_wrap,
                                only_need_one);
      }

      switch(this->type())
      {
      case Json_dom::J_ARRAY:
        {
          if (cell_idx < this->length())
          {
            Json_wrapper cell= (*this)[cell_idx];
            return cell.seek_no_ellipsis(path, hits, leg_number + 1, auto_wrap,
                                         only_need_one);
          }
          return false;
        }

      default:
        {
          return false;
        }
      } // end inner switch on wrapper type
    }

  case jpl_array_cell_wildcard:
    {
      switch(this->type())
      {
      case Json_dom::J_ARRAY:
        {
          size_t  array_length= this->length();
          for (size_t idx= 0; idx < array_length; idx++)
          {
            if (is_seek_done(hits, only_need_one))
              return false;

            // recursion
            Json_wrapper cell= (*this)[idx];
            if (cell.seek_no_ellipsis(path, hits, leg_number + 1, auto_wrap,
                                      only_need_one))
              return true;                    /* purecov: inspected */
          }
          return false;
        }

      default:
        {
          return false;
        }
      } // end inner switch on wrapper type
    }

  default:
    // should never be called on a path which contains an ellipsis
    DBUG_ABORT();                               /* purecov: inspected */
    return true;                                /* purecov: inspected */
  } // end outer switch on leg type
}


bool Json_wrapper::seek(const Json_seekable_path &path,
                        Json_wrapper_vector *hits,
                        bool auto_wrap, bool only_need_one)
{
  if (empty())
  {
    /* purecov: begin inspected */
    DBUG_ABORT();
    return false;
    /* purecov: end */
  }

  // use fast-track code if the path doesn't have any ellipses
  if (!path.contains_ellipsis())
  {
    return seek_no_ellipsis(path, hits, 0, auto_wrap, only_need_one);
  }

  /*
    FIXME.

    Materialize the dom if the path contains ellipses. Duplicate
    detection is difficult on binary values.
   */
  to_dom();

  Json_dom_vector dhits(key_memory_JSON);
  if (m_dom_value->seek(path, &dhits, auto_wrap, only_need_one))
    return true;                              /* purecov: inspected */
  for (Json_dom_vector::iterator it= dhits.begin(); it != dhits.end(); ++it)
  {
    Json_wrapper clone((*it)->clone());
    if (clone.empty() || hits->push_back(Json_wrapper()))
      return true;                            /* purecov: inspected */
    hits->back().steal(&clone);
  }

  return false;
}


size_t Json_wrapper::length() const
{
  if (empty())
  {
    return 0;
  }

  if (m_is_dom)
  {
    switch(m_dom_value->json_type())
    {
    case Json_dom::J_ARRAY:
      return down_cast<Json_array *>(m_dom_value)->size();
    case Json_dom::J_OBJECT:
      return down_cast<Json_object *>(m_dom_value)->cardinality();
    default:
      return 1;
    }
  }

  switch(m_value.type())
  {
  case json_binary::Value::ARRAY:
  case json_binary::Value::OBJECT:
    return m_value.element_count();
  default:
    return 1;
  }
}


size_t Json_wrapper::depth() const
{
  if (empty())
  {
    return 0;
  }

  if (m_is_dom)
  {
    return m_dom_value->depth();
  }

  Json_dom *d= Json_dom::parse(m_value);
  size_t result= d->depth();
  delete d;
  return result;
}


/**
  Compare two numbers of the same type.
  @param val1 the first number
  @param val2 the second number
  @retval -1 if val1 is less than val2,
  @retval 0 if val1 is equal to val2,
  @retval 1 if val1 is greater than val2
*/
template <class T> static int compare_numbers(T val1, T val2)
{
  return (val1 < val2) ? -1 : ((val1 == val2) ? 0 : 1);
}


/**
  Compare a decimal value to a double by converting the double to a
  decimal.
  @param a the decimal value
  @param b the double value
  @return -1 if a is less than b,
          0 if a is equal to b,
          1 if a is greater than b
*/
static int compare_json_decimal_double(const my_decimal &a, double b)
{
  /*
    First check the sign of the two values. If they differ, the
    negative value is the smaller one.
  */
  const bool a_is_zero= my_decimal_is_zero(&a);
  const bool a_is_negative= a.sign() && !a_is_zero;
  const bool b_is_negative= (b < 0);
  if (a_is_negative != b_is_negative)
    return a_is_negative ? -1 : 1;

  // Both arguments have the same sign. Compare their values.

  const bool b_is_zero= b == 0;
  if (a_is_zero)
    // b is non-negative, so it is either equal to or greater than a.
    return b_is_zero ? 0 : -1;

  if (b_is_zero)
    // a is positive and non-zero, so it is greater than b.
    return 1;

  my_decimal b_dec;
  switch (double2decimal(b, &b_dec))
  {
  case E_DEC_OK:
    return my_decimal_cmp(&a, &b_dec);
  case E_DEC_OVERFLOW:
    /*
      b is too big to fit in a DECIMAL, so it must have a
      larger absolute value than a, which is a DECIMAL.
    */
    return a_is_negative ? 1 : -1;
  case E_DEC_TRUNCATED:
    /*
      b was truncated to fit in a DECIMAL, which means that b_dec is
      closer to zero than b.
    */
    {
      int cmp= my_decimal_cmp(&a, &b_dec);

      /*
        If the truncated b_dec is equal to a, a must be closer to zero
        than b.
      */
      if (cmp == 0)
        return a_is_negative ? 1 : -1;

      return cmp;
    }
  default:
    /*
      double2decimal() is not supposed to return anything other than
      E_DEC_OK, E_DEC_OVERFLOW or E_DEC_TRUNCATED, so this should
      never happen.
    */
    DBUG_ABORT();                             /* purecov: inspected */
    return 1;                                 /* purecov: inspected */
  }
}


/**
  Compare a decimal value to a signed integer by converting the
  integer to a decimal.
  @param a the decimal value
  @param b the signed integer value
  @return -1 if a is less than b,
          0 if a is equal to b,
          1 if a is greater than b
*/
static int compare_json_decimal_int(const my_decimal &a, longlong b)
{
  if (my_decimal_is_zero(&a))
    return (b == 0) ? 0 : (b > 0 ? -1 : 1);

  if (b == 0)
    return a.sign() ? -1 : 1;

  // Different signs. The negative number is the smallest one.
  if (a.sign() != (b < 0))
    return (b < 0) ? 1 : -1;

  // Couldn't tell the difference by looking at the signs. Compare as decimals.
  my_decimal b_dec;
  longlong2decimal(b, &b_dec);
  return my_decimal_cmp(&a, &b_dec);
}


/**
  Compare a decimal value to an unsigned integer by converting the
  integer to a decimal.
  @param a the decimal value
  @param b the unsigned integer value
  @return -1 if a is less than b,
          0 if a is equal to b,
          1 if a is greater than b
*/
static int compare_json_decimal_uint(const my_decimal &a, ulonglong b)
{
  if (my_decimal_is_zero(&a))
    return (b == 0) ? 0 : -1;

  // If a is negative, it must be smaller than the unsigned value b.
  if (a.sign())
    return -1;

  // When we get here, we know that a is greater than zero.
  if (b == 0)
    return 1;

  // Couldn't tell the difference by looking at the signs. Compare as decimals.
  my_decimal b_dec;
  ulonglong2decimal(b, &b_dec);
  return my_decimal_cmp(&a, &b_dec);
}


/**
  Compare a JSON double to a JSON signed integer.
  @param a the double value
  @param b the integer value
  @return -1 if a is less than b,
          0 if a is equal to b,
          1 if a is greater than b
*/
static int compare_json_double_int(double a, longlong b)
{
  double b_double= static_cast<double>(b);
  if (a < b_double)
    return -1;
  if (a > b_double)
    return 1;

  /*
    The two numbers were equal when compared as double. Since
    conversion from longlong to double isn't lossless, they could
    still be different. Convert to decimal to compare their exact
    values.
  */
  my_decimal b_dec;
  longlong2decimal(b, &b_dec);
  return -compare_json_decimal_double(b_dec, a);
}


/**
  Compare a JSON double to a JSON unsigned integer.
  @param a the double value
  @param b the unsigned integer value
  @return -1 if a is less than b,
          0 if a is equal to b,
          1 if a is greater than b
*/
static int compare_json_double_uint(double a, ulonglong b)
{
  double b_double= ulonglong2double(b);
  if (a < b_double)
    return -1;
  if (a > b_double)
    return 1;

  /*
    The two numbers were equal when compared as double. Since
    conversion from longlong to double isn't lossless, they could
    still be different. Convert to decimal to compare their exact
    values.
  */
  my_decimal b_dec;
  ulonglong2decimal(b, &b_dec);
  return -compare_json_decimal_double(b_dec, a);
}


/**
  Compare a JSON signed integer to a JSON unsigned integer.
  @param a the signed integer
  @param b the unsigned integer
  @return -1 if a is less than b,
          0 if a is equal to b,
          1 if a is greater than b
*/
static int compare_json_int_uint(longlong a, ulonglong b)
{
  // All negative values are less than the unsigned value b.
  if (a < 0)
    return -1;

  // If a is not negative, it is safe to cast it to ulonglong.
  return compare_numbers(static_cast<ulonglong>(a), b);
}


/**
  Compare the contents of two strings in a JSON value. The strings
  could be either JSON string scalars encoded in utf8mb4, or binary
  strings from JSON opaque scalars. In either case they are compared
  byte by byte.

  @param str1 the first string
  @param str1_len the length of str1
  @param str2 the second string
  @param str2_len the length of str2
  @retval -1 if str1 is less than str2,
  @retval 0 if str1 is equal to str2,
  @retval 1 if str1 is greater than str2
*/
static int compare_json_strings(const char *str1, size_t str1_len,
                                const char *str2, size_t str2_len)
{
  int cmp= memcmp(str1, str2, std::min(str1_len, str2_len));
  if (cmp != 0)
    return cmp;
  return compare_numbers(str1_len, str2_len);
}


/**
  The following matrix tells how two JSON values should be compared
  based on their types. If type_comparison[type_of_a][type_of_b] is
  -1, it means that a is smaller than b. If it is 1, it means that a
  is greater than b. If it is 0, it means it cannot be determined
  which value is the greater one just by looking at the types.
*/
static const int type_comparison[Json_dom::J_ERROR + 1][Json_dom::J_ERROR + 1]=
{
  /* NULL */      {0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  /* DECIMAL */   {1,  0,  0,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  /* INT */       {1,  0,  0,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  /* UINT */      {1,  0,  0,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  /* DOUBLE */    {1,  0,  0,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  /* STRING */    {1,  1,  1,  1,  1,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  /* OBJECT */    {1,  1,  1,  1,  1,  1,  0, -1, -1, -1, -1, -1, -1, -1, -1},
  /* ARRAY */     {1,  1,  1,  1,  1,  1,  1,  0, -1, -1, -1, -1, -1, -1, -1},
  /* BOOLEAN */   {1,  1,  1,  1,  1,  1,  1,  1,  0, -1, -1, -1, -1, -1, -1},
  /* DATE */      {1,  1,  1,  1,  1,  1,  1,  1,  1,  0, -1, -1, -1, -1, -1},
  /* TIME */      {1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  0, -1, -1, -1, -1},
  /* DATETIME */  {1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  0,  0, -1, -1},
  /* TIMESTAMP */ {1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  0,  0, -1, -1},
  /* OPAQUE */    {1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  0, -1},
  /* ERROR */     {1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1},
};


int Json_wrapper::compare(const Json_wrapper &other) const
{
  const Json_dom::enum_json_type this_type= type();
  const Json_dom::enum_json_type other_type= other.type();

  DBUG_ASSERT(this_type != Json_dom::J_ERROR);
  DBUG_ASSERT(other_type != Json_dom::J_ERROR);

  // Check if the type tells us which value is bigger.
  int cmp= type_comparison[this_type][other_type];
  if (cmp != 0)
    return cmp;

  // Same or similar type. Go on and inspect the values.

  switch (this_type)
  {
  case Json_dom::J_ARRAY:
    /*
      Two arrays are equal if they have the same length, and all
      elements in one array are equal to the corresponding elements in
      the other array.

      The array that has the smallest value on the first position that
      contains different values in the two arrays, is considered
      smaller than the other array. If the two arrays are of different
      size, and all values in the shorter array are equal to the
      corresponding values in the longer array, the shorter array is
      considered smaller.
    */
    {
      const size_t size_a= length();
      const size_t size_b= other.length();
      const size_t min_size= std::min(size_a, size_b);
      for (size_t i= 0; i < min_size; i++)
      {
        int cmp= (*this)[i].compare(other[i]);
        if (cmp != 0)
          return cmp;
      }
      return compare_numbers(size_a, size_b);
    }
  case Json_dom::J_OBJECT:
    /*
      An object is equal to another object if they have the same set
      of keys, and all values in one objects are equal to the values
      associated with the same key in the other object.
    */
    {
      /*
        If their sizes are different, the object with the smallest
        number of elements is smaller than the other object.
      */
      cmp= compare_numbers(length(), other.length());
      if (cmp != 0)
        return cmp;

      /*
        Otherwise, compare each key/value pair in the two objects.
        Return on the first difference that is found.
      */
      Json_wrapper_object_iterator it1= object_iterator();
      Json_wrapper_object_iterator it2= other.object_iterator();
      while (!it1.empty())
      {
        const std::pair<const std::string, Json_wrapper> elt1= it1.elt();
        const std::pair<const std::string, Json_wrapper> elt2= it2.elt();

        const std::string key1= elt1.first;
        const std::string key2= elt2.first;

        cmp= compare_json_strings(key1.data(), key1.size(),
                                  key2.data(), key2.size());
        if (cmp != 0)
          return cmp;

        cmp= elt1.second.compare(elt2.second);
        if (cmp != 0)
          return cmp;

        it1.next();
        it2.next();
      }

      DBUG_ASSERT(it1.empty());
      DBUG_ASSERT(it2.empty());

      // No differences found. The two objects must be equal.
      return 0;
    }
  case Json_dom::J_STRING:
    return compare_json_strings(get_data(), get_data_length(),
                                other.get_data(), other.get_data_length());
  case Json_dom::J_INT:
    // Signed integers can be compared to all other numbers.
    switch (other_type)
    {
    case Json_dom::J_INT:
      return compare_numbers(get_int(), other.get_int());
    case Json_dom::J_UINT:
      return compare_json_int_uint(get_int(), other.get_uint());
    case Json_dom::J_DOUBLE:
      return -compare_json_double_int(other.get_double(), get_int());
    case Json_dom::J_DECIMAL:
      {
        my_decimal b_dec;
        if (other.get_decimal_data(&b_dec))
          return 1;                           /* purecov: inspected */
        return -compare_json_decimal_int(b_dec, get_int());
      }
    default:;
    }
    break;
  case Json_dom::J_UINT:
    // Unsigned integers can be compared to all other numbers.
    switch (other_type)
    {
    case Json_dom::J_UINT:
      return compare_numbers(get_uint(), other.get_uint());
    case Json_dom::J_INT:
      return -compare_json_int_uint(other.get_int(), get_uint());
    case Json_dom::J_DOUBLE:
      return -compare_json_double_uint(other.get_double(), get_uint());
    case Json_dom::J_DECIMAL:
      {
        my_decimal b_dec;
        if (other.get_decimal_data(&b_dec))
          return 1;                           /* purecov: inspected */
        return -compare_json_decimal_uint(b_dec, get_uint());
      }
    default:;
    }
    break;
  case Json_dom::J_DOUBLE:
    // Doubles can be compared to all other numbers.
    {
      switch (other_type)
      {
      case Json_dom::J_DOUBLE:
        return compare_numbers(get_double(), other.get_double());
      case Json_dom::J_INT:
        return compare_json_double_int(get_double(), other.get_int());
      case Json_dom::J_UINT:
        return compare_json_double_uint(get_double(), other.get_uint());
      case Json_dom::J_DECIMAL:
        {
          my_decimal other_dec;
          if (other.get_decimal_data(&other_dec))
            return 1;                         /* purecov: inspected */
          return -compare_json_decimal_double(other_dec, get_double());
        }
      default:;
      }
      break;
    }
  case Json_dom::J_DECIMAL:
    // Decimals can be compared to all other numbers.
    {
      my_decimal a_dec;
      my_decimal b_dec;
      if (get_decimal_data(&a_dec))
        return 1;                             /* purecov: inspected */
      switch (other_type)
      {
      case Json_dom::J_DECIMAL:
        if (other.get_decimal_data(&b_dec))
          return 1;                           /* purecov: inspected */
        /*
          my_decimal_cmp() treats -0 and 0 as not equal, so check for
          zero first.
        */
        if (my_decimal_is_zero(&a_dec) && my_decimal_is_zero(&b_dec))
          return 0;
        return my_decimal_cmp(&a_dec, &b_dec);
      case Json_dom::J_INT:
        return compare_json_decimal_int(a_dec, other.get_int());
      case Json_dom::J_UINT:
        return compare_json_decimal_uint(a_dec, other.get_uint());
      case Json_dom::J_DOUBLE:
        return compare_json_decimal_double(a_dec, other.get_double());
      default:;
      }
      break;
    }
  case Json_dom::J_BOOLEAN:
    // Booleans are only equal to other booleans. false is less than true.
    return compare_numbers(get_boolean(), other.get_boolean());
  case Json_dom::J_DATETIME:
  case Json_dom::J_TIMESTAMP:
    // Timestamps and datetimes can be equal to each other.
    {
      MYSQL_TIME val_a;
      get_datetime(&val_a);
      MYSQL_TIME val_b;
      other.get_datetime(&val_b);
      return compare_numbers(TIME_to_longlong_packed(&val_a),
                             TIME_to_longlong_packed(&val_b));
    }
  case Json_dom::J_TIME:
  case Json_dom::J_DATE:
    // Dates and times can only be equal to values of the same type.
    {
      DBUG_ASSERT(this_type == other_type);
      MYSQL_TIME val_a;
      get_datetime(&val_a);
      MYSQL_TIME val_b;
      other.get_datetime(&val_b);
      return compare_numbers(TIME_to_longlong_packed(&val_a),
                             TIME_to_longlong_packed(&val_b));
    }
  case Json_dom::J_OPAQUE:
    /*
      Opaque values are equal to other opaque values with the same
      field type and the same binary representation.
    */
    cmp= compare_numbers(field_type(), other.field_type());
    if (cmp == 0)
      cmp= compare_json_strings(get_data(), get_data_length(),
                                other.get_data(), other.get_data_length());
    return cmp;
  case Json_dom::J_NULL:
    // Null is always equal to other nulls.
    DBUG_ASSERT(this_type == other_type);
    return 0;
  case Json_dom::J_ERROR:
    break;
  }

  DBUG_ABORT();                               /* purecov: inspected */
  return 1;                                   /* purecov: inspected */
}


/**
  Push a warning about a problem encountered when coercing a JSON
  value to some other data type.

  @param[in] target_type  the name of the target type of the coercion
  @param[in] error_code   the error code to use for the warning
  @param[in] msgnam       the name of the field/expression being coerced
*/
static void push_json_coercion_warning(const char *target_type,
                                       int error_code,
                                       const char *msgnam)
{
  /*
    One argument is no longer used (the empty string), but kept to avoid
    changing error message format.
  */
  push_warning_printf(current_thd,
                      Sql_condition::SL_WARNING,
                      error_code,
                      ER_THD(current_thd, error_code),
                      target_type,
                      "",
                      msgnam,
                      current_thd->get_stmt_da()->current_row_for_condition());
}


longlong Json_wrapper::coerce_int(const char *msgnam) const
{
  switch (type())
  {
  case Json_dom::J_UINT:
    return static_cast<longlong>(get_uint());
  case Json_dom::J_INT:
    return get_int();
  case Json_dom::J_STRING:
    {
      /*
        For a string result, we must first get the string and then convert it
        to a longlong.
      */
      const char *start= get_data();
      size_t length= get_data_length();
      char *end= const_cast<char *>(start + length);
      const CHARSET_INFO *cs= &my_charset_utf8mb4_bin;

      int error;
      longlong value= cs->cset->strtoll10(cs, start, &end, &error);

      if (error > 0 || end != start + length)
      {
        int code= (error == MY_ERRNO_ERANGE ?
                   ER_NUMERIC_JSON_VALUE_OUT_OF_RANGE :
                   ER_INVALID_JSON_VALUE_FOR_CAST);
        push_json_coercion_warning("INTEGER", code, msgnam);
      }

      return value;
    }
  case Json_dom::J_BOOLEAN:
    return get_boolean() ? 1 : 0;
  case Json_dom::J_DECIMAL:
    {
      longlong i;
      my_decimal decimal_value;
      get_decimal_data(&decimal_value);
      /*
        We do not know if this int is destined for signed or unsigned usage, so
        just get longlong from the value using the sign in the decimal.
      */
      my_decimal2int(E_DEC_FATAL_ERROR, &decimal_value, !decimal_value.sign(),
                     &i);
      return i;
    }
  case Json_dom::J_DOUBLE:
    {
      // logic here is borrowed from Field_double::val_int
      double j= get_double();
      longlong res;

      if (j <= (double) LLONG_MIN)
      {
        res= LLONG_MIN;
      }
      else if (j >= (double) (ulonglong) LLONG_MAX)
      {
        res= LLONG_MAX;
      }
      else
      {
        return (longlong) rint(j);
      }

      push_json_coercion_warning("INTEGER",
                                 ER_NUMERIC_JSON_VALUE_OUT_OF_RANGE, msgnam);
      return res;
    }
  default:;
  }

  push_json_coercion_warning("INTEGER", ER_INVALID_JSON_VALUE_FOR_CAST,
                             msgnam);
  return 0;
}


double Json_wrapper::coerce_real(const char *msgnam) const
{
  switch (type())
  {
  case Json_dom::J_DECIMAL:
    {
      double dbl;
      my_decimal decimal_value;
      get_decimal_data(&decimal_value);
      my_decimal2double(E_DEC_FATAL_ERROR, &decimal_value, &dbl);
      return dbl;
    }
  case Json_dom::J_STRING:
    {
      /*
        For a string result, we must first get the string and then convert it
        to a double.
      */
      const char *start= get_data();
      size_t length= get_data_length();
      char *end= const_cast<char*>(start) + length;
      const CHARSET_INFO *cs= &my_charset_utf8mb4_bin;

      int error;
      double value= my_strntod(cs, const_cast<char*>(start), length,
                               &end, &error);

      if (error || end != start + length)
      {
        int code= (error == EOVERFLOW ?
                   ER_NUMERIC_JSON_VALUE_OUT_OF_RANGE :
                   ER_INVALID_JSON_VALUE_FOR_CAST);
        push_json_coercion_warning("DOUBLE", code, msgnam);
      }
      return value;
    }
  case Json_dom::J_DOUBLE:
    return get_double();
  case Json_dom::J_INT:
    return static_cast<double>(get_int());
  case Json_dom::J_UINT:
    return static_cast<double>(get_uint());
  case Json_dom::J_BOOLEAN:
    return static_cast<double>(get_boolean());
  default:;
  }

  push_json_coercion_warning("DOUBLE", ER_INVALID_JSON_VALUE_FOR_CAST,
                             msgnam);
  return 0.0;
}

my_decimal
*Json_wrapper::coerce_decimal(my_decimal *decimal_value,
                              const char *msgnam) const
{
  switch (type())
  {
  case Json_dom::J_DECIMAL:
    get_decimal_data(decimal_value);
    return decimal_value;
  case Json_dom::J_STRING:
    {
      /*
        For a string result, we must first get the string and then convert it
        to a decimal.
      */
      // has own error handling, but not very informative
      int err= str2my_decimal(E_DEC_FATAL_ERROR, get_data(), get_data_length(),
                              &my_charset_utf8mb4_bin, decimal_value);
      if (err)
      {
        int code= (err == E_DEC_OVERFLOW ?
                   ER_NUMERIC_JSON_VALUE_OUT_OF_RANGE :
                   ER_INVALID_JSON_VALUE_FOR_CAST);
        push_json_coercion_warning("DECIMAL", code, msgnam);
      }
      return decimal_value;
    }
  case Json_dom::J_DOUBLE:
    if (double2my_decimal(E_DEC_FATAL_ERROR, get_double(), decimal_value))
    {
      push_json_coercion_warning("DECIMAL",
                                 ER_NUMERIC_JSON_VALUE_OUT_OF_RANGE, msgnam);
    }
    return decimal_value;
  case Json_dom::J_INT:
    if (longlong2decimal(get_int(), decimal_value))
    {
      push_json_coercion_warning("DECIMAL",
                                 ER_NUMERIC_JSON_VALUE_OUT_OF_RANGE, msgnam);
    }
    return decimal_value;
  case Json_dom::J_UINT:
    if (longlong2decimal(get_uint(), decimal_value))
    {
      push_json_coercion_warning("DECIMAL",
                                 ER_NUMERIC_JSON_VALUE_OUT_OF_RANGE, msgnam);
    }
    return decimal_value;
  case Json_dom::J_BOOLEAN:
    // no danger of overflow, so void result
    (void)int2my_decimal(E_DEC_FATAL_ERROR, get_boolean(),
                         true /* unsigned */, decimal_value);
    return decimal_value;
  default:;
  }

  push_json_coercion_warning("DECIMAL", ER_INVALID_JSON_VALUE_FOR_CAST,
                             msgnam);

  my_decimal_set_zero(decimal_value);
  return decimal_value;
}


bool Json_wrapper::coerce_date(MYSQL_TIME *ltime,
                               my_time_flags_t fuzzydate,
                               const char *msgnam) const
{
  bool result= coerce_time(ltime, msgnam);

  if (!result && ltime->time_type == MYSQL_TIMESTAMP_TIME)
  {
    MYSQL_TIME tmp= *ltime;
    time_to_datetime(current_thd, &tmp, ltime);
  }

  return result;
}


bool Json_wrapper::coerce_time(MYSQL_TIME *ltime,
                               const char *msgnam) const
{
  switch (type())
  {
  case Json_dom::J_DATETIME:
  case Json_dom::J_DATE:
  case Json_dom::J_TIME:
  case Json_dom::J_TIMESTAMP:
    set_zero_time(ltime, MYSQL_TIMESTAMP_DATETIME);
    get_datetime(ltime);
    return false;
  default:
    push_json_coercion_warning("DATE/TIME/DATETIME/TIMESTAMP",
                               ER_INVALID_JSON_VALUE_FOR_CAST, msgnam);
    return true;
  }
}


/// Wrapper around a sort key buffer.
class Wrapper_sort_key
{
private:
  uchar *m_buffer;  ///< the buffer into which to write
  size_t m_length;  ///< the length of the buffer
  size_t m_pos;     ///< the current position in the buffer

public:
  Wrapper_sort_key(uchar *buf, size_t len)
    : m_buffer(buf), m_length(len), m_pos(0)
  {}

  /// Get the remaining space in the buffer.
  size_t remaining() const
  {
    return m_length - m_pos;
  }

  /// Append a character to the buffer.
  void append(uchar ch)
  {
    if (m_pos < m_length)
      m_buffer[m_pos++]= ch;
  }

  /// Pad the buffer with the specified character.
  void pad_fill(uchar pad_character, size_t length)
  {
    size_t num_chars= std::min(remaining(), length);
    memset(m_buffer + m_pos, pad_character, num_chars);
    m_pos += num_chars;
  }

  /**
    Copy an integer to the buffer and format it in a way that makes it
    possible to sort the integers with memcpy().

    @param target_length  the number of bytes to write to the buffer
    @param from           the buffer to copy the integer from (in little-endian
                          format)
    @param from_length    the size of the from buffer
    @param is_unsigned    true if the from buffer contains an unsigned integer,
                          false otherwise
  */
  void copy_int(size_t target_length,
                const uchar* from, size_t from_length,
                bool is_unsigned)
  {
    size_t to_length= std::min(remaining(), target_length);
    copy_integer<false>(m_buffer + m_pos, to_length,
                        from, from_length, is_unsigned);
    m_pos+= to_length;
  }

  /**
    Append a string to the buffer, and add the length of the string to
    the end of the buffer. The space between the end of the string and
    the beginning of the length field is padded with zeros.
  */
  void append_str_and_len(const char *str, size_t len)
  {
    /*
      The length is written as a four byte value at the end of the
      buffer, provided that there is enough room.
    */
    size_t space_for_len= std::min(static_cast<size_t>(4), remaining());

    /*
      The string contents are written up to where the length is
      stored, and get truncated if the string is longer than that.
    */
    size_t space_for_str= remaining() - space_for_len;
    size_t copy_len= std::min(len, space_for_str);
    memcpy(m_buffer + m_pos, str, copy_len);
    m_pos+= copy_len;

    /*
      Fill the space between the end of the string and the beginning
      of the length with zeros.
    */
    pad_fill(0, space_for_str - copy_len);

    /*
      Write the length in a format that memcmp() knows how to sort.
      First we store it in little-endian format in a four-byte buffer,
      and then we use copy_integer to transform it into a format that
      works with memcmp().
    */
    uchar length_buffer[4];
    int4store(length_buffer, static_cast<uint32>(len));
    copy_int(space_for_len,
             length_buffer, sizeof(length_buffer), true);

    // The entire buffer has been filled when we are done here.
    m_pos= m_length;
  }
};


/// Helper class for building a hash key.
class Wrapper_hash_key
{
private:

  ulonglong m_crc;

public:

  Wrapper_hash_key(ulonglong *hash_val) : m_crc(*hash_val)
  {}

  /**
    Return the computed hash value.
  */
  ulonglong get_crc()
  {
    return m_crc;
  }

  void add_character(uchar ch)
  {
    add_to_crc(ch);
  }

  void add_integer(longlong ll)
  {
    char tmp[8];
    int8store(tmp, ll);
    add_string(tmp, sizeof(tmp));
  }

  void add_double(double d)
  {
    // Make -0.0 and +0.0 have the same key.
    if (d == 0)
    {
      add_character(0);
      return;
    }

    char tmp[8];
    float8store(tmp, d);
    add_string(tmp, sizeof(tmp));
  }

  void add_string(const char *str, size_t len)
  {
    for (size_t idx= 0; idx < len; idx++)
    {
      add_to_crc(*str++);
    }
  }

private:

  /**
    Add another character to the evolving crc.

    @param[in] ch The character to add
  */
  void add_to_crc(uchar ch)
  {
    // This logic was cribbed from sql_executor.cc/unique_hash
    m_crc=((m_crc << 8) +
         (((uchar) ch))) +
      (m_crc >> (8*sizeof(ha_checksum)-8));
  }
};


/// Check if a character represents a non-zero digit.
static inline bool is_non_zero_digit(char ch)
{
  return ch >= '1' && ch <= '9';
}


/*
  Type identifiers used in the sort key generated by
  Json_wrapper::make_sort_key(). Types with lower identifiers sort
  before types with higher identifiers.
*/
#define JSON_KEY_NULL        '\x00'
#define JSON_KEY_NUMBER_NEG  '\x01'
#define JSON_KEY_NUMBER_ZERO '\x02'
#define JSON_KEY_NUMBER_POS  '\x03'
#define JSON_KEY_STRING      '\x04'
#define JSON_KEY_OBJECT      '\x05'
#define JSON_KEY_ARRAY       '\x06'
#define JSON_KEY_FALSE       '\x07'
#define JSON_KEY_TRUE        '\x08'
#define JSON_KEY_DATE        '\x09'
#define JSON_KEY_TIME        '\x0A'
#define JSON_KEY_DATETIME    '\x0B'
#define JSON_KEY_OPAQUE      '\x0C'


/**
  Make a sort key for a JSON numeric value from its string representation. The
  input string could be either on scientific format (such as 1.234e2) or on
  plain format (such as 12.34).

  The sort key will have the following parts:

  1) One byte that is JSON_KEY_NUMBER_NEG, JSON_KEY_NUMBER_ZERO or
  JSON_KEY_NUMBER_POS if the number is positive, zero or negative,
  respectively.

  2) Two bytes that represent the decimal exponent of the number (log10 of the
  number, truncated to an integer).

  3) All the digits of the number, without leading zeros.

  4) Padding to ensure that equal numbers sort equal even if they have a
  different number of trailing zeros.

  If the number is zero, parts 2, 3 and 4 are skipped.

  For negative numbers, the values in parts 2, 3 and 4 need to be inverted so
  that bigger negative numbers sort before smaller negative numbers.

  @param[in]     from     the string representation of the number
  @param[in]     len      the length of the input string
  @param[in]     negative true if the number is negative, false otherwise
  @param[in,out] to       the target sort key
*/
static void make_json_numeric_sort_key(const char *from, size_t len,
                                       bool negative, Wrapper_sort_key *to)
{
  const char *end= from + len;

  // Find the start of the exponent part, if there is one.
  const char *end_of_digits= std::find(from, end, 'e');

  /*
    Find the first significant digit. Skip past sign, leading zeros
    and the decimal point, until the first non-zero digit is found.
  */
  const char *first_significant_digit=
    std::find_if(from, end_of_digits, is_non_zero_digit);

  if (first_significant_digit == end_of_digits)
  {
    // We didn't find any significant digits, so the number is zero.
    to->append(JSON_KEY_NUMBER_ZERO);
    return;
  }

  longlong exp;
  if (end_of_digits != end)
  {
    // Scientific format. Fetch the exponent part after the 'e'.
    char *endp= const_cast<char *>(end);
    exp= my_strtoll(end_of_digits + 1, &endp, 10);
  }
  else
  {
    /*
      Otherwise, find the exponent by calculating the distance between the
      first significant digit and the decimal point.
    */
    const char *dec_point= std::find(from, end_of_digits, '.');
    if (!dec_point)
    {
      // There is no decimal point. Just count the digits.
      exp= end_of_digits - first_significant_digit - 1;
    }
    else if (first_significant_digit < dec_point)
    {
      // Non-negative exponent.
      exp= dec_point - first_significant_digit - 1;
    }
    else
    {
      // Negative exponent.
      exp= dec_point - first_significant_digit;
    }
  }

  if (negative)
  {
    to->append(JSON_KEY_NUMBER_NEG);
    /*
      For negative numbers, we have to invert the exponents so that numbers
      with high exponents sort before numbers with low exponents.
    */
    exp= -exp;
  }
  else
  {
    to->append(JSON_KEY_NUMBER_POS);
  }

  /*
    Store the exponent part before the digits. Since the decimal exponent of a
    double can be in the range [-323, +308], we use two bytes for the
    exponent. (Decimals and bigints also fit in that range.)
  */
  uchar exp_buff[2];
  int2store(exp_buff, static_cast<int16>(exp));
  to->copy_int(sizeof(exp_buff), exp_buff, sizeof(exp_buff), false);

  /*
    Append all the significant digits of the number. Stop before the exponent
    part if there is one, otherwise go to the end of the string.
  */
  for (const char *ch= first_significant_digit; ch < end_of_digits; ++ch)
  {
    if (my_isdigit(&my_charset_numeric, *ch))
    {
      /*
        If the number is negative, the digits must be inverted so that big
        negative numbers sort before small negative numbers.
      */
      if (negative)
        to->append('9' - *ch + '0');
      else
        to->append(*ch);
    }
  }

  /*
    Pad the rest of the buffer with zeros, so that the number of trailing
    zeros doesn't affect how the number is sorted. As above, we need to invert
    the digits for negative numbers.
  */
  to->pad_fill(negative ? '9' : '0', to->remaining());
}


void Json_wrapper::make_sort_key(uchar *to, size_t to_length) const
{
  Wrapper_sort_key key(to, to_length);
  const Json_dom::enum_json_type jtype= type();
  switch (jtype)
  {
  case Json_dom::J_NULL:
    key.append(JSON_KEY_NULL);
    break;
  case Json_dom::J_DECIMAL:
    {
      my_decimal dec;
      if (get_decimal_data(&dec))
        break;                                  /* purecov: inspected */
      char buff[DECIMAL_MAX_STR_LENGTH + 1];
      String str(buff, sizeof(buff), &my_charset_numeric);
      if (my_decimal2string(E_DEC_FATAL_ERROR, &dec, 0, 0, 0, &str))
        break;                                  /* purecov: inspected */
      make_json_numeric_sort_key(str.ptr(), str.length(), dec.sign(), &key);
      break;
    }
  case Json_dom::J_INT:
    {
      longlong i= get_int();
      char buff[MAX_BIGINT_WIDTH + 1];
      size_t len= longlong10_to_str(i, buff, -10) - buff;
      make_json_numeric_sort_key(buff, len, i < 0, &key);
      break;
    }
  case Json_dom::J_UINT:
    {
      ulonglong ui= get_uint();
      char buff[MAX_BIGINT_WIDTH + 1];
      size_t len= longlong10_to_str(ui, buff, 10) - buff;
      make_json_numeric_sort_key(buff, len, false, &key);
      break;
    }
  case Json_dom::J_DOUBLE:
    {
      double dbl= get_double();
      char buff[MY_GCVT_MAX_FIELD_WIDTH + 1];
      size_t len= my_gcvt(dbl, MY_GCVT_ARG_DOUBLE,
                          sizeof(buff) - 1, buff, NULL);
      make_json_numeric_sort_key(buff, len, (dbl < 0), &key);
      break;
    }
  case Json_dom::J_STRING:
    key.append(JSON_KEY_STRING);
    key.append_str_and_len(get_data(), get_data_length());
    break;
  case Json_dom::J_OBJECT:
  case Json_dom::J_ARRAY:
    /*
      Internal ordering of objects and arrays only considers length
      for now.
    */
    {
      key.append(jtype == Json_dom::J_OBJECT ?
                 JSON_KEY_OBJECT : JSON_KEY_ARRAY);
      uchar len[4];
      int4store(len, static_cast<uint32>(length()));
      key.copy_int(sizeof(len), len, sizeof(len), true);
      /*
        Raise a warning to give an indication that sorting of objects
        and arrays is not properly supported yet. The warning is
        raised for each object/array that is found during the sort,
        but Filesort_error_handler will make sure that only one
        warning is seen on the top level for every sort.
      */
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          ER_NOT_SUPPORTED_YET,
                          ER_THD(current_thd, ER_NOT_SUPPORTED_YET),
                          "sorting of non-scalar JSON values");
      break;
    }
  case Json_dom::J_BOOLEAN:
    key.append(get_boolean() ? JSON_KEY_TRUE : JSON_KEY_FALSE);
    break;
  case Json_dom::J_DATE:
  case Json_dom::J_TIME:
  case Json_dom::J_DATETIME:
  case Json_dom::J_TIMESTAMP:
    {
      if (jtype == Json_dom::J_DATE)
        key.append(JSON_KEY_DATE);
      else if (jtype == Json_dom::J_TIME)
        key.append(JSON_KEY_TIME);
      else
        key.append(JSON_KEY_DATETIME);

      /*
        Temporal values are stored in the packed format in the binary
        JSON format. The packed values are 64-bit signed little-endian
        integers.
      */
      const size_t packed_length= Json_datetime::PACKED_SIZE;
      char tmp[packed_length];
      const char *packed= get_datetime_packed(tmp);
      key.copy_int(packed_length,
                   pointer_cast<const uchar *>(packed),
                   packed_length, false);
      break;
    }
  case Json_dom::J_OPAQUE:
    key.append(JSON_KEY_OPAQUE);
    key.append(field_type());
    key.append_str_and_len(get_data(), get_data_length());
    break;
  case Json_dom::J_ERROR:
    break;
  }

  key.pad_fill(0, key.remaining());
}


ulonglong Json_wrapper::make_hash_key(ulonglong *hash_val)
{
  Wrapper_hash_key hash_key(hash_val);

  const Json_dom::enum_json_type jtype= type();
  switch (jtype)
  {
  case Json_dom::J_NULL:
    hash_key.add_character(JSON_KEY_NULL);
    break;
  case Json_dom::J_DECIMAL:
    {
      my_decimal dec;
      if (get_decimal_data(&dec))
        break;                                  /* purecov: inspected */
      double dbl;
      decimal2double(&dec, &dbl);
      hash_key.add_double(dbl);
      break;
    }
  case Json_dom::J_INT:
    hash_key.add_double(static_cast<double>(get_int()));
    break;
  case Json_dom::J_UINT:
    hash_key.add_double(ulonglong2double(get_uint()));
    break;
  case Json_dom::J_DOUBLE:
    hash_key.add_double(get_double());
    break;
  case Json_dom::J_STRING:
  case Json_dom::J_OPAQUE:
    hash_key.add_string(get_data(), get_data_length());
    break;
  case Json_dom::J_OBJECT:
    {
      hash_key.add_character(JSON_KEY_OBJECT);
      for (Json_wrapper_object_iterator it(object_iterator());
           !it.empty(); it.next())
      {
        std::pair<const std::string, Json_wrapper> pair= it.elt();
        hash_key.add_string(pair.first.c_str(), pair.first.length());
        ulonglong t= hash_key.get_crc();
        hash_key.add_integer(pair.second.make_hash_key(&t));
      }
      break;
    }
  case Json_dom::J_ARRAY:
    {
      hash_key.add_character(JSON_KEY_ARRAY);
      size_t elts= length();
      for (uint i= 0; i < elts; i++)
      {
        ulonglong t= hash_key.get_crc();
        hash_key.add_integer((*this)[i].make_hash_key(&t));
      }
    break;
    }
  case Json_dom::J_BOOLEAN:
    hash_key.add_character(get_boolean() ? JSON_KEY_TRUE : JSON_KEY_FALSE);
    break;
  case Json_dom::J_DATE:
  case Json_dom::J_TIME:
  case Json_dom::J_DATETIME:
  case Json_dom::J_TIMESTAMP:
    {
      const size_t packed_length= Json_datetime::PACKED_SIZE;
      char tmp[packed_length];
      const char *packed= get_datetime_packed(tmp);
      hash_key.add_string(packed, packed_length);
      break;
    }
  case Json_dom::J_ERROR:
    DBUG_ABORT();                               /* purecov: inspected */
    break;                                      /* purecov: inspected */
  }

  ulonglong result= hash_key.get_crc();
  return result;
}
