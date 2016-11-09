/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_VIEW_IDENTIFIER_INCLUDED
#define GCS_VIEW_IDENTIFIER_INCLUDED

#include <string>

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

class Gcs_view_identifier
{
public:
  /**
    Returns a normalized representation of the DTO.

    @return a const char * containing the normalized representation of the DTO
  */

  virtual const std::string &get_representation() const= 0;


  /**
    Returns a copy of this object. Since this object is an interface, this is
    necessary to generate copies of it.

    @return a copy of the current object
  */

  virtual Gcs_view_identifier *clone() const= 0;


  /**
    Redefinition of the operator less, to allow usage as key in maps.

    @param[in] other the Gcs_view_identifier to compare to

    @return the result of comparing internal representation with the
            compare operation from the string object
  */

  bool operator<(const Gcs_view_identifier &other) const;


  /**
    Redefinition of the operator equals, to allow usage in sets.

    @param[in] other the Gcs_view_identifier to compare to

    @return the result of comparing internal representation with the
            compare operation from the string object
  */

  bool operator==(const Gcs_view_identifier &other) const;


  virtual ~Gcs_view_identifier() {}
};

#endif // GCS_VIEW_IDENTIFIER_INCLUDED
