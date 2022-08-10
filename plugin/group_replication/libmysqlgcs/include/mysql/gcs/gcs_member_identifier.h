/* Copyright (c) 2014, 2022, Oracle and/or its affiliates.

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

#ifndef GCS_MEMBER_IDENTIFIER_INCLUDED
#define GCS_MEMBER_IDENTIFIER_INCLUDED

#include <string>

/**
  @class Gcs_member_identifier

  It represents the identity of a group member within a certain group.
  It can be the same or different within groups.

  One should avoid creating this representation from outside the binding,
  since each one might have its own internal representations.
  Instead one should use the Gcs_control_interface::get_local_information
  method to know our own identification within the group.
 */
class Gcs_member_identifier {
 public:
  /**
    Gcs_member_identifier constructor.

    @param[in] member_id the member identifier
  */

  explicit Gcs_member_identifier(const std::string &member_id);

  Gcs_member_identifier() = default;
  Gcs_member_identifier(const Gcs_member_identifier &) = default;
  Gcs_member_identifier(Gcs_member_identifier &&) = default;
  Gcs_member_identifier &operator=(const Gcs_member_identifier &) = default;
  Gcs_member_identifier &operator=(Gcs_member_identifier &&) = default;

  virtual ~Gcs_member_identifier() = default;

  /**
    @return the member identifier
  */

  const std::string &get_member_id() const;

  /**
    Redefinition of the operator less, to allow usage as key in maps.

    @param[in] other the Gcs_member_identifier to compare to

    @return the result of comparing internal representation with the
            compare operation from the string object
  */

  bool operator<(const Gcs_member_identifier &other) const;

  /**
    Redefinition of the operator equals, to allow usage in sets.

    @param[in] other the Gcs_member_identifier to compare to

    @return the result of comparing internal representation with the
            compare operation from the string object
  */

  bool operator==(const Gcs_member_identifier &other) const;

 private:
  std::string m_member_id;
};
#endif  // GCS_MEMBER_IDENTIFIER_INCLUDED
