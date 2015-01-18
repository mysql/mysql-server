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

#ifndef GCS_COROSYNC_INTERFACE_INCLUDED
#define	GCS_COROSYNC_INTERFACE_INCLUDED

/**
  This file is the main entry point for the gcs_interface implementation for the
 Corosync GCS (http://corosync.github.io/corosync/)
 */

#include "gcs_interface.h"
#include "gcs_corosync_communication_interface.h"
#include "gcs_corosync_control_interface.h"
#include "gcs_corosync_statistics_interface.h"
#include "gcs_state_exchange.h"

#include <map>
#include <string>
#include <poll.h>
#include <cstdlib>
#include <ctime>

#include <corosync/cpg.h>
#include <corosync/corotypes.h>

using std::map;
using std::string;

/**
 Struct that holds instances of this binding interface implementations
 */
typedef struct group_interfaces
{
  Gcs_control_interface* control_interface;
  Gcs_communication_interface* communication_interface;
  Gcs_statistics_interface* statistics_interface;

  /*
   Additional storage of group interface auxiliary structures for later
   deletion
  */

  Gcs_corosync_communication_proxy* comm_proxy;
  Gcs_corosync_view_change_control_interface *vce;
  Gcs_corosync_control_proxy* control_proxy;
  Gcs_corosync_state_exchange_interface *se;

} gcs_corosync_group_interfaces;

/**
 Implementation of the Gcs_interface for the Corosync binding
 */
class Gcs_corosync_interface: public Gcs_interface
{
  /**
   Since one want that a single instance exists, the interface implementation
   shall be retrieved via a Singleton pattern
   */
private:
  //Corosync single instance
  static Gcs_interface* interface_reference_singleton;

  /**
   Corosync binding private constructor
   */
  Gcs_corosync_interface();

public:
  /**
   Public method that allows the retrieving of the single instance

   @return a reference to the Singleton instance
   */
  static Gcs_interface* get_interface();

  /**
   Public method that finalizes and cleans the singleton
   */
  static void cleanup();

public:
  virtual ~Gcs_corosync_interface();

  /**
   This block implements the virtual methods defined in Gcs_interface
   */
  bool initialize();

  bool is_initialized();

  bool finalize();

  Gcs_control_interface*
    get_control_session(Gcs_group_identifier group_identifier);
  Gcs_communication_interface*
    get_communication_session(Gcs_group_identifier group_identifier);
  Gcs_statistics_interface*
    get_statistics(Gcs_group_identifier group_identifier);

private:
  /**
   Internal helper method that retrieves all group interfaces for a certain
   group.

   @note Since the group interfaces work as a singleton, meaning that a group
   has a single set of interfaces built, this method will also implement the
   behavior to build and initialize the interfaces implementation.

   @param[in] group_identifier the group in which one wants to instantiate the
          interface implementation.

   @return a reference to a struct gcs_corosync_group_interfaces
   */
  gcs_corosync_group_interfaces*
    get_group_interfaces(Gcs_group_identifier group_identifier);

  /**
    Contains all the code needed to initialize a connection to the Corosync
    deamon.

    @return true in case of error
   */
  bool initialize_corosync();

  /**
   Internal helper method to delete all previously created group interfaces
   */
  void clean_group_interfaces();

  //Holder to the created group interfaces, in which the key is the group
  map<string, gcs_corosync_group_interfaces*> group_interfaces;

  //Handle to the session open to Corosync
  cpg_handle_t handle;

  //Reference to the thread created and registered in Corosync
  pthread_t dispatcher_thd;

  //States if this interface is initialized
  bool initialized;
};

/**
  Method registered in the Corosync interface to receive view_change
  notifications
 */
void
view_change(cpg_handle_t handle, const struct cpg_name *name,
            const struct cpg_address *total, size_t total_entries,
            const struct cpg_address *left, size_t left_entries,
            const struct cpg_address *joined, size_t joined_entries);

/**
  Method registered in Corosync to receive messages
 */
void
deliver(cpg_handle_t handle, const struct cpg_name *name,
        uint32_t nodeid, uint32_t pid, void *data, size_t len);

//Synchronization structures for the dispatcher thread
extern pthread_cond_t dispatcher_cond;
extern pthread_mutex_t dispatcher_mutex;
extern bool is_dispatcher_inited;

/**
  Main thread dispatcher method
 */
void*
run_dispatcher(void *args);

#endif	/* GCS_COROSYNC_INTERFACE_INCLUDED */


