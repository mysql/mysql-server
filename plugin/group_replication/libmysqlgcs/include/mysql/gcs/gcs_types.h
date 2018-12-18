/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_TYPES_INCLUDED
#define GCS_TYPES_INCLUDED

#include <map>
#include <string>
#include <vector>

/* Helper definitions for types used in this interface */
typedef unsigned char uchar;
typedef unsigned int uint32;

/**
  @enum enum_gcs_error

  This enumeration describes errors which can occur during group
  communication operations.
*/
enum enum_gcs_error {
  /* Gcs operation was successfully completed. */
  GCS_OK = 0,
  /* Error occurred while message communication. */
  GCS_NOK,
  /* Message was bigger then what gcs can successfully communicate/handle. */
  GCS_MESSAGE_TOO_BIG
};

/**
 @class Gcs_interface_parameters

 This class is to be used to provide parameters to bindings in a transparent
 and generic way.

 Each binding must document which parameters it needs and it is the
 responsibility of the client to provide them at initialize() time.
 */
class Gcs_interface_parameters {
 public:
  /**
    Adds a parameter to the parameter catalog.
    If the value already exists, it is overridden by the new one.

    @param name the name of the parameter
    @param value value of the parameter
  */

  void add_parameter(const std::string &name, const std::string &value);

  /**
    Retrieves a parameter from the object.

    @return a pointer to a registered value. NULL if not present
  */

  const std::string *get_parameter(const std::string &name) const;

  /**
    Check whether any of the paramters were provided.

    @param params list of parameters.
    @return True if any of the parameters is stored.
  */

  bool check_parameters(const std::vector<std::string> &params) const;

  /**
    Check whether any of the parameters were provided.

    @param params list of parameters.
    @param size number of parameters.
    @return True if any of the parameters is stored.
  */

  bool check_parameters(const char *params[], int size) const;

  /**
   Adds the parameters provided to the existing set of parameters.
   @param p Parameters to add.
  */

  void add_parameters_from(const Gcs_interface_parameters &p) {
    std::map<std::string, std::string>::const_iterator it;
    for (it = p.parameters.begin(); it != p.parameters.end(); it++) {
      std::string name = (*it).first;
      std::string val = (*it).second;
      add_parameter(name, val);
    }
  }

  /**
   Clears all parameters.
   */
  void clear() { parameters.clear(); }

  Gcs_interface_parameters() : parameters() {}

  virtual ~Gcs_interface_parameters() {}

 private:
  std::map<std::string, std::string> parameters;
};

/**
 The GCS protocol versions.
 */
enum class Gcs_protocol_version : unsigned short {
  /* Unknown version that shall not be used. */
  UNKNOWN = 0,
  V1 = 1,
  V2 = 2,
  /* Define the highest known version. */
  HIGHEST_KNOWN = V2,
  /* Currently used in test cases. */
  V3 = 3,
  V4 = 4,
  V5 = 5,
  /*
   No valid version number can appear after this one. If a version number is to
   be added, this value needs to be incremented and the lowest version number
   available be assigned to the new version.

   Remember also to set HIGHEST_KNOWN to the newly created version number.
  */
  MAXIMUM = 5
};

#endif /* GCS_TYPES_INCLUDED */
