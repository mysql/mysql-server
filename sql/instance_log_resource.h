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

#include "sql/binlog.h"
#include "sql/handler.h"
#include "sql/json_dom.h"
#include "sql/rpl_gtid.h"
#include "sql/rpl_mi.h"


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

    @param[in] json_arg the pointer to the JSON object to be populated with the
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
  @class Instance_log_resource_mi_wrapper

  This is the Instance_log_resource to handle Master_info resources.
*/
class Instance_log_resource_mi_wrapper: public Instance_log_resource
{
  Master_info *mi= nullptr;

public:
  /**
    Instance_log_resource_mi_wrapper constructor.

    @param[in] mi_arg the pointer to the Master_info object resource.
    @param[in] json_arg the pointer to the JSON object to be populated with the
                        resource log information.
  */

  Instance_log_resource_mi_wrapper(Master_info *mi_arg, Json_dom *json_arg)
    : Instance_log_resource(json_arg),
      mi(mi_arg)
  {
  };


  void lock() override;
  void unlock() override;
  bool collect_info() override;
};


/**
  @class Instance_log_resource_binlog_wrapper

  This is the Instance_log_resource to handle MYSQL_BIN_LOG resources.
*/
class Instance_log_resource_binlog_wrapper: public Instance_log_resource
{
  MYSQL_BIN_LOG *binlog= nullptr;

public:
  /**
    Instance_log_resource_binlog_wrapper constructor.

    @param[in] binlog_arg the pointer to the MYSQL_BIN_LOG object resource.
    @param[in] json_arg the pointer to the JSON object to be populated with the
                        resource log information.
  */

  Instance_log_resource_binlog_wrapper(MYSQL_BIN_LOG *binlog_arg,
                                       Json_dom *json_arg)
    : Instance_log_resource(json_arg),
      binlog(binlog_arg)
  {
  };


  void lock() override;
  void unlock() override;
  bool collect_info() override;
};


/**
  @class Instance_log_resource_gtid_state_wrapper

  This is the Instance_log_resource to handle Gtid_state resources.
*/
class Instance_log_resource_gtid_state_wrapper: public Instance_log_resource
{
  Gtid_state *gtid_state= nullptr;

public:
  /**
    Instance_log_resource_gtid_state_wrapper constructor.

    @param[in] gtid_state_arg the pointer to the Gtid_state object resource.
    @param[in] json_arg the pointer to the JSON object to be populated with the
                        resource log information.
  */

  Instance_log_resource_gtid_state_wrapper(Gtid_state *gtid_state_arg,
                                           Json_dom *json_arg)
    : Instance_log_resource(json_arg),
      gtid_state(gtid_state_arg)
  {
  };


  void lock() override;
  void unlock() override;
  bool collect_info() override;
};


/**
  @class Instance_log_resource_hton_wrapper

  This is the Instance_log_resource to handle handlerton resources.
*/
class Instance_log_resource_hton_wrapper: public Instance_log_resource
{
  handlerton *hton= nullptr;

public:
  /**
    Instance_log_resource_hton_wrapper constructor.

    @param[in] hton_arg the pointer to the hton resource.
    @param[in] json_arg the pointer to the JSON object to be populated with the
                        resource log information.
  */

  Instance_log_resource_hton_wrapper(handlerton *hton_arg,
                                     Json_dom *json_arg)
    : Instance_log_resource(json_arg),
      hton(hton_arg)
  {
  };


  void lock() override;
  void unlock() override;
  bool collect_info() override;

};


/**
  @class Instance_log_resource_factory

  This is the Instance_log_resource factory to create wrappers for supported
  resources.
*/
class Instance_log_resource_factory
{
public:
  /**
    Creates a Instance_log_resource wrapper based on a Master_info object.

    @param[in] mi the pointer to the Master_info object resource.
    @param[in] json the pointer to the JSON object to be populated with the
                    resource log information.
    @return  the pointer to the new Instance_log_resource.
  */

  static Instance_log_resource *get_wrapper(Master_info *mi, Json_dom *json);


  /**
    Creates a Instance_log_resource wrapper based on a Master_info object.

    @param[in] binog the pointer to the MYSQL_BIN_LOG object resource.
    @param[in] json the pointer to the JSON object to be populated with the
                    resource log information.
    @return  the pointer to the new Instance_log_resource.
  */

  static Instance_log_resource *get_wrapper(MYSQL_BIN_LOG *binlog,
                                            Json_dom *json);


  /**
    Creates a Instance_log_resource wrapper based on a Gtid_state object.

    @param[in] gtid_state the pointer to the Gtid_state object resource.
    @param[in] json the pointer to the JSON object to be populated with the
                    resource log information.
    @return  the pointer to the new Instance_log_resource.
  */

  static Instance_log_resource *get_wrapper(Gtid_state *gtid_state,
                                            Json_dom *json);


  /**
    Creates a Instance_log_resource wrapper based on a handlerton.

    @param[in] hton the pointer to the handlerton resource.
    @param[in] json the pointer to the JSON object to be populated with the
                    resource log information.
    @return  the pointer to the new Instance_log_resource.
  */

  static Instance_log_resource *get_wrapper(handlerton *hton,
                                            Json_dom *json);
};

/**
  @} (End of group Backup)
*/

#endif /* INSTANCE_LOG_RESOURCE_H */
