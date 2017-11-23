/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

/**
  @addtogroup Backup
  @{

  @file

  @brief Instance log resource definitions. This includes code for the server
         resources that will take part on the results of the
         performance_schema.instance_log_status table.
*/

#ifndef INSTANCE_LOG_RESOURCE_H
#define INSTANCE_LOG_RESOURCE_H

#include "sql/json_dom.h"


/**
  @class Instance_log_resource

  This is the base class that the logic of collecting a MySQL server instance
  resources log will call.

  It basically contains lock, unlock and collect functions that shall be
  overridden by more specialized classes to handle the specific cases of
  resources participating in the process.
*/

class Instance_log_resource
{
  /* JSON object to be filled by the do_collect_info function */
  Json_dom *json= nullptr;

public:
  /**
    There must be one function of this kind in order for the symbols in the
    server's dynamic library to be visible to plugins.
  */
  static int dummy_function_to_ensure_we_are_linked_into_the_server();


  /**
    Instance_log_resource constructor.

    @param[in] info the pointer to the JSON object to be populated with the
                    resource log information.
  */
  Instance_log_resource(Json_dom* json_arg)
    : json(json_arg)
  {
  };


  /**
    Destructor.

    This function will point the JSON object to nullptr.
  */

  virtual ~Instance_log_resource()
  {
    json= nullptr;
  };


  /**
    Return the pointer to the JSON object that should be used to fill the
    resource log information.

    @return  the Json_object pointer to be filled with the log information.
  */

  Json_dom *get_json()
  {
    return json;
  };


  /*
    Next three virtual functions need to be overridden by any more specialized
    class inheriting from this to support a specific resource participating in
    the collection process.
  */

  /**
    Lock the resource avoiding updates.
  */

  virtual void lock()
  {
  };


  /**
    Unlock the resource allowing updates.
  */

  virtual void unlock()
  {
  };


  /**
    Collect resource log information.

    @return  false on success.
             true if there was an error collecting the information.
  */

  virtual bool collect_info()
  {
    return false;
  };
};

/**
  @} (End of group Backup)
*/

#endif /* INSTANCE_LOG_RESOURCE_H */
