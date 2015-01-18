/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_MEMBER_IDENTIFIER_INCLUDED
#define GCS_MEMBER_IDENTIFIER_INCLUDED

#include <string>

using std::string;

/**
  @class Gcs_member_identifier
  It represents the identity of a group member within a certain group.
  It can be the same or different within groups.
 */
class Gcs_member_identifier
{
public:
  /**
    Gcs_member_identifier constructor

    @param[in] member_id the member identifier
   */
  Gcs_member_identifier(string member_id);
  virtual ~Gcs_member_identifier();

  /**
    @return the member identifier
   */
  string* get_member_id();

  /**
    Redefinition of the operator less, to allow usage as key in maps

    @param[in] other the Gcs_member_identifier to compare to
    @return the result of comparing internal representation with the
            compare operation from the string object
   */
  bool operator<(const Gcs_member_identifier& other) const;

    /**
    Redefinition of the operator equals, to allow usage in sets

    @param[in] other the Gcs_member_identifier to compare to
    @return the result of comparing internal representation with the
            compare operation from the string object
   */

  bool operator==(const Gcs_member_identifier& other) const;

private:
  string member_id;
};

#endif // GCS_MEMBER_IDENTIFIER_INCLUDED
