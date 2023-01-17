/* Copyright (c) 2014, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef GCS_VIEW_IDENTIFIER_INCLUDED
#define GCS_VIEW_IDENTIFIER_INCLUDED

#include <string>
#include <typeinfo>

/**
  @class Gcs_view_identifier

  This represents the identification of a certain view of a certain group in a
  moment in time.

  This identifier must increase monotonically and be unique,
  in order to provide the notion of "happened before".

  The get_representation() method is virtual, since each binding
  implementation might provide its notion of view identifier. The only
  mandatory property is that they should be comparable.
*/

class Gcs_view_identifier {
 public:
  Gcs_view_identifier() = default;
  Gcs_view_identifier(const Gcs_view_identifier &) = default;
  Gcs_view_identifier(Gcs_view_identifier &&) = default;
  Gcs_view_identifier &operator=(const Gcs_view_identifier &) = default;
  Gcs_view_identifier &operator=(Gcs_view_identifier &&) = default;

  /**
     Returns a normalized representation of the DTO.

     @return a const char * containing the normalized representation of the DTO
   */

  virtual const std::string &get_representation() const = 0;

  /**
    Returns a copy of this object. Since this object is an interface, this is
    necessary to generate copies of it.

    @return a copy of the current object
  */

  virtual Gcs_view_identifier *clone() const = 0;

  /**
    Redefinition of the operator less, to allow usage as key in maps.

    @param[in] other the Gcs_view_identifier to compare to

    @return true if the objects is less than @c other, according to the
                 internal representation of the object.
            false otherwise.
  */

  bool operator<(const Gcs_view_identifier &other) const {
    return typeid(*this) == typeid(other) && this->lessThan(other);
  }

  /**
    Redefinition of the operator equals, to allow usage in sets.

    @param[in] other the Gcs_view_identifier to compare to

    @return true if the objects have the same internal representation.
            false otherwise.
  */

  bool operator==(const Gcs_view_identifier &other) const {
    return typeid(*this) == typeid(other) && this->equals(other);
  }

  bool operator!=(const Gcs_view_identifier &other) const {
    return !(*this == other);
  }

  virtual ~Gcs_view_identifier() = default;

 private:
  virtual bool equals(const Gcs_view_identifier &other) const = 0;
  virtual bool lessThan(const Gcs_view_identifier &other) const = 0;
};

#endif  // GCS_VIEW_IDENTIFIER_INCLUDED
