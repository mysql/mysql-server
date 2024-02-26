/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#include "storage/perfschema/pfs_tls_channel.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/psi/mysql_rwlock.h"

/* clang-format off */

/**
  @page PAGE_PFS_TLS_CHANNEL Performance schema TLS channels instrumentation

  @section TLS_CHANNEL_REGISTRATION Registration
  @startuml
    title Registration

    participant server as "MySQL server"
    participant pfs as "Performance schema"
    participant component as "Component or Plugin"

    == Bootstrap ==

    server -> pfs : mysql_tls_channel_register(<main channel>)
    server -> pfs : mysql_tls_channel_register(<admin channel>)

    == INSTALL COMPONENT/SERVER ==

    server -> component : init()
    component -> pfs : mysql_tls_channel_register(<channel>)

    == SELECT * FROM PERFORMANCE_SCHEMA.tls_channel_status ==
    
    server -> pfs : select
    pfs -> server : (Fetch TLS configuration details)
    pfs -> component : (Fetch TLS configuration details)
    pfs --> server : result set row 1
    pfs --> server : result set row 2
    pfs --> server : ...
    pfs --> server : result set row N

    == UNINSTALL COMPONENT/PLUGIN ==

    server -> component : deinit()
    component -> pfs : mysql_tls_channel_unregister(<channel>)

    == Shutdown ==

    server -> pfs : mysql_tls_channel_unregister(<admin channel>)
    server -> pfs : mysql_tls_channel_unregister(<main channel>)

  @enduml

    To expose TLS configuration details through performance schema:
  - Implement a subclass of TLS_channel_property_iterator
  - Register it with Performance schema on init/bootstrap
  - Unregister instrumentation with Performance schema on deinit/shutdown

  @section TLS_CHANNEL_SCAN Iteration for each TLS channel
  @startuml
    title Iteration for each TLS channel

    participant server as "MySQL server"
    participant pfs as "Performance schema\nTable_tls_channel_status"
    participant pfs_container as "Performance schema\nTLS channel status container"
    participant tls_channel_iterator_1 as "TLS channel status iterator"
    participant tls_channel_iterator_2 as "Another TLS channel status iterator"

    == SELECT init ==

    server -> pfs : rnd_init()
    pfs -> pfs_container : create()
    activate pfs_container

    == For each TLS channel ==

    pfs -> tls_channel_iterator_1 : create_iterator()
    activate tls_channel_iterator_1
    pfs -> tls_channel_iterator_1 : (multiple calls)
    pfs -> tls_channel_iterator_1 : destroy_iterator()
    deactivate tls_channel_iterator_1
    pfs -> tls_channel_iterator_2 : create_iterator()
    activate tls_channel_iterator_2
    pfs -> tls_channel_iterator_2 : (multiple calls)
    pfs -> tls_channel_iterator_2 : destroy_iterator()
    deactivate tls_channel_iterator_2

    == SELECT end ==

    server -> pfs : rnd_end()
    pfs -> pfs_container : destroy()
    deactivate pfs_container

  @enduml

  When the server performs a SELECT * from performance_schema.tls_channeL_info, the performance schema creates
  a TLS_channel_status_container for the duration of the table scan.

  Then, the scan loops for each TLS channel capable of exposing status (i.e. channels that registered
  TLS channel status iterator).

  For each channel, an iterator is created, dedicated for this SELECT scan.

  @section TLS_CHANNEL_INTERNAL_SCAN Inside a registered TLS channel
  @startuml
    title Inside a registered TLS channel

    participant server as "MySQL server"
    participant pfs as "Performance schema\nTable_tls_channel_status"
    participant pfs_container as "Performance schema\nTLS channel status container"
    participant tls_channel_iterator as "TLS channel status iterator"
    participant tls_channel_container as "Local TLS channel status container"
    participant tls_channel as "TLS channel"

    == SELECT init ==

    server -> pfs : rand_init()
    pfs -> pfs_container : create()
    activate pfs_container

    == Materialize ==

    loop until all registered TLS channels are processed
    pfs -> tls_channel_iterator : create_iterator()
    activate tls_channel_iterator
    tls_channel_iterator -> tls_channel_container : create()
    activate tls_channel_container
    tls_channel_container -> tls_channel : start_fetch_status()
    activate tls_channel
    tls_channel_container -> tls_channel : fetch_status()
    tls_channel_container -> tls_channel : ...
    tls_channel_container -> tls_channel : end_fetch_status()
    deactivate tls_channel
    pfs -> tls_channel_iterator : get()
    tls_channel_iterator -> tls_channel_container : fetch_next_status()
    pfs -> tls_channel_iterator : next()
    tls_channel_iterator -> tls_channel_container : move_to_next_status()
    pfs -> tls_channel_iterator : ...
    pfs -> tls_channel_iterator : ...
    pfs -> tls_channel_iterator : destroy_iterator()
    tls_channel_iterator -> tls_channel_container : destroy_container()
    deactivate tls_channel_container
    deactivate tls_channel_iterator

    == Subsequent scans ==

    server -> pfs : rnd_next()
    pfs -> pfs_container : get_row(2)
    pfs --> server : result set row 2
    server -> pfs : rnd_next()
    pfs -> pfs_container : get_row(...)
    pfs --> server : result set row ...
    server -> pfs : rnd_next()
    pfs -> pfs_container : get_row(N)
    pfs --> server : result set row N

    == SELECT end ==

    server -> pfs : rnd_end()
    pfs -> pfs_container : destroy()
    deactivate pfs_container

  @enduml

  When rnd_init() is called, the performance schema iterates through registered TLS channels
  and add data corresponding to each channel in TLS channel data container.

  Upon subsequent calls ::rnd_next(), data present in the container is returned. This process
  continues until no more data is left in the container.

  This method allows performance schema to shorten the duration for which registered TLS
  channel container is locked. This is essential because mysql_tls_channel_register() and
  mysql_tls_channel_deregister() may modify the TLS channel container.

*/

/* clang-format on */

tls_channels g_instrumented_tls_channels;
static bool g_instrumented_tls_channels_inited = false;

/**
  RW lock that protects list of instrumented TLS channels.
  @sa g_instrumented_tls_channels
*/
mysql_rwlock_t LOCK_pfs_tls_channels;
static PSI_rwlock_key key_LOCK_pfs_tls_channels;
static PSI_rwlock_info info_LOCK_pfs_tls_channels = {
    &key_LOCK_pfs_tls_channels, "LOCK_pfs_tls_channels", PSI_FLAG_SINGLETON, 0,
    "This lock protects list of instrumented TLS channels."};

void init_pfs_tls_channels_instrumentation() {
  /* This is called once at startup */
  mysql_rwlock_register("pfs", &info_LOCK_pfs_tls_channels, 1);
  mysql_rwlock_init(key_LOCK_pfs_tls_channels, &LOCK_pfs_tls_channels);
  g_instrumented_tls_channels_inited = true;
}

void cleanup_pfs_tls_channels_instrumentation() {
  g_instrumented_tls_channels_inited = false;
  mysql_rwlock_destroy(&LOCK_pfs_tls_channels);
}

void pfs_register_tls_channel_v1(TLS_channel_property_iterator *provider) {
  if (!g_instrumented_tls_channels_inited) return;
  bool insert = true;
  mysql_rwlock_wrlock(&LOCK_pfs_tls_channels);
  for (auto *channel : g_instrumented_tls_channels) {
    if (channel == provider) {
      insert = false;
      break;
    }
  }
  if (insert) g_instrumented_tls_channels.push_back(provider);
  mysql_rwlock_unlock(&LOCK_pfs_tls_channels);
}

void pfs_unregister_tls_channel_v1(TLS_channel_property_iterator *provider) {
  if (!g_instrumented_tls_channels_inited) return;
  if (g_instrumented_tls_channels.empty()) return;
  mysql_rwlock_wrlock(&LOCK_pfs_tls_channels);
  for (auto it = g_instrumented_tls_channels.cbegin();
       it != g_instrumented_tls_channels.cend(); ++it) {
    if (*it == provider) {
      g_instrumented_tls_channels.erase(it);
      break;
    }
  }
  mysql_rwlock_unlock(&LOCK_pfs_tls_channels);
}

void pfs_tls_channels_lock_for_read() {
  mysql_rwlock_rdlock(&LOCK_pfs_tls_channels);
}

void pfs_tls_channels_unlock() { mysql_rwlock_unlock(&LOCK_pfs_tls_channels); }

tls_channels &pfs_get_instrumented_tls_channels() {
  return g_instrumented_tls_channels;
}
