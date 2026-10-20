/* Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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

#include "health_monitor_collector.h"

#include <chrono>
#include <cmath>    // round
#include <iomanip>  // setprecision
#include <sstream>

#include "my_systime.h"
#include "my_thread.h"

#include "health_monitor.h"  // log
#include "health_monitor_log.h"
#include "health_monitor_system_vars.h"

namespace hm {

Collector::Collector(const char *name, const char *resource_name,
                     const char *instr_name, const char *os_name,
                     const char *description)
    : Thread(name, instr_name, os_name, description),
      m_resource_name{resource_name} {
  m_base_interval = m_sample_interval;
}
/**
  Sample collection loop.
*/
void Collector::run_process() {
  // Initialize the thread state
  pre_process();

  mysql_mutex_lock(&m_wait_lock);
  bool done{m_stop};  // set by start_thread() and stop_thread()
  mysql_mutex_unlock(&m_wait_lock);

  bool timeout{true};  // true after a wait timeout
  timespec wait{0, 0};
  // Initialize sample and status message timers
  initialize_sample_timers();

  while (!done) {
    if (timeout) {
      // Read resource info, store if sample timer expired
      process();
      // Advance sample and status timers
      advance_sample_timers();
      timeout = false;
    }
    const uint64_t wait_seconds{m_base_interval * hundredth_ns};
    wait.tv_sec = (my_getsystime() + wait_seconds) / hundredth_ns;

    mysql_mutex_lock(&m_wait_lock);
    done = m_stop;  // has a stop been requested?
    if (!done) {
      const auto result = mysql_cond_timedwait(&m_wait, &m_wait_lock, &wait);
      // Collect after a timeout but not after a signal
      timeout = is_timeout(result);
      done = m_stop;
    }
    mysql_mutex_unlock(&m_wait_lock);
  }

  // Wrap remaining tasks before exiting the thread
  post_process();
}

/**
  Initialize collection timers.
*/
void Collector::initialize_sample_timers() {
  // Sample timer is the countdown to the next sample collection
  m_sample_timer.set_name("Sample_Timer");
  m_sample_timer.set_interval(m_base_interval);
  m_sample_timer.set_timeout(m_sample_interval);
  // Status message timer is the countdown to the next status message
  m_status_timer.set_name("Status_Timer");
  m_status_timer.set_interval(m_base_interval);
  m_status_timer.set_timeout(m_status_interval * m_sample_interval);
}

/**
  Process collector sample and status timers.
*/
void Collector::advance_sample_timers() {
  if (m_sample_timer.expired()) {
    m_sample_timer.reset();
  }
  m_sample_timer.tick(m_base_interval);

  if (m_status_timer.expired()) {
    m_status_timer.reset();
  }
  m_status_timer.tick(m_base_interval);
}

/**
  Mark a settings update for evaluation by the Supervisor.

  Do not inspect collector settings or acquire the command lock in the system
  variable callback. The Supervisor evaluates the current values while it
  holds the command lock.
*/
void Collector::mark_settings_pending() { set_settings_pending(true); }

/**
  Update system variable settings for the Collector.
  - Static system variables require the thread to be restarted.
  - Dynamic system variables can be applied without restarting the thread.
  - Collectors that do not distinguish between static and dynamic settings use
    settings_changed() to detect changes and therefore require a thread restart.
  - If no variables have changed, no action is taken.

  @return true when the pending notification was consumed
 */
bool Collector::update_settings() {
  auto locked = std::unique_lock<std::mutex>(m_command_lock);
  apply_pending_settings();
  return true;
}

/**
  Apply settings while the collector command lock is held.
*/
void Collector::apply_pending_settings() {
  consume_settings_pending();
  // If static settings have changed, restart the thread.
  if (settings_changed() || static_settings_changed()) {
    // A concurrent SET can change the run state while stop_thread() waits for
    // the worker to exit.
    stop_thread();
    set_params();
    // Use the run state applied by set_params(). A later SET will remain
    // pending for the next pass.
    const bool next_run_state = m_running;
    initialize_sample_timers();
    log_collector_status(std::string("Settings"), true);
    if (next_run_state) {
      start_thread();
    }
  }
  // If only dynamic settings changed, update live, no restart needed.
  else if (dynamic_settings_changed()) {
    set_params();
    log_collector_status(std::string("Settings (live)"), true);
  }
}

/**
  Consume one pending notification without clearing a concurrent update.
*/
void Collector::consume_settings_pending() {
  // Consume this notification before reading settings. A concurrent callback
  // after this exchange remains pending for the next Supervisor pass.
  m_settings_pending.exchange(false, std::memory_order_acq_rel);
}

/**
  Set the base monitoring interval if different than the sample interval.
  Adjust the sample interval to the nearest multiple of the base interval.
  If the base interval is not set, then the sample interval is used as the
  base interval.
*/
void Collector::set_base_interval(int base_interval) {
  int const orig_base_interval{base_interval};
  if (base_interval < k_min_interval || k_max_interval < base_interval) {
    base_interval = k_default_base_interval;
    log_collector_status("Base interval adjusted from " +
                             std::to_string(orig_base_interval) + " to " +
                             std::to_string(base_interval),
                         false);
  }
  m_base_interval = base_interval;
  m_custom_base_interval = true;
}

/**
  Set the sample interval, adjust to the nearest multiple of the base interval.
*/
void Collector::set_sample_interval(int sample_interval) {
  assert(m_base_interval > 0);
  // Unless set separately, the base interval is the sample interval
  if (!m_custom_base_interval) {
    m_base_interval = sample_interval;
    m_sample_interval = sample_interval;
    return;
  }
  // The base interval was set separately, so adjust the sample interval
  int const orig_sample_interval{sample_interval};
  if (sample_interval < m_base_interval) {
    sample_interval = m_base_interval;
  } else if (sample_interval % m_base_interval != 0) {
    sample_interval =
        std::round(static_cast<float>(sample_interval) / m_base_interval) *
        m_base_interval;
  }
  if (sample_interval != orig_sample_interval) {
    log_collector_status("Sample interval adjusted from " +
                             std::to_string(orig_sample_interval) + " to " +
                             std::to_string(sample_interval) + " seconds",
                         false);
  }
  m_sample_interval = sample_interval;
}

/**
  Set the base and sample intervals together to ensure that the sample interval
  is a multiple of the base interval.
*/
void Collector::set_collector_intervals(int base_interval,
                                        int sample_interval) {
  set_base_interval(base_interval);
  set_sample_interval(sample_interval);
}

/**
  Collector status with optional settings.
*/
void Collector::log_collector_status(bool settings) {
  log_collector_status("", settings);
}

/**
  Collector status with optional message and optional settings.
*/
void Collector::log_collector_status(const std::string &state_msg,
                                     bool settings) {
  const std::string state{state_msg.empty() ? tag_collector_state()
                                            : state_msg};
  log_info("%s", build_collector_message(state, "", "", settings).c_str());
}

/**
  Issue a log status message for the collector.

  - Status messages are used by the collector state machines to report
    storage level changes.
  - The status timer advances every collection cycle and expires after
    status_interval * sample_interval seconds to limit repeated messages.
  - Status messages can be forced with m_force_status = true.
*/
void Collector::log_status() {
  if (status_message_timer().expired() || m_force_status) {
    log_resource_status();
    m_force_status = false;
  }
}
/**
  Log general resource status with state, mode (Fallback, Recovery, ...) and
  levels.
*/
void Collector::log_resource_levels() {
  log_info("%s",
           build_resource_message(tag_resource_state(), tag_mode(), "", true)
               .c_str());
}

/**
  Formatted collector message with optional settings.

  @param status   Collector status such as Started, Stopped, etc.
  @param mode     Mode name, if any, such as Fallback, Recovery, etc.
  @param message  Message text, if any
  @param settings If true then append message with collector system variables
  @return         Message string assembled from all parameters, prefixed with
                  the collector name such as Disk Collector, Memory Collector.
*/
std::string Collector::build_collector_message(const std::string &status,
                                               const std::string &mode,
                                               const std::string &message,
                                               bool settings) {
  std::stringstream msg;
  msg << name();          // collector name
  msg << ": " << status;  // required
  if (!mode.empty()) {
    msg << ": " << mode;
  }
  if (!message.empty()) {
    msg << ": " << message;
  }
  if (settings) {
    msg << ": " << format_settings();
  }
  return msg.str();
}

/**
  Formatted resource message, no levels or settings.

  @param status   Resource status, if any, such as Warning Level, Normal Level
  @param mode     Mode name, if any, such as Fallback, Recovery
  @param message  Message text, if any
  @param levels   If true then append message with resource levels
  @return         Message string assembled from non-empty parameters, prefixed
                  with the resource name, such as Disk, Memory, System
*/
std::string Collector::build_resource_message(const std::string &status,
                                              const std::string &mode,
                                              const std::string &message,
                                              bool levels) {
  std::stringstream msg;
  msg << resource_name();
  if (!status.empty()) {
    msg << ": " << status;
  }
  if (!mode.empty()) {
    msg << ": " << mode;
  }
  if (!message.empty()) {
    msg << ": " << message;
  }
  if (levels) {
    msg << ": " << format_status_info();
  }
  return msg.str();
}

/**
  Format storage amount in MiB, no units.
  @param[in] mbytes  Storage amount in megabytes
  @return Formatted string
*/
std::string Collector::format_level(uint64_t mbytes) {
  return format_mb(mbytes, -1, true);
}
/**
  Format storage amount.
  @param[in] mbytes      Storage amount in megabytes
  @param[in] prec        Digits after the decimal, -1 for variable wrt units
  @param[in] show_units  True to append units
  @return Formatted string
*/
std::string Collector::format_mb(uint64_t mbytes, int prec, bool show_units) {
  std::string units;
  std::stringstream amount;
  uint64_t divisor{1};
  constexpr bool long_units{false};
  int var_prec{0};
  int precision{prec};

  if (mbytes >= eib) {
    divisor = eib;
    units = "EiB";
    var_prec = 3;
  } else if (mbytes >= pib) {
    divisor = pib;
    units = "PiB";
    var_prec = 3;
  } else if (mbytes >= tib) {
    divisor = tib;
    units = "TiB";
    var_prec = 2;
  } else if (mbytes >= gib) {
    divisor = gib;
    units = "GiB";
    var_prec = 1;
  } else {
    divisor = mib;
    units = "MiB";
    var_prec = 0;
  }

  if (prec < 0) {
    precision = var_prec;
  }

  if (divisor == mib) {
    amount << mbytes;
  } else {
    double xib = mbytes;
    xib /= divisor;
    // xib = std::round(xib);
    amount << std::setprecision(precision) << std::fixed << xib;
  }

  if (show_units) {
    if (long_units) {  // future use
      amount << " " << units;
    } else {
      amount << units.at(0);
    }
  }

  return amount.str();
}

}  // namespace hm
