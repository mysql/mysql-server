/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file storage/perfschema/mysql_server_telemetry_traces_service_imp.cc
  The performance schema implementation of server telemetry traces service.
*/

#include "storage/perfschema/mysql_server_telemetry_traces_service_imp.h"

#include <mysql/components/services/mysql_server_telemetry_traces_service.h>
#include <list>
#include <string>

#include "sql/auth/sql_security_ctx.h"
#include "sql/field.h"
#include "sql/pfs_priv_util.h"
#include "sql/sql_class.h"  // THD

#include "pfs_global.h"

/* clang-format off */
/**

  @page PAGE_MYSQL_SERVER_TELEMETRY_TRACES_SERVICE Server telemetry traces service
  Performance Schema server telemetry traces service is a mechanism which provides
  plugins/components a way to get notifications related to SQL statements lifetime.

  @subpage TELEMETRY_TRACES_SERVICE_INTRODUCTION

  @subpage TELEMETRY_TRACES_SERVICE_INTERFACE

  @subpage TELEMETRY_TRACES_EXAMPLE_PLUGIN_COMPONENT


  @page TELEMETRY_TRACES_SERVICE_INTRODUCTION Service Introduction

  This service is named <i>mysql_server_telemetry_traces_v1</i> and it exposes three major
  methods:\n
  - @c register_telemetry   : plugin/component to register notification callbacks
  - @c unregister_telemetry : plugin/component to unregister notification callbacks
  - @c abort_telemetry : abort telemetry tracing for current statement within THD
    (on telemetry component uninstall)

  Register/unregister methods accept the pointer to a telemetry_v1_t structure that
  stores a collection of function pointers (callbacks), each callback called to notify
  of a different event type:
    - telemetry session created
    - telemetry session destroyed
    - new statement started
    - statement got query attributes attached
    - statement ended
    - statement telemetry aborted

  This set of callbacks allows the plugin/component to implement telemetry tracing of
  the statements being executed.

  @section TELEMETRY_TRACES_SERVICE_BLOCK_DIAGRAM Block Diagram
  Following diagram shows the block diagram of PFS services functionality, to
  register/unregister notification callbacks, exposed via mysql-server component.

@startuml

  actor client as "Plugin/component"
  box "Performance Schema Storage Engine" #LightBlue
  participant pfs_service as "mysql_server_telemetry_traces Service\n(mysql-server component)"
  endbox

  == Initialization ==
  client -> pfs_service :
  note right: Register notification callbacks \nPFS Service Call \n[register_telemetry()].

  == Cleanup ==
  client -> pfs_service :
  note right: Unregister notification callbacks \nPFS Service Call \n[unregister_telemetry()].

  @enduml

  @page TELEMETRY_TRACES_SERVICE_INTERFACE Service Interface

  This interface is provided to plugins/components, using which they can receive notifications
  related to statement lifetime events.
  Event notifications are obtained by registering a set of function pointers (callbacks).

  Each callback in a collection handles the single notification event:
   - m_tel_session_create \n
     Telemetry session has been started. Telemetry session concept contains a single THD session
     executing statements with telemetry component installed (telemetry active).
     Telemetry session will be destroyed when a client session (or an internal session like
     worker thread) ends or when we detect that the telemetry component itself has been
     uninstalled/reinstalled.
   - m_tel_session_destroy \n
     Telemetry session has been destroyed. Session callbacks can be used for bookkeeping
     the statements executing within the current session.
   - m_tel_stmt_start \n
     This callback is called when the new statement has started.
     Function returns pointer to opaque telemetry_locker_t structure, used by the component
     itself to store the data needed to trace/filter the respective statement.
     Returning nullptr from this function will cause the tracing of this statement to be
     aborted, i.e. subsequent notification handlers (m_tel_stmt_notify_qa, m_tel_stmt_end)
     will not be called for this statement.
     The component itself is responsible for disposing of the memory used to store data
     for this statement, before returning the nullptr (in order to stop tracing the statement).
   - m_tel_stmt_notify_qa \n
     This callback is called when the query attributes for the statement become available.
     This event is useful if the component that implements telemetry tracing uses filtering
     based on query attributes attached to each statement.
     Function returns pointer to opaque telemetry_locker_t structure, used by the component
     itself to store the data needed to trace/filter the respective statement.
     Returning nullptr from this function will cause the tracing of this statement to be aborted,
     i.e. subsequent notification handlers (m_tel_stmt_end)
     will not be called for this statement.
     The component itself is responsible for disposing of the memory used to store data for
     this statement, before returning the nullptr (in order to stop tracing the statement).
   - m_tel_stmt_end \n
     This callback is called when the statement has ended.
     At this point, the component that implements the telemetry tracing will need to decide
     if to emit the telemetry for this statement or not.
     The component itself is responsible for disposing of the memory used to store data for
     this statement, before exiting this callback.
   - m_tel_stmt_abort \n
     This callback is being called for a statement when we detect that the telemetry session
     has ended (such statement won't be emitted by the telemetry code).
     The component itself is responsible for disposing of the memory used to store data
     for this statement, before exiting this callback.

  Note that, at any given time, there can be only one user of this service.
  There is no support for multiple collections of telemetry callbacks
  being registered at the same time.

  @section TELEMETRY_TRACES_SERVICE_QUERY_FLOW_DIAGRAM Query Flow Diagram
  Following flow diagram shows the execution of query statement with the
  matching sequence of telemetry notification callback calls being triggered.

@startuml

  actor pfs as "PFS Implementation"
  participant client as "Plugin/component"

  == Callback sequence example ==

  == single statement executed ==
  pfs -> client   : Call "statement start" callback \n[m_tel_stmt_start()]
  client -> pfs   : Return result
  pfs -> client   : If result not nullptr \ncall "statement qa available" callback \n[m_tel_stmt_notify_qa()]
  client -> pfs   : Return result
  pfs -> client   : If result not nullptr \ncall "statement end" callback \n[m_tel_stmt_end()]

  == statement with sub-statement executed ==
  pfs -> client   : Call "statement start" callback \n[m_tel_stmt_start()]
  client -> pfs   : Return result
  pfs -> client   : If result not nullptr \ncall "statement qa available" callback \n[m_tel_stmt_notify_qa()]
  client -> pfs   : Return result
  pfs -> client   : Call "statement start" callback for sub-statement \n[m_tel_stmt_start()]
  client -> pfs   : Return result
  pfs -> client   : If result not nullptr \ncall "statement end" callback for sub-statement \n[m_tel_stmt_end()]
  pfs -> client   : If result not nullptr \ncall "statement end" callback for top-statement \n[m_tel_stmt_end()]

  @enduml

  @page TELEMETRY_TRACES_EXAMPLE_PLUGIN_COMPONENT  Example component

  Component/plugin that implements telemetry tracing, typically also uses other services
  within the callbacks to inspect and filter out the traced statements according to its needs.
  For example, you can skip tracing statements based on client user name, host or IP,
  schema name, query (digest) text and similar.
  As an example, see "components/test_server_telemetry_traces" test component source code,
  used to test this service.

*/
/* clang-format on */

BEGIN_SERVICE_IMPLEMENTATION(performance_schema,
                             mysql_server_telemetry_traces_v1)
impl_register_telemetry, impl_abort_telemetry, impl_unregister_telemetry,
    END_SERVICE_IMPLEMENTATION();

#ifdef HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE
bool server_telemetry_traces_service_initialized = false;
#endif /* HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE */

// currently registered collection of telemetry trace callbacks
PFS_ALIGNED PFS_cacheline_atomic_ptr<telemetry_t *> g_telemetry;

// locking for callback register/unregister
mysql_mutex_t LOCK_pfs_tracing_callback;
#ifdef HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE
static PSI_mutex_key key_LOCK_pfs_tracing_callback;
static PSI_mutex_info info_LOCK_pfs_tracing_callback = {
    &key_LOCK_pfs_tracing_callback, "LOCK_pfs_tracing_callback",
    PSI_VOLATILITY_PERMANENT, PSI_FLAG_SINGLETON,
    "This lock protects telemetry trace callback functions."};
#endif /* HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE */

void initialize_mysql_server_telemetry_traces_service() {
#ifdef HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE
  g_telemetry.m_ptr = nullptr;

  assert(!server_telemetry_traces_service_initialized);

  /* This is called once at startup */
  mysql_mutex_register("pfs", &info_LOCK_pfs_tracing_callback, 1);
  mysql_mutex_init(key_LOCK_pfs_tracing_callback, &LOCK_pfs_tracing_callback,
                   MY_MUTEX_INIT_FAST);
  server_telemetry_traces_service_initialized = true;
#endif /* HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE */
}

void cleanup_mysql_server_telemetry_traces_service() {
#ifdef HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE
  if (server_telemetry_traces_service_initialized) {
    mysql_mutex_destroy(&LOCK_pfs_tracing_callback);
    server_telemetry_traces_service_initialized = false;
  }
  g_telemetry.m_ptr = nullptr;
#endif /* HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE */
}

void server_telemetry_tracing_lock() {
#ifdef HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE
  mysql_mutex_lock(&LOCK_pfs_tracing_callback);
#endif /* HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE */
}

void server_telemetry_tracing_unlock() {
#ifdef HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE
  mysql_mutex_unlock(&LOCK_pfs_tracing_callback);
#endif /* HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE */
}

bool impl_register_telemetry(telemetry_t *telemetry [[maybe_unused]]) {
#ifdef HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE
  if (!server_telemetry_traces_service_initialized) return true;
  // allow overwriting existing callbacks to avoid possible time gap with no
  // telemetry available, if we would need to uninstall previous component using
  // this before installing new one
  mysql_mutex_lock(&LOCK_pfs_tracing_callback);
  g_telemetry.m_ptr = telemetry;
  mysql_mutex_unlock(&LOCK_pfs_tracing_callback);
  // Success
  return false;
#else
  // Failure
  return true;
#endif /* HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE */
}

bool impl_unregister_telemetry(telemetry_t *telemetry [[maybe_unused]]) {
#ifdef HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE
  if (!server_telemetry_traces_service_initialized) return true;
  mysql_mutex_lock(&LOCK_pfs_tracing_callback);
  if (g_telemetry.m_ptr == telemetry) {
    g_telemetry.m_ptr = nullptr;
    mysql_mutex_unlock(&LOCK_pfs_tracing_callback);
    // Success
    return false;
  }
  mysql_mutex_unlock(&LOCK_pfs_tracing_callback);
#endif /* HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE */
  // Failure
  return true;
}

void impl_abort_telemetry(THD *thd [[maybe_unused]]) {
  assert(thd != nullptr);

#ifdef HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE

#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_statement_locker *locker = thd->m_statement_psi;
  if (locker != nullptr) {
    PSI_STATEMENT_CALL(statement_abort_telemetry)(locker);
  }
#endif /* HAVE_PSI_STATEMENT_INTERFACE */

#ifdef HAVE_PSI_THREAD_INTERFACE
  PSI_thread *psi = thd->get_psi();
  if (psi != nullptr) {
    PSI_THREAD_CALL(abort_telemetry)(psi);
  }
#endif /* HAVE_PSI_THREAD_INTERFACE */

#endif /* HAVE_PSI_SERVER_TELEMETRY_TRACES_INTERFACE */
}
