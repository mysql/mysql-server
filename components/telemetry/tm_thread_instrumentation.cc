/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

// For HAVE_SETNS
#include "my_config.h"

#include "tm_ns.h"
#include "tm_required_services.h"
#include "tm_thread_instrumentation.h"

/*
  For debug only, do not use in production.
*/
// #define DEBUG_INSTRUMENTATION

namespace telemetry {

struct MySQLThreadInstrumentationState {
 public:
  PSI_idle_locker *m_idle_psi{nullptr};
  PSI_idle_locker_state m_idle_state;
};

thread_local MySQLThreadInstrumentationState *TLS_state;

MySQLThreadInstrumentation::MySQLThreadInstrumentation(
    PSI_thread_key thread_key, int network_namespace_fd)
    : m_thread_key(thread_key), m_network_namespace_fd(network_namespace_fd) {}

void MySQLThreadInstrumentation::OnStart() {
#ifdef DEBUG_INSTRUMENTATION
  (void)fprintf(stderr,
                "MySQLThreadInstrumentation::OnStart(key = %d, fd = %d)\n",
                m_thread_key, m_network_namespace_fd);
#endif

  /* Instrument this OTEL thread in the performance schema. */

  PSI_thread *psi;
  psi = thread_srv->new_thread(m_thread_key, 0, this, 0);
  thread_srv->set_thread_os_id(psi);
  thread_srv->set_thread(psi);

  /* Set the network namespace */

#ifdef HAVE_SETNS
  if (m_network_namespace_fd != NO_FD) {
    set_network_namespace(m_network_namespace_fd);
  }
#endif /* HAVE_SETNS */

  /* Create per thread instance state. */

  assert(TLS_state == nullptr);
  TLS_state = new MySQLThreadInstrumentationState();
}

void MySQLThreadInstrumentation::OnEnd() {
#ifdef DEBUG_INSTRUMENTATION
  (void)fprintf(stderr, "MySQLThreadInstrumentation::OnEnd(key = %d)\n",
                m_thread_key);
#endif

  /* Destroy per thread instance state. */

  assert(TLS_state != nullptr);
  assert(TLS_state->m_idle_psi == nullptr);
  delete TLS_state;
  TLS_state = nullptr;

  /* Destroy the performance schema instrumentation. */

  thread_srv->delete_current_thread();
}

void MySQLThreadInstrumentation::BeforeWait() {
#ifdef DEBUG_INSTRUMENTATION
  (void)fprintf(stderr, "MySQLThreadInstrumentation::BeforeWait(key = %d)\n",
                m_thread_key);
#endif

  MySQLThreadInstrumentationState *state = TLS_state;
  if (state != nullptr) {
    assert(state->m_idle_psi == nullptr);
    state->m_idle_psi =
        idle_srv->start_idle_wait(&state->m_idle_state, __FILE__, __LINE__);
  }
}

void MySQLThreadInstrumentation::AfterWait() {
#ifdef DEBUG_INSTRUMENTATION
  (void)fprintf(stderr, "MySQLThreadInstrumentation::AfterWait(key = %d)\n",
                m_thread_key);
#endif

  MySQLThreadInstrumentationState *state = TLS_state;
  if (state != nullptr) {
    if (state->m_idle_psi != nullptr) {
      idle_srv->end_idle_wait(state->m_idle_psi);
      state->m_idle_psi = nullptr;
    }
  }
}

void MySQLThreadInstrumentation::BeforeLoad() {
#ifdef DEBUG_INSTRUMENTATION
  (void)fprintf(stderr, "MySQLThreadInstrumentation::BeforeLoad(key = %d)\n",
                m_thread_key);
#endif
}

void MySQLThreadInstrumentation::AfterLoad() {
#ifdef DEBUG_INSTRUMENTATION
  (void)fprintf(stderr, "MySQLThreadInstrumentation::AfterLoad(key = %d)\n",
                m_thread_key);
#endif
}

}  // namespace telemetry
