/* Copyright (c) 2014, 2016 Oracle and/or its affiliates. All rights reserved.

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


#ifndef GCS_TYPES_INCLUDED
#define GCS_TYPES_INCLUDED

#include <map>
#include <string>
#include <vector>

/* Helper definitions for types used in this interface */
typedef unsigned char uchar;
typedef unsigned int  uint32;

/**
  @enum enum_gcs_error

  This enumeration describes errors which can occur during group
  communication operations.
*/
enum enum_gcs_error
{
  /* Gcs operation was successfully completed. */
  GCS_OK= 0,
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
class Gcs_interface_parameters
{
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
    Check whether any of the paramters were provided.

    @param params list of parameters.
    @return True if any of the parameters is stored.
  */

  bool check_parameters(const char* params[], int size) const;


  /**
   Adds the parameters provided to the existing set of parameters.
   @param p Parameters to add.
  */

  void add_parameters_from(const Gcs_interface_parameters& p)
  {
    std::map<std::string,std::string>::const_iterator it;
    for (it= p.parameters.begin(); it != p.parameters.end(); it++)
    {
      std::string name= (*it).first;
      std::string val= (*it).second;
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

#endif  /* GCS_TYPES_INCLUDED */

