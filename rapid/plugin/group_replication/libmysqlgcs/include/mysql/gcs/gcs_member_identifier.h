/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_MEMBER_IDENTIFIER_INCLUDED
#define GCS_MEMBER_IDENTIFIER_INCLUDED

#include <string>
#include "gcs_types.h"


/*
  Internal GCS unique identifier.
*/
class Gcs_uuid
{
public:
  /*
    Create a GCS unique identifier.
  */

  static Gcs_uuid create_uuid();


  /*
    Default constructor
  */
  Gcs_uuid();


  /*
    Copies the internal buffer which is used to store a uuid to an
    external buffer. If the parameters buffer or size point to NULL,
    nothing is returned.

    @param [out] buffer storage buffer
    @param [out] size data size
    @return Whether the data was returned or not.
  */

  bool encode(uchar **buffer, unsigned int *size) const;


  /*
    Copies the external buffer to an internal buffer. If the
    parameter buffer points to NULL, nothing is returned.

    @param [in] buffer storage buffer
    @param [in] size data size
    @return Whether the data was copied or not.
  */

  bool decode(const uchar *buffer, const unsigned int size);

  /*
    Return the size of the UUID in use.
  */
  size_t size() const;

  /*
    Unique identifier which currently only accommodates 64 bits but
    can easily be extended to 128 bits and become a truly UUID in
    the future.
  */

  std::string actual_value;

private:
  /*
    Create a GCS unique identifier.
  */

  static const std::string do_create_uuid();
};


/**
  @class Gcs_member_identifier

  It represents the identity of a group member within a certain group.
  It can be the same or different within groups.

  One should avoid creating this representation from outside the binding,
  since each one might have its own internal representations.
  Instead one should use the Gcs_control_interface::get_local_information
  method to know our own identification within the group.
 */
class Gcs_member_identifier
{
public:
  /**
    Gcs_member_identifier constructor.

    @param[in] member_id the member identifier
  */

  explicit Gcs_member_identifier(const std::string &member_id);


  /**
    Gcs_member_identifier constructor.

    @param[in] member_id the member identifier
    @param[in] uuid the member uuid
  */

  explicit Gcs_member_identifier(const std::string &member_id,
                                 const Gcs_uuid &uuid);


  virtual ~Gcs_member_identifier() {}


  /**
    @return the member identifier
  */

  const std::string& get_member_id() const;

  /**
    @return the member uuid
  */

  const Gcs_uuid& get_member_uuid() const;


  /**
    Regenerate the member uuid
  */

  void regenerate_member_uuid();

  /**
    Redefinition of the operator less, to allow usage as key in maps.

    @param[in] other the Gcs_member_identifier to compare to

    @return the result of comparing internal representation with the
            compare operation from the string object
  */

  bool operator<(const Gcs_member_identifier& other) const;


  /**
    Redefinition of the operator equals, to allow usage in sets.

    @param[in] other the Gcs_member_identifier to compare to

    @return the result of comparing internal representation with the
            compare operation from the string object
  */

  bool operator==(const Gcs_member_identifier& other) const;


private:
  std::string m_member_id;
  Gcs_uuid m_uuid;
};

#endif // GCS_MEMBER_IDENTIFIER_INCLUDED
