/*
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PROPERTIES_HPP
#define PROPERTIES_HPP

#include <ndb_global.h>
#include <BaseString.hpp>
#include <UtilBuffer.hpp>
#include <unordered_map>

enum PropertiesType {
  PropertiesType_Undefined = -1,
  PropertiesType_Uint32 = 0,
  PropertiesType_char = 1,
  PropertiesType_Properties = 2,
  PropertiesType_Uint64 = 3
};

/**
 * @struct  Property
 * @brief   Stores one (name, value)-pair
 * 
 * Value can be of type Properties, i.e. a Property may contain 
 * a Properties object.
 */
struct Property {
  Property(const char* name, Uint32 val);
  Property(const char* name, Uint64 val);
  Property(const char* name, const char * value);
  Property(const char* name, const class Properties * value);
  ~Property();
private:
  friend class Properties;
  struct PropertyImpl * impl;
};

/**
 * @class  Properties
 * @brief  Stores information in (name, value)-pairs
 */
class Properties {
public:
  static const char delimiter;
  static const char version[];

  Properties(bool case_insensitive= false);
  Properties(const Properties &);
  Properties(const Property *, int len);
  virtual ~Properties();

  /**
   * Set/Get wheather names in the Properties should be compared 
   * w/o case.
   * NOTE: The property is automatically applied to all propoerties put
   *       into this after a called to setCaseInsensitiveNames has been made
   *       But properties already in when calling setCaseInsensitiveNames will
   *       not be affected
   */
  void setCaseInsensitiveNames(bool value);
  bool getCaseInsensitiveNames() const;

  /**
   * Insert an array of value(s)
   */
  void put(const Property *, int len);

  bool put(const char * name, Uint32 value, bool replace = false);
  bool put64(const char * name, Uint64 value, bool replace = false);
  bool put(const char * name, const char * value, bool replace = false);
  bool put(const char * name, const Properties * value, bool replace = false);
  bool append(const char * name, const char * value);

  /**
   * Same as put above,
   *   except that _%d (where %d is a number) is added to the name
   * Compare get(name, no)
   */
  bool put(const char *, Uint32 no, Uint32, bool replace = false);
  bool put64(const char *, Uint32 no, Uint64, bool replace = false);
  bool put(const char *, Uint32 no, const char *, bool replace = false);
  bool put(const char *, Uint32 no, const Properties *, bool replace = false);


  bool getTypeOf(const char * name, PropertiesType * type) const;

  /** @return true if Properties object contains name */
  bool contains(const char * name) const;

  bool get(const char * name, Uint32 * value) const;
  bool get(const char * name, Uint64 * value) const;
  bool get(const char * name, const char ** value) const;
  bool get(const char * name, BaseString & value) const;
  bool get(const char * name, const Properties ** value) const;
  
  bool getCopy(const char * name, char ** value) const;
  bool getCopy(const char * name, Properties ** value) const;

  /**
   * Same as get above
   *   except that _%d (where %d = no) is added to the name
   */
  bool getTypeOf(const char * name, Uint32 no, PropertiesType * type) const;
  bool contains(const char * name, Uint32 no) const;

  bool get(const char * name, Uint32 no, Uint32 * value) const;
  bool get(const char * name, Uint32 no, Uint64 * value) const;
  bool get(const char * name, Uint32 no, const char ** value) const;
  bool get(const char * name, Uint32 no, const Properties ** value) const;
  
  bool getCopy(const char * name, Uint32 no, char ** value) const;
  bool getCopy(const char * name, Uint32 no, Properties ** value) const;

  void clear();

  void remove(const char * name);
  
  void print(FILE * file = stdout, const char * prefix = 0) const;
  /**
   *  Iterator over names 
   */
  class Iterator
  {
  public:
    Iterator(const Properties* prop);
    ~Iterator();

    const char* first();
    const char* next();
  private:
    const Properties*  m_prop;
    class IteratorImpl *m_iterImpl;
  };
  friend class Properties::Iterator;

  Uint32 getPackedSize() const;
  bool unpack(const Uint32 * buf, Uint32 bufLen);
  bool unpack(UtilBuffer &buf);
  
  Uint32 getPropertiesErrno() const { return propErrno; }
  Uint32 getOSErrno() const { return osErrno; }

private:
  Uint32 propErrno;
  Uint32 osErrno;

  friend class PropertiesImpl;
  class PropertiesImpl * impl;
  class Properties * parent;

  void setErrno(Uint32 pErr, Uint32 osErr = 0) const ;
};

/**
 * Error code for properties
 */

/**
 * No error
 */
extern const Uint32 E_PROPERTIES_OK;

/**
 * Invalid name in put, names can not contain Properties::delimiter
 */
extern const Uint32 E_PROPERTIES_INVALID_NAME;

/**
 * Element did not exist when using get
 */
extern const Uint32 E_PROPERTIES_NO_SUCH_ELEMENT;

/**
 * Element had wrong type when using get
 */
extern const Uint32 E_PROPERTIES_INVALID_TYPE;

/**
 * Element already existed when using put, and replace was not specified
 */
extern const Uint32 E_PROPERTIES_ELEMENT_ALREADY_EXISTS;

/**
 * Invalid version on properties file you are trying to read
 */
extern const Uint32 E_PROPERTIES_INVALID_VERSION_WHILE_UNPACKING;

/**
 * When unpacking an buffer
 *  found that buffer is to short
 *
 * Probably an invlaid buffer
 */
extern const Uint32 E_PROPERTIES_INVALID_BUFFER_TO_SHORT;

/**
 * Error when packing, can not allocate working buffer
 *   
 * Note: OS error is set
 */
extern const Uint32 E_PROPERTIES_ERROR_MALLOC_WHILE_PACKING;

/**
 * Error when unpacking, can not allocate working buffer
 *   
 * Note: OS error is set
 */
extern const Uint32 E_PROPERTIES_ERROR_MALLOC_WHILE_UNPACKING;

/**
 * Error when unpacking, invalid checksum
 *   
 */
extern const Uint32 E_PROPERTIES_INVALID_CHECKSUM;

/**
 * Error when unpacking
 *   No of items > 0 while size of buffer (left) <= 0
 */
extern const Uint32 E_PROPERTIES_BUFFER_TO_SMALL_WHILE_UNPACKING;

inline bool
Properties::unpack(UtilBuffer &buf) {
  return unpack((const Uint32 *)buf.get_data(), buf.length());
}

#endif
